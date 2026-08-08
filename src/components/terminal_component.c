#include "terminal_component.h"
#include "../render.h"
#include "../backend.h"
#include "../event.h"
#include "../util.h"
#include "../layer_update.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* XKB keysym constants (from xkbcommon-keysyms.h) */
#define XKB_KEY_BackSpace  0xff08
#define XKB_KEY_Return     0xff0d
#define XKB_KEY_Home       0xff50
#define XKB_KEY_Left       0xff51
#define XKB_KEY_Up         0xff52
#define XKB_KEY_Right      0xff53
#define XKB_KEY_Down       0xff54
#define XKB_KEY_End        0xff57

static void terminal_write_cb(struct tsm_vte* vte, const char* u8,
                               size_t len, void* data) {
    /* Echo data back to VTE so keyboard input appears on screen.
     * We don't have a PTY/shell, so the "send" is just local echo. */
    TerminalComponent* comp = (TerminalComponent*)data;
    if (!comp || !comp->vte) return;
    tsm_vte_input(comp->vte, u8, len);
}

static int terminal_draw_cb(struct tsm_screen* con, uint32_t id,
                             const uint32_t* ch, size_t len,
                             unsigned int width, unsigned int posx,
                             unsigned int posy,
                             const struct tsm_screen_attr* attr,
                             tsm_age_t age, void* data) {
    TerminalComponent* comp = (TerminalComponent*)data;
    if (!comp) return 0;

    int x = comp->layer->rect.x + comp->input_padding +
            (int)posx * comp->cell_width;
    int y = comp->layer->rect.y + comp->input_padding +
            (int)posy * comp->line_height;
    int cell_w = (int)width * comp->cell_width;

    unsigned int cy = tsm_screen_get_cursor_y(comp->screen);
    Color fg = comp->output_color;
    Color bg = comp->output_bg_color;

    if ((unsigned int)posy == cy) {
        if ((unsigned int)posx < strlen(comp->prompt_text))
            fg = comp->prompt_color;
        else
            fg = comp->input_color;
    }

    if (attr->inverse) {
        Color tmp = fg; fg = bg; bg = tmp;
    }

    /* Fill background for this cell */
    Rect bg_rect = { x, y, cell_w, comp->line_height };
    backend_render_fill_rect(&bg_rect, bg);

    /* Render character if there is one */
    if (len > 0 && ch[0] != 0 && ch[0] != ' ') {
                Texture* tex = render_text(comp->layer, ch, fg);
        if (tex) {
            int tw, th;
            backend_query_texture(tex, NULL, NULL, &tw, &th);
            int dst_w = tw / yui_density;
            int dst_h = th / yui_density;
            int dst_x = x;
            int dst_y = y + (comp->line_height - dst_h) / 2;
            if (width <= 1 && dst_w > cell_w) dst_w = cell_w;
            Rect dst = { dst_x, dst_y, dst_w, dst_h };
            backend_render_text_copy(tex, NULL, &dst);
            backend_render_text_destroy(tex);
        }
    }

    return 0;
}

static void terminal_write_line(TerminalComponent* comp, const char* text) {
    if (!comp || !comp->vte) return;
    if (text && text[0] != '\0') {
        tsm_vte_input(comp->vte, text, strlen(text));
    }
    tsm_vte_input(comp->vte, "\r\n", 2);
}

static void terminal_redraw_input(TerminalComponent* comp) {
    if (!comp || !comp->screen || !comp->layer) return;
    Layer* layer = comp->layer;

    if (comp->needs_prompt) {
        tsm_vte_input(comp->vte, comp->prompt_text, strlen(comp->prompt_text));
        comp->needs_prompt = 0;
    }

    unsigned int cy = tsm_screen_get_cursor_y(comp->screen);
    unsigned int px = (unsigned int)strlen(comp->prompt_text);

    tsm_screen_move_to(comp->screen, px, cy);
    tsm_screen_erase_cursor_to_end(comp->screen, false);

    if (layer->text && layer->text[0]) {
        tsm_vte_input(comp->vte, layer->text, strlen(layer->text));
    }

    tsm_screen_move_to(comp->screen, px + comp->cursor_pos, cy);
}

static void terminal_clear_screen(TerminalComponent* comp) {
    if (!comp || !comp->vte || !comp->screen) return;
    tsm_vte_reset(comp->vte);
    tsm_screen_erase_screen(comp->screen, false);
    tsm_screen_clear_sb(comp->screen);
    tsm_screen_sb_reset(comp->screen);
    tsm_screen_move_to(comp->screen, 0, 0);
    mark_layer_dirty(comp->layer, DIRTY_COLOR);
}

// 终端组件不接管 data 指针：仅从中读取文本写入屏幕。
// 按约定返回 0 表示"未接管所有权"，由 handle_data 释放复制出的 cJSON；
// 返回非零会被 layer_set_data 视为"已接管"，导致 handle_data 跳过释放（泄漏）。
static int terminal_on_data_update(Layer* layer, cJSON* json) {
    if (!layer || !json) return 0;
    TerminalComponent* comp = (TerminalComponent*)layer->component;
    if (!comp || !comp->vte) return 0;
    if (!cJSON_IsArray(json)) {
        if (cJSON_IsString(json)) {
            terminal_write_line(comp, json->valuestring);
            mark_layer_dirty(layer, DIRTY_COLOR);
            return 0;
        }
        return 0;
    }
    int n = cJSON_GetArraySize(json);
    if (n == 0) {
        terminal_clear_screen(comp);
        comp->needs_prompt = 1;
        return 0;
    }
    for (int i = 0; i < n; i++) {
        cJSON* item = cJSON_GetArrayItem(json, i);
        if (cJSON_IsString(item)) {
            terminal_write_line(comp, item->valuestring);
        } else if (cJSON_IsObject(item)) {
            cJSON* text = cJSON_GetObjectItem(item, "text");
            if (text && cJSON_IsString(text)) {
                terminal_write_line(comp, text->valuestring);
            }
        }
    }
    comp->needs_prompt = 1;
    mark_layer_dirty(layer, DIRTY_COLOR);
    return 0;
}

static void terminal_layer_destroy(Layer* layer) {
    if (!layer || !layer->component) return;
    terminal_component_destroy((TerminalComponent*)layer->component);
    layer->component = NULL;
}

static cJSON* terminal_component_get_property(Layer* layer, const char* key) {
    if (!layer || !key) return NULL;
    TerminalComponent* comp = (TerminalComponent*)layer->component;
    if (!comp) return NULL;

    if (strcmp(key, "_keyType") == 0) {
        return cJSON_CreateNumber(comp->last_key_type);
    }
    if (strcmp(key, "_keyCode") == 0) {
        return cJSON_CreateNumber(comp->last_key_code);
    }
    if (strcmp(key, "_keyMod") == 0) {
        return cJSON_CreateNumber(comp->last_key_mod);
    }
    if (strcmp(key, "_keyText") == 0) {
        return cJSON_CreateString(comp->last_key_text);
    }
    return NULL;
}

TerminalComponent* terminal_component_create(Layer* layer) {
    if (!layer) return NULL;

    TerminalComponent* comp = calloc(1, sizeof(TerminalComponent));
    if (!comp) return NULL;

    int r;
    r = tsm_screen_new(&comp->screen, NULL, NULL);
    if (r != 0) {
        free(comp);
        return NULL;
    }
    r = tsm_vte_new(&comp->vte, comp->screen, terminal_write_cb, comp, NULL, NULL);
    if (r != 0) {
        tsm_screen_unref(comp->screen);
        free(comp);
        return NULL;
    }

    comp->layer = layer;
    comp->cursor_pos = 0;
    comp->history_index = -1;
    comp->history_capacity = 32;
    comp->history = malloc(comp->history_capacity * sizeof(char*));
    comp->input_height = 28;
    comp->input_padding = 8;
    comp->prompt_color = (Color){137, 180, 250, 255};
    comp->input_color = (Color){205, 214, 244, 255};
    comp->cursor_color = (Color){205, 214, 244, 255};
    comp->output_color = (Color){166, 227, 161, 255};
    comp->output_bg_color = (Color){30, 30, 46, 255};
    comp->scrollback_max = 1000;
    strcpy(comp->prompt_text, "$ ");

    comp->line_height = 18;
    comp->cell_width = 8;
    comp->cols = 0;
    comp->rows = 0;

    tsm_screen_set_max_sb(comp->screen, comp->scrollback_max);
    tsm_screen_resize(comp->screen, 80, 24);
    comp->needs_prompt = 1;

    layer->component = comp;
    layer->render = terminal_component_render;
    layer->handle_pointer_event = terminal_component_handle_pointer_event;
    layer->handle_key_event = terminal_component_handle_key_event;
    layer->register_event = terminal_component_register_event;
    layer->on_data_update = terminal_on_data_update;
    layer->on_destroy = terminal_layer_destroy;
    layer->get_property = terminal_component_get_property;
    layer->focusable = 1;

    return comp;
}

static void add_history(TerminalComponent* comp, const char* text) {
    if (!text || text[0] == '\0') return;
    if (comp->history_count >= comp->history_capacity) {
        comp->history_capacity *= 2;
        char** new_h = realloc(comp->history, comp->history_capacity * sizeof(char*));
        if (!new_h) return;
        comp->history = new_h;
    }
    comp->history[comp->history_count] = strdup(text);
    comp->history_count++;
    comp->history_index = -1;
}

void terminal_component_append_output(TerminalComponent* comp, const char* text) {
    if (!comp || !comp->vte || !text) return;
    terminal_write_line(comp, text);
    mark_layer_dirty(comp->layer, DIRTY_COLOR);
}

void terminal_component_set_prompt(TerminalComponent* comp, const char* prompt) {
    if (!prompt) return;
    strncpy(comp->prompt_text, prompt, sizeof(comp->prompt_text) - 1);
    comp->prompt_text[sizeof(comp->prompt_text) - 1] = '\0';
}

static void terminal_fire_command_event(TerminalComponent* comp) {
    EventHandler handler;
    if (!comp || !comp->layer || comp->on_command_name[0] == '\0') return;

    if (!comp->layer->event) {
        comp->layer->event = calloc(1, sizeof(Event));
        if (!comp->layer->event) return;
    }
    strncpy(comp->layer->event->click_name,comp->on_command_name,sizeof(comp->layer->event->click_name) - 1);
    comp->layer->event->click_name[sizeof(comp->layer->event->click_name) - 1] = '\0';

    handler = comp->on_command;
    if (!handler) {
        handler = find_event_by_name(comp->on_command_name);
        comp->on_command = handler;
    }
    if (handler) {
        handler(comp->layer);
    }
}

static void terminal_fire_key_event(TerminalComponent* comp) {
    EventHandler handler;
    if (!comp || !comp->layer || comp->on_key_name[0] == '\0') return;

    if (!comp->layer->event) {
        comp->layer->event = calloc(1, sizeof(Event));
        if (!comp->layer->event) return;
    }
    strncpy(comp->layer->event->click_name,comp->on_key_name,sizeof(comp->layer->event->click_name) - 1);
    comp->layer->event->click_name[sizeof(comp->layer->event->click_name) - 1] = '\0';

    handler = comp->on_key;
    if (!handler) {
        handler = find_event_by_name(comp->on_key_name);
        comp->on_key = handler;
    }
    if (handler) {
        handler(comp->layer);
    }
}

static void dispatch_command(TerminalComponent* comp) {
    const char* cmd;
    if (!comp || !comp->layer) return;

    cmd = comp->layer->text ? comp->layer->text : "";

    add_history(comp, cmd);
    terminal_fire_command_event(comp);

    layer_set_text(comp->layer, "");
    comp->cursor_pos = 0;
    comp->needs_prompt = 1;
    mark_layer_dirty(comp->layer, DIRTY_COLOR);
}

TerminalComponent* terminal_component_create_from_json(Layer* layer, cJSON* json_obj) {
    TerminalComponent* comp = terminal_component_create(layer);
    if (!comp) return NULL;

    cJSON* prompt = cJSON_GetObjectItem(json_obj, "prompt");
    if (prompt && cJSON_IsString(prompt)) {
        strncpy(comp->prompt_text, prompt->valuestring, sizeof(comp->prompt_text) - 1);
        comp->prompt_text[sizeof(comp->prompt_text) - 1] = '\0';
    }

    cJSON* input_height = cJSON_GetObjectItem(json_obj, "inputHeight");
    if (input_height && cJSON_IsNumber(input_height)) {
        comp->input_height = input_height->valueint;
    }

    cJSON* scrollback = cJSON_GetObjectItem(json_obj, "scrollback");
    if (scrollback && cJSON_IsNumber(scrollback)) {
        comp->scrollback_max = scrollback->valueint;
        tsm_screen_set_max_sb(comp->screen, comp->scrollback_max);
    }

    cJSON* events = cJSON_GetObjectItem(json_obj, "events");
    if (events) {
        cJSON* on_command = cJSON_GetObjectItem(events, "onCommand");
        if (on_command && cJSON_IsString(on_command) && on_command->valuestring) {
            const char* name = on_command->valuestring;
            if (name[0] == '@') name++;
            strncpy(comp->on_command_name, name, sizeof(comp->on_command_name) - 1);
            comp->on_command_name[sizeof(comp->on_command_name) - 1] = '\0';
        }
        cJSON* on_key = cJSON_GetObjectItem(events, "onKey");
        if (on_key && cJSON_IsString(on_key) && on_key->valuestring) {
            const char* name = on_key->valuestring;
            if (name[0] == '@') name++;
            strncpy(comp->on_key_name, name, sizeof(comp->on_key_name) - 1);
            comp->on_key_name[sizeof(comp->on_key_name) - 1] = '\0';
        }
    }

    cJSON* style = cJSON_GetObjectItem(json_obj, "style");
    if (!style) style = json_obj;
    if (style) {
        cJSON* item = style->child;
        while (item) {
            if (!item->string || !cJSON_IsString(item)) {
                item = item->next;
                continue;
            }
            if (strcmp(item->string, "promptColor") == 0) {
                parse_color(item->valuestring, &comp->prompt_color);
            } else if (strcmp(item->string, "inputColor") == 0) {
                parse_color(item->valuestring, &comp->input_color);
            } else if (strcmp(item->string, "cursorColor") == 0) {
                parse_color(item->valuestring, &comp->cursor_color);
            } else if (strcmp(item->string, "outputColor") == 0) {
                parse_color(item->valuestring, &comp->output_color);
            } else if (strcmp(item->string, "outputBgColor") == 0 ||
                       strcmp(item->string, "bgColor") == 0) {
                parse_color(item->valuestring, &comp->output_bg_color);
                if (strcmp(item->string, "bgColor") == 0) {
                    parse_color(item->valuestring, &layer->bg_color);
                }
            }
            item = item->next;
        }
    }

    return comp;
}

void terminal_component_destroy(TerminalComponent* comp) {
    if (!comp) return;

    if (comp->vte) {
        tsm_vte_unref(comp->vte);
        comp->vte = NULL;
    }
    if (comp->screen) {
        tsm_screen_unref(comp->screen);
        comp->screen = NULL;
    }

    for (int i = 0; i < comp->history_count; i++) {
        free(comp->history[i]);
    }
    free(comp->history);
    free(comp);
}

void terminal_component_render(Layer* layer) {
    if (!layer) return;
    TerminalComponent* comp = (TerminalComponent*)layer->component;
    if (!comp) return;

    backend_render_fill_rect(&layer->rect, comp->output_bg_color);

    if (!layer->font || !layer->font->default_font) {
        if (layer->font) load_all_fonts(layer);
        if (!layer->font || !layer->font->default_font) return;
    }

    int fh = layer->font->size > 0 ? layer->font->size : 14;
    int desired_line_height = fh + 4;
    if (desired_line_height < 1) desired_line_height = 18;
    if (comp->line_height != desired_line_height) {
        comp->line_height = desired_line_height;
    }

    int desired_cell_width = (int)(comp->line_height * 0.6f);
    if (desired_cell_width < 1) desired_cell_width = 8;
    if (comp->cell_width != desired_cell_width) {
        comp->cell_width = desired_cell_width;
    }

    int w = layer->rect.w;
    int h = layer->rect.h;

    int output_w = w - comp->input_padding * 2;
    int output_h = h - comp->input_padding * 2;
    if (output_w < 1) output_w = 1;
    if (output_h < 1) output_h = 1;

    unsigned int new_cols = (unsigned int)(output_w / comp->cell_width);
    unsigned int new_rows = (unsigned int)(output_h / comp->line_height);
    if (new_cols < 1) new_cols = 1;
    if (new_rows < 1) new_rows = 1;

    if (new_cols != comp->cols || new_rows != comp->rows) {
        comp->cols = new_cols;
        comp->rows = new_rows;
        tsm_screen_resize(comp->screen, comp->cols, comp->rows);
        comp->needs_prompt = 1;
    }

    if (comp->needs_prompt) {
        tsm_vte_input(comp->vte, comp->prompt_text, strlen(comp->prompt_text));
        comp->needs_prompt = 0;
    }

    Rect prev_clip;
    render_clip_push(&layer->rect, &prev_clip);

    int out_x = layer->rect.x + comp->input_padding;
    int out_y = layer->rect.y + comp->input_padding;
    int out_w = (int)comp->cols * comp->cell_width;
    int out_h = (int)comp->rows * comp->line_height;
    Rect out_rect = { out_x, out_y, out_w, out_h };
    backend_render_fill_rect(&out_rect, comp->output_bg_color);

    if (comp->screen) {
        Rect out_prev;
        if (render_clip_push(&out_rect, &out_prev)) {
            tsm_screen_draw(comp->screen, terminal_draw_cb, comp);
            render_clip_pop(&out_prev);
        }
    }

    /* Draw cursor block at VTE cursor position */
    if (comp->screen && (layer->state & LAYER_STATE_FOCUSED)) {
        unsigned int cx = tsm_screen_get_cursor_x(comp->screen);
        unsigned int cy = tsm_screen_get_cursor_y(comp->screen);
        int cursor_x = out_x + (int)cx * comp->cell_width;
        int cursor_y = out_y + (int)cy * comp->line_height;
        Rect cursor_rect = { cursor_x, cursor_y, comp->cell_width, comp->line_height };
        /* Semi-transparent cursor block */
        Color cursor_col = comp->cursor_color;
        cursor_col.a = 80;
        backend_render_fill_rect(&cursor_rect, cursor_col);
    }

    render_clip_pop(&prev_clip);
}

int terminal_component_handle_pointer_event(Layer* layer, PointerEvent* event) {
    if (!layer || !event) return 0;
    TerminalComponent* comp = (TerminalComponent*)layer->component;
    if (!comp) return 0;

    int inside = event->x >= layer->rect.x && event->x < layer->rect.x + layer->rect.w &&
                 event->y >= layer->rect.y && event->y < layer->rect.y + layer->rect.h;

    int out_x = layer->rect.x + comp->input_padding;
    int out_y = layer->rect.y + comp->input_padding;
    int cell_x = (event->x - out_x) / comp->cell_width;
    int cell_y = (event->y - out_y) / comp->line_height;
    if (cell_x < 0) cell_x = 0;
    if (cell_y < 0) cell_y = 0;

    if (event->phase == POINTER_DOWN && inside) {
        backend_start_text_input();
        Rect r = { layer->rect.x, layer->rect.y, layer->rect.w, layer->rect.h };
        backend_set_text_input_rect(&r);
        comp->selecting = 1;
        tsm_screen_selection_reset(comp->screen);
        tsm_screen_selection_start(comp->screen, (unsigned int)cell_x, (unsigned int)cell_y);
        return 1;
    }

    if (event->phase == POINTER_MOVE && comp->selecting) {
        tsm_screen_selection_target(comp->screen, (unsigned int)cell_x, (unsigned int)cell_y);
        mark_layer_dirty(layer, DIRTY_COLOR);
        return 1;
    }

    if (event->phase == POINTER_UP && comp->selecting) {
        comp->selecting = 0;
        return 1;
    }

    if (event->phase == POINTER_WHEEL && inside) {
        if (event->delta_y > 0) {
            tsm_screen_sb_up(comp->screen, 3);
        } else {
            tsm_screen_sb_down(comp->screen, 3);
        }
        mark_layer_dirty(layer, DIRTY_COLOR);
        return 1;
    }

    return 0;
}

int terminal_component_handle_key_event(Layer* layer, KeyEvent* event) {
    if (!layer || !event) return 0;
    TerminalComponent* comp = (TerminalComponent*)layer->component;
    if (!comp) return 0;

    /* Store key event data and fire JS callback */
    comp->last_key_type = event->type;
    if (event->type == KEY_EVENT_TEXT_INPUT) {
        strncpy(comp->last_key_text, event->data.text.text, sizeof(comp->last_key_text) - 1);
        comp->last_key_text[sizeof(comp->last_key_text) - 1] = '\0';
        comp->last_key_code = 0;
        comp->last_key_mod = 0;
    } else if (event->type == KEY_EVENT_DOWN) {
        comp->last_key_code = event->data.key.key_code;
        comp->last_key_mod = event->data.key.mod;
        comp->last_key_text[0] = '\0';
    }
    terminal_fire_key_event(comp);

    if (event->type == KEY_EVENT_TEXT_INPUT) {
        const char* text = event->data.text.text;
        size_t input_len = strlen(text);
        if (input_len == 0) return 0;

        char* buf = layer->text;
        int len = buf ? (int)strlen(buf) : 0;

        /* Insert text at cursor position */
        char* new_buf = malloc((size_t)(len + (int)input_len + 1));
        if (!new_buf) return 1;
        if (buf && comp->cursor_pos > 0)
            memcpy(new_buf, buf, (size_t)comp->cursor_pos);
        memcpy(new_buf + comp->cursor_pos, text, input_len);
        if (buf && comp->cursor_pos < len)
            memcpy(new_buf + comp->cursor_pos + (int)input_len,
                   buf + comp->cursor_pos, (size_t)(len - comp->cursor_pos));
        new_buf[len + (int)input_len] = '\0';
        layer_set_text(layer, new_buf);
        free(new_buf);
        comp->cursor_pos += (int)input_len;

        terminal_redraw_input(comp);
        mark_layer_dirty(layer, DIRTY_COLOR);
        return 1;
    }

    if (event->type == KEY_EVENT_DOWN) {
        int key = event->data.key.key_code;
        int mod = event->data.key.mod;

        unsigned int tsm_mod = 0;
        if (mod & KMOD_CTRL) tsm_mod |= TSM_CONTROL_MASK;
        if (mod & KMOD_SHIFT) tsm_mod |= TSM_SHIFT_MASK;
        if (mod & KMOD_ALT) tsm_mod |= TSM_ALT_MASK;

        if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
            tsm_vte_handle_keyboard(comp->vte, XKB_KEY_Return, '\r', tsm_mod, '\r');
            tsm_vte_input(comp->vte, "\n", 1);
            dispatch_command(comp);
            return 1;
        }

        if (key == SDLK_BACKSPACE) {
            char* buf = layer->text;
            int len = buf ? (int)strlen(buf) : 0;
            if (comp->cursor_pos > 0 && len > 0) {
                int remove = 1;
                if (comp->cursor_pos > 1 && (unsigned char)buf[comp->cursor_pos - 1] >= 0x80) {
                    while (remove < comp->cursor_pos &&
                           (unsigned char)buf[comp->cursor_pos - remove] >= 0x80 &&
                           (unsigned char)buf[comp->cursor_pos - remove] < 0xC0)
                        remove++;
                }
                int new_len = len - remove;
                char* new_buf = malloc((size_t)new_len + 1);
                if (!new_buf) return 1;
                if (comp->cursor_pos - remove > 0)
                    memcpy(new_buf, buf, (size_t)(comp->cursor_pos - remove));
                if (comp->cursor_pos < len)
                    memcpy(new_buf + comp->cursor_pos - remove,
                           buf + comp->cursor_pos, (size_t)(len - comp->cursor_pos));
                new_buf[new_len] = '\0';
                comp->cursor_pos -= remove;
                layer_set_text(layer, new_buf);
                free(new_buf);

                terminal_redraw_input(comp);
            }
            mark_layer_dirty(layer, DIRTY_COLOR);
            return 1;
        }

        if (key == SDLK_DELETE) {
            char* buf = layer->text;
            int len = buf ? (int)strlen(buf) : 0;
            if (comp->cursor_pos < len) {
                int remove = 1;
                if ((unsigned char)buf[comp->cursor_pos] >= 0x80) {
                    while (comp->cursor_pos + remove < len &&
                           (unsigned char)buf[comp->cursor_pos + remove] >= 0x80 &&
                           (unsigned char)buf[comp->cursor_pos + remove] < 0xC0)
                        remove++;
                }
                int new_len = len - remove;
                char* new_buf = malloc((size_t)new_len + 1);
                if (!new_buf) return 1;
                if (comp->cursor_pos > 0)
                    memcpy(new_buf, buf, (size_t)comp->cursor_pos);
                if (comp->cursor_pos + remove < len)
                    memcpy(new_buf + comp->cursor_pos,
                           buf + comp->cursor_pos + remove,
                           (size_t)(len - comp->cursor_pos - remove));
                new_buf[new_len] = '\0';
                layer_set_text(layer, new_buf);
                free(new_buf);

                terminal_redraw_input(comp);
            }
            mark_layer_dirty(layer, DIRTY_COLOR);
            return 1;
        }

        if (key == SDLK_LEFT) {
            if (comp->cursor_pos > 0) {
                comp->cursor_pos--;
                unsigned int cy = tsm_screen_get_cursor_y(comp->screen);
                tsm_screen_move_to(comp->screen,
                    (unsigned int)strlen(comp->prompt_text) + comp->cursor_pos, cy);
            }
            return 1;
        }

        if (key == SDLK_RIGHT) {
            int len = layer->text ? (int)strlen(layer->text) : 0;
            if (comp->cursor_pos < len) {
                comp->cursor_pos++;
                unsigned int cy = tsm_screen_get_cursor_y(comp->screen);
                tsm_screen_move_to(comp->screen,
                    (unsigned int)strlen(comp->prompt_text) + comp->cursor_pos, cy);
            }
            return 1;
        }

        if (key == SDLK_HOME) {
            comp->cursor_pos = 0;
            unsigned int cy = tsm_screen_get_cursor_y(comp->screen);
            tsm_screen_move_to(comp->screen,
                (unsigned int)strlen(comp->prompt_text), cy);
            return 1;
        }

        if (key == SDLK_END) {
            comp->cursor_pos = layer->text ? (int)strlen(layer->text) : 0;
            unsigned int cy = tsm_screen_get_cursor_y(comp->screen);
            tsm_screen_move_to(comp->screen,
                (unsigned int)strlen(comp->prompt_text) + comp->cursor_pos, cy);
            return 1;
        }

        if (key == SDLK_UP) {
            if (comp->history_count > 0) {
                if (comp->history_index < 0)
                    comp->history_index = comp->history_count - 1;
                else if (comp->history_index > 0)
                    comp->history_index--;
                layer_set_text(layer, comp->history[comp->history_index]);
                comp->cursor_pos = (int)strlen(comp->history[comp->history_index]);
                terminal_redraw_input(comp);
            }
            mark_layer_dirty(layer, DIRTY_COLOR);
            return 1;
        }

        if (key == SDLK_DOWN) {
            if (comp->history_index >= 0) {
                comp->history_index++;
                if (comp->history_index >= comp->history_count) {
                    comp->history_index = -1;
                    layer_set_text(layer, "");
                } else {
                    layer_set_text(layer, comp->history[comp->history_index]);
                }
                comp->cursor_pos = layer->text ? (int)strlen(layer->text) : 0;
                terminal_redraw_input(comp);
            }
            mark_layer_dirty(layer, DIRTY_COLOR);
            return 1;
        }

        if (key == SDLK_TAB) {
            char* buf = layer->text;
            int len = buf ? (int)strlen(buf) : 0;
            char* new_buf = malloc((size_t)(len + 5));
            if (!new_buf) return 1;
            if (buf && comp->cursor_pos > 0)
                memcpy(new_buf, buf, (size_t)comp->cursor_pos);
            memset(new_buf + comp->cursor_pos, ' ', 4);
            if (buf && comp->cursor_pos < len)
                memcpy(new_buf + comp->cursor_pos + 4,
                       buf + comp->cursor_pos, (size_t)(len - comp->cursor_pos));
            new_buf[len + 4] = '\0';
            layer_set_text(layer, new_buf);
            free(new_buf);
            comp->cursor_pos += 4;
            terminal_redraw_input(comp);
            mark_layer_dirty(layer, DIRTY_COLOR);
            return 1;
        }

        /* Ctrl+C: copy selection */
        if (key == SDLK_c && (mod & KMOD_CTRL)) {
            char* raw = NULL;
            int raw_len = tsm_screen_selection_copy(comp->screen, &raw);
            if (raw_len > 0 && raw) {
                /* libtsm copy_line writes \0 for empty cells, which
                 * truncates C strings. Rebuild a clean copy. */
                char* clean = malloc((size_t)(raw_len + 1));
                if (clean) {
                    int w = 0;
                    for (int r = 0; r < raw_len; r++) {
                        if (raw[r] != '\0')
                            clean[w++] = raw[r];
                    }
                    clean[w] = '\0';
                    backend_set_clipboard_text(clean);
                    free(clean);
                }
                free(raw);
            }
            return 1;
        }

        /* Ctrl+V: paste */
        if (key == SDLK_v && (mod & KMOD_CTRL)) {
            char* clip = backend_get_clipboard_text();
            if (clip && clip[0]) {
                size_t clip_len = strlen(clip);
                char* buf = layer->text;
                int len = buf ? (int)strlen(buf) : 0;
                char* new_buf = malloc((size_t)(len + (int)clip_len + 1));
                if (new_buf) {
                    if (buf && comp->cursor_pos > 0)
                        memcpy(new_buf, buf, (size_t)comp->cursor_pos);
                    memcpy(new_buf + comp->cursor_pos, clip, clip_len);
                    if (buf && comp->cursor_pos < len)
                        memcpy(new_buf + comp->cursor_pos + (int)clip_len,
                               buf + comp->cursor_pos, (size_t)(len - comp->cursor_pos));
                    new_buf[len + (int)clip_len] = '\0';
                    layer_set_text(layer, new_buf);
                    free(new_buf);
                    comp->cursor_pos += (int)clip_len;
                    terminal_redraw_input(comp);
                    mark_layer_dirty(layer, DIRTY_COLOR);
                }
            }
            if (clip) free(clip);
            return 1;
        }
    }

    return 0;
}

int terminal_component_register_event(Layer* layer, const char* event_name,
                                       const char* event_func_name, EventHandler event_handler) {
    if (!layer || !layer->component || !event_handler) return -1;

    TerminalComponent* comp = (TerminalComponent*)layer->component;

    if (strcmp(event_name, "command") == 0 || strcmp(event_name, "onCommand") == 0) {
        comp->on_command = event_handler;
        if (event_func_name && event_func_name[0] != '\0') {
            const char* name = event_func_name;
            if (name[0] == '@') name++;
            strncpy(comp->on_command_name, name, sizeof(comp->on_command_name) - 1);
            comp->on_command_name[sizeof(comp->on_command_name) - 1] = '\0';
        }
        return 0;
    }
    if (strcmp(event_name, "key") == 0 || strcmp(event_name, "onKey") == 0) {
        comp->on_key = event_handler;
        if (event_func_name && event_func_name[0] != '\0') {
            const char* name = event_func_name;
            if (name[0] == '@') name++;
            strncpy(comp->on_key_name, name, sizeof(comp->on_key_name) - 1);
            comp->on_key_name[sizeof(comp->on_key_name) - 1] = '\0';
        }
        return 0;
    }
    return -1;
}
