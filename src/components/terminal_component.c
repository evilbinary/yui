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

static Color ansi_256_to_color(int code) {
    static const Color ansi[16] = {
        {0,0,0,255}, {128,0,0,255}, {0,128,0,255}, {128,128,0,255},
        {0,0,128,255}, {128,0,128,255}, {0,128,128,255}, {192,192,192,255},
        {128,128,128,255}, {255,0,0,255}, {0,255,0,255}, {255,255,0,255},
        {0,0,255,255}, {255,0,255,255}, {0,255,255,255}, {255,255,255,255},
    };
    if (code >= 0 && code < 16) return ansi[code];
    if (code >= 16 && code <= 231) {
        int n = code - 16;
        int r = (n / 36) * 42 + 55;
        int g = ((n / 6) % 6) * 42 + 55;
        int b = (n % 6) * 42 + 55;
        Color c = { r, g, b, 255 };
        return c;
    }
    if (code >= 232 && code <= 255) {
        int v = (code - 232) * 10 + 8;
        Color c = { v, v, v, 255 };
        return c;
    }
    return (Color){205, 214, 244, 255};
}

static Color attr_to_fg(const struct tsm_screen_attr* attr, Color def) {
    if (attr->inverse) {
        if (attr->bccode >= 0) return ansi_256_to_color(attr->bccode);
        return (Color){attr->br, attr->bg, attr->bb, 255};
    }
    if (attr->fccode >= 0) {
        Color c = ansi_256_to_color(attr->fccode);
        if (attr->bold && attr->fccode < 8) {
            c = ansi_256_to_color(attr->fccode + 8);
        }
        return c;
    }
    return (Color){attr->fr, attr->fg, attr->fb, 255};
}

static Color attr_to_bg(const struct tsm_screen_attr* attr, Color def) {
    if (attr->inverse) {
        if (attr->fccode >= 0) return ansi_256_to_color(attr->fccode);
        return (Color){attr->fr, attr->fg, attr->fb, 255};
    }
    if (attr->bccode >= 0) return ansi_256_to_color(attr->bccode);
    return (Color){attr->br, attr->bg, attr->bb, 255};
}

static void terminal_write_cb(struct tsm_vte* vte, const char* u8,
                               size_t len, void* data) {
}

static int terminal_draw_cb(struct tsm_screen* con, uint32_t id,
                             const uint32_t* ch, size_t len,
                             unsigned int width, unsigned int posx,
                             unsigned int posy,
                             const struct tsm_screen_attr* attr,
                             tsm_age_t age, void* data) {
    TerminalComponent* comp = (TerminalComponent*)data;
    if (!comp) return 0;

    int x = comp->input_padding + (int)posx * comp->cell_width;
    int y = comp->input_padding + (int)posy * comp->line_height;
    x += comp->layer->rect.x;
    y += comp->layer->rect.y;

    Color bg = attr_to_bg(attr, comp->output_bg_color);
    if (bg.r != comp->output_bg_color.r || bg.g != comp->output_bg_color.g ||
        bg.b != comp->output_bg_color.b) {
        Rect bg_rect = { x, y, (int)width * comp->cell_width, comp->line_height };
        backend_render_fill_rect(&bg_rect, bg);
    }

    if (len > 0 && ch[0] != 0 && ch[0] != ' ') {
        size_t utf8_len;
        char* utf8 = tsm_ucs4_to_utf8_alloc(ch, len, &utf8_len);
        if (utf8) {
            Color fg = attr_to_fg(attr, comp->input_color);
            Texture* tex = render_text(comp->layer, utf8, fg);
            if (tex) {
                int tw, th;
                backend_query_texture(tex, NULL, NULL, &tw, &th);
                Rect dst = { x, y + (comp->line_height - th / yui_density) / 2,
                             tw / yui_density, th / yui_density };
                backend_render_text_copy(tex, NULL, &dst);
                backend_render_text_destroy(tex);
            }
            free(utf8);
        }
    }

    if (attr->underline) {
        int uy = y + comp->line_height - 2;
        Rect uline = { x, uy, (int)width * comp->cell_width, 1 };
        Color fg = attr_to_fg(attr, comp->input_color);
        backend_render_fill_rect(&uline, fg);
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

static void terminal_clear_screen(TerminalComponent* comp) {
    if (!comp || !comp->vte || !comp->screen) return;
    tsm_vte_reset(comp->vte);
    tsm_screen_erase_screen(comp->screen, false);
    tsm_screen_clear_sb(comp->screen);
    mark_layer_dirty(comp->layer, DIRTY_COLOR);
}

static int terminal_on_data_update(Layer* layer, cJSON* json) {
    if (!layer || !json) return 0;
    TerminalComponent* comp = (TerminalComponent*)layer->component;
    if (!comp || !comp->vte) return 0;
    if (!cJSON_IsArray(json)) {
        if (cJSON_IsString(json)) {
            terminal_write_line(comp, json->valuestring);
            mark_layer_dirty(layer, DIRTY_COLOR);
            return 1;
        }
        return 0;
    }
    int n = cJSON_GetArraySize(json);
    if (n == 0) {
        terminal_clear_screen(comp);
        return 1;
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
    mark_layer_dirty(layer, DIRTY_COLOR);
    return 1;
}

static void terminal_layer_destroy(Layer* layer) {
    if (!layer || !layer->component) return;
    terminal_component_destroy((TerminalComponent*)layer->component);
    layer->component = NULL;
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
    comp->scroll_x = 0;
    comp->history_index = -1;
    comp->history_capacity = 32;
    comp->history = malloc(comp->history_capacity * sizeof(char*));
    comp->input_height = 28;
    comp->input_padding = 8;
    comp->prompt_color = (Color){137, 180, 250, 255};
    comp->input_color = (Color){205, 214, 244, 255};
    comp->cursor_color = (Color){205, 214, 244, 255};
    comp->output_bg_color = (Color){30, 30, 46, 255};
    comp->scrollback_max = 1000;
    strcpy(comp->prompt_text, "$ ");

    comp->line_height = 0;
    comp->cell_width = 0;
    comp->cols = 0;
    comp->rows = 0;

    struct tsm_screen_attr def_attr;
    memset(&def_attr, 0, sizeof(def_attr));
    def_attr.fccode = -1;
    def_attr.bccode = -1;
    def_attr.fr = 205;
    def_attr.fg = 214;
    def_attr.fb = 244;
    def_attr.br = 30;
    def_attr.bg = 30;
    def_attr.bb = 46;
    tsm_screen_set_def_attr(comp->screen, &def_attr);
    tsm_screen_set_max_sb(comp->screen, comp->scrollback_max);
    tsm_screen_set_flags(comp->screen, TSM_SCREEN_HIDE_CURSOR);

    layer->component = comp;
    layer->render = terminal_component_render;
    layer->handle_pointer_event = terminal_component_handle_pointer_event;
    layer->handle_key_event = terminal_component_handle_key_event;
    layer->register_event = terminal_component_register_event;
    layer->on_data_update = terminal_on_data_update;
    layer->on_destroy = terminal_layer_destroy;
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
    strncpy(comp->layer->event->click_name, comp->on_command_name, MAX_PATH - 1);
    comp->layer->event->click_name[MAX_PATH - 1] = '\0';

    handler = comp->on_command;
    if (!handler) {
        handler = find_event_by_name(comp->on_command_name);
        comp->on_command = handler;
    }
    if (handler) {
        handler(comp->layer);
    }
}

static void dispatch_command(TerminalComponent* comp) {
    char line[MAX_TEXT + 64];
    const char* cmd;
    if (!comp || !comp->layer) return;

    cmd = comp->layer->text ? comp->layer->text : "";

    /* 回显：prompt + 命令写入输出区，再交给 JS 处理 */
    snprintf(line, sizeof(line), "%s%s", comp->prompt_text, cmd);
    terminal_write_line(comp, line);

    add_history(comp, cmd);
    terminal_fire_command_event(comp);

    layer_set_text(comp->layer, "");
    comp->cursor_pos = 0;
    comp->scroll_x = 0;
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

static void render_text_at(Layer* layer, int x, int y, const char* text, Color color) {
    if (!text || text[0] == '\0') return;
    Texture* tex = render_text(layer, text, color);
    if (!tex) return;
    int tw = 0, th = 0;
    backend_query_texture(tex, NULL, NULL, &tw, &th);
    Rect dst = { x, y, tw / yui_density, th / yui_density };
    backend_render_text_copy(tex, NULL, &dst);
    backend_render_text_destroy(tex);
}

void terminal_component_render(Layer* layer) {
    if (!layer) return;
    TerminalComponent* comp = (TerminalComponent*)layer->component;
    if (!comp) return;

    backend_render_fill_rect(&layer->rect, layer->bg_color);

    if (!layer->font || !layer->font->default_font) {
        if (layer->font) load_all_fonts(layer);
        if (!layer->font || !layer->font->default_font) return;
    }

    if (comp->line_height <= 0) {
        int fh = layer->font->size > 0 ? layer->font->size : 14;
        comp->line_height = fh + 4;
    }
    if (comp->cell_width <= 0) {
        Texture* m = render_text(layer, "M", comp->input_color);
        if (m) {
            int mw;
            backend_query_texture(m, NULL, NULL, &mw, NULL);
            comp->cell_width = mw / yui_density;
            backend_render_text_destroy(m);
        }
        if (comp->cell_width <= 0) comp->cell_width = comp->line_height * 3 / 5;
    }

    int w = layer->rect.w;
    int h = layer->rect.h;
    int input_area_top = h - comp->input_height - comp->input_padding;

    int output_w = w - comp->input_padding * 2;
    int output_h = input_area_top - comp->input_padding;
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

    {
        int input_y = layer->rect.y + input_area_top;
        {
            Rect input_bg = { layer->rect.x, input_y, w, h - input_area_top };
            backend_render_fill_rect(&input_bg, comp->output_bg_color);
        }

        int prompt_x = layer->rect.x + comp->input_padding;
        int prompt_w = 0;
        {
            Texture* pt = render_text(layer, comp->prompt_text, comp->prompt_color);
            if (pt) {
                int pw;
                backend_query_texture(pt, NULL, NULL, &pw, NULL);
                prompt_w = pw / yui_density;
                backend_render_text_destroy(pt);
            }
        }
        /* 与行高对齐，避免 prompt / 输入 / 光标基线不一致 */
        int prompt_y = input_y + (comp->input_height - comp->line_height) / 2;
        if (prompt_y < input_y) prompt_y = input_y;
        render_text_at(layer, prompt_x, prompt_y, comp->prompt_text, comp->prompt_color);

        int input_x = prompt_x + prompt_w;
        const char* input_text = layer->text ? layer->text : "";
        render_text_at(layer, input_x, prompt_y, input_text, comp->input_color);

        if (layer->state & LAYER_STATE_FOCUSED) {
            int cursor_x = input_x;
            if (comp->cursor_pos > 0 && input_text[0] != '\0') {
                int before_len = comp->cursor_pos;
                char* before = malloc((size_t)before_len + 1);
                if (before) {
                    memcpy(before, input_text, (size_t)before_len);
                    before[before_len] = '\0';
                    Texture* bt = render_text(layer, before, comp->input_color);
                    if (bt) {
                        int bw;
                        backend_query_texture(bt, NULL, NULL, &bw, NULL);
                        cursor_x = input_x + (bw / yui_density) - comp->scroll_x;
                        backend_render_text_destroy(bt);
                    }
                    free(before);
                }
            }

            Rect cursor = { cursor_x, prompt_y, 2, comp->line_height };
            backend_render_fill_rect(&cursor, comp->cursor_color);
        }
    }

    render_clip_pop(&prev_clip);
}

int terminal_component_handle_pointer_event(Layer* layer, PointerEvent* event) {
    if (!layer || !event) return 0;
    TerminalComponent* comp = (TerminalComponent*)layer->component;
    if (!comp) return 0;

    int inside = event->x >= layer->rect.x && event->x < layer->rect.x + layer->rect.w &&
                 event->y >= layer->rect.y && event->y < layer->rect.y + layer->rect.h;

    if (event->phase == POINTER_DOWN && inside) {
        if (!(layer->state & LAYER_STATE_FOCUSED)) {
            if (focused_layer && focused_layer != layer) {
                focused_layer->state = LAYER_STATE_NORMAL;
            }
            focused_layer = layer;
            layer->state = LAYER_STATE_FOCUSED;
            backend_start_text_input();
            Rect r = { layer->rect.x, layer->rect.y, layer->rect.w, layer->rect.h };
            backend_set_text_input_rect(&r);
        }
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

    if (event->type == KEY_EVENT_TEXT_INPUT) {
        const char* text = event->data.text.text;
        char* buf = layer->text;
        int len = buf ? (int)strlen(buf) : 0;
        int input_len = (int)strlen(text);
        if (len + input_len >= MAX_TEXT - 1) return 1;
        char* new_buf = malloc(len + input_len + 1);
        if (!new_buf) return 1;
        if (buf && comp->cursor_pos > 0) {
            memcpy(new_buf, buf, comp->cursor_pos);
        }
        memcpy(new_buf + comp->cursor_pos, text, input_len);
        if (buf && comp->cursor_pos < len) {
            memcpy(new_buf + comp->cursor_pos + input_len, buf + comp->cursor_pos, len - comp->cursor_pos);
        }
        new_buf[len + input_len] = '\0';
        layer_set_text(layer, new_buf);
        free(new_buf);
        comp->cursor_pos += input_len;
        mark_layer_dirty(layer, DIRTY_COLOR);
        return 1;
    }

    if (event->type == KEY_EVENT_DOWN) {
        int key = event->data.key.key_code;
        int mod = event->data.key.mod;

        if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
            dispatch_command(comp);
            return 1;
        }

        if (key == SDLK_BACKSPACE) {
            char* buf = layer->text;
            int len = buf ? (int)strlen(buf) : 0;
            if (comp->cursor_pos > 0 && len > 0) {
                int remove = 1;
                if (comp->cursor_pos > 1 && (unsigned char)buf[comp->cursor_pos - 1] >= 0x80) {
                    while (remove < comp->cursor_pos && (unsigned char)buf[comp->cursor_pos - remove] >= 0x80 &&
                           (unsigned char)buf[comp->cursor_pos - remove] < 0xC0) {
                        remove++;
                    }
                }
                int new_len = len - remove;
                char* new_buf = malloc(new_len + 1);
                if (!new_buf) return 1;
                if (comp->cursor_pos - remove > 0) {
                    memcpy(new_buf, buf, comp->cursor_pos - remove);
                }
                if (comp->cursor_pos < len) {
                    memcpy(new_buf + comp->cursor_pos - remove, buf + comp->cursor_pos, len - comp->cursor_pos);
                }
                new_buf[new_len] = '\0';
                comp->cursor_pos -= remove;
                layer_set_text(layer, new_buf);
                free(new_buf);
                mark_layer_dirty(layer, DIRTY_COLOR);
            }
            return 1;
        }

        if (key == SDLK_DELETE) {
            char* buf = layer->text;
            int len = buf ? (int)strlen(buf) : 0;
            if (comp->cursor_pos < len) {
                int remove = 1;
                if (comp->cursor_pos + 1 < len && (unsigned char)buf[comp->cursor_pos] >= 0x80) {
                    while (comp->cursor_pos + remove < len && (unsigned char)buf[comp->cursor_pos + remove] >= 0x80 &&
                           (unsigned char)buf[comp->cursor_pos + remove] < 0xC0) {
                        remove++;
                    }
                }
                int new_len = len - remove;
                char* new_buf = malloc(new_len + 1);
                if (!new_buf) return 1;
                if (comp->cursor_pos > 0) {
                    memcpy(new_buf, buf, comp->cursor_pos);
                }
                if (comp->cursor_pos + remove < len) {
                    memcpy(new_buf + comp->cursor_pos, buf + comp->cursor_pos + remove, len - comp->cursor_pos - remove);
                }
                new_buf[new_len] = '\0';
                layer_set_text(layer, new_buf);
                free(new_buf);
                mark_layer_dirty(layer, DIRTY_COLOR);
            }
            return 1;
        }

        if (key == SDLK_LEFT) {
            if ((mod & KMOD_CTRL) && comp->cursor_pos > 0) {
                char* buf = layer->text;
                int pos = comp->cursor_pos - 1;
                while (pos > 0 && buf[pos] != ' ') pos--;
                comp->cursor_pos = pos;
            } else if (comp->cursor_pos > 0) {
                comp->cursor_pos--;
                while (comp->cursor_pos > 0 &&
                       (unsigned char)layer->text[comp->cursor_pos] >= 0x80 &&
                       (unsigned char)layer->text[comp->cursor_pos] < 0xC0) {
                    comp->cursor_pos--;
                }
            }
            mark_layer_dirty(layer, DIRTY_COLOR);
            return 1;
        }

        if (key == SDLK_RIGHT) {
            int len = layer->text ? (int)strlen(layer->text) : 0;
            if ((mod & KMOD_CTRL)) {
                char* buf = layer->text;
                int pos = comp->cursor_pos;
                while (pos < len && buf[pos] == ' ') pos++;
                while (pos < len && buf[pos] != ' ') pos++;
                comp->cursor_pos = pos;
            } else if (comp->cursor_pos < len) {
                comp->cursor_pos++;
                while (comp->cursor_pos < len &&
                       (unsigned char)layer->text[comp->cursor_pos] >= 0x80 &&
                       (unsigned char)layer->text[comp->cursor_pos] < 0xC0) {
                    comp->cursor_pos++;
                }
            }
            mark_layer_dirty(layer, DIRTY_COLOR);
            return 1;
        }

        if (key == SDLK_HOME) {
            comp->cursor_pos = 0;
            mark_layer_dirty(layer, DIRTY_COLOR);
            return 1;
        }

        if (key == SDLK_END) {
            comp->cursor_pos = layer->text ? (int)strlen(layer->text) : 0;
            mark_layer_dirty(layer, DIRTY_COLOR);
            return 1;
        }

        if (key == SDLK_UP) {
            if (comp->history_count > 0) {
                if (comp->history_index < 0) {
                    comp->history_index = comp->history_count - 1;
                } else if (comp->history_index > 0) {
                    comp->history_index--;
                }
                layer_set_text(layer, comp->history[comp->history_index]);
                comp->cursor_pos = (int)strlen(comp->history[comp->history_index]);
                comp->scroll_x = 0;
                mark_layer_dirty(layer, DIRTY_COLOR);
            }
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
                comp->scroll_x = 0;
                mark_layer_dirty(layer, DIRTY_COLOR);
            }
            return 1;
        }

        if (key == SDLK_TAB) {
            char* buf = layer->text;
            int len = buf ? (int)strlen(buf) : 0;
            char* new_buf = malloc(len + 5);
            if (!new_buf) return 1;
            if (buf && comp->cursor_pos > 0) {
                memcpy(new_buf, buf, comp->cursor_pos);
            }
            memset(new_buf + comp->cursor_pos, ' ', 4);
            if (buf && comp->cursor_pos < len) {
                memcpy(new_buf + comp->cursor_pos + 4, buf + comp->cursor_pos, len - comp->cursor_pos);
            }
            new_buf[len + 4] = '\0';
            layer_set_text(layer, new_buf);
            free(new_buf);
            comp->cursor_pos += 4;
            mark_layer_dirty(layer, DIRTY_COLOR);
            return 1;
        }
    }

    return 0;
}

int terminal_component_register_event(Layer* layer, const char* event_name,
                                       const char* event_func_name, EventHandler event_handler) {
    if (!layer || !layer->component || !event_handler) return -1;
    if (strcmp(event_name, "command") != 0 && strcmp(event_name, "onCommand") != 0) return -1;

    TerminalComponent* comp = (TerminalComponent*)layer->component;
    comp->on_command = event_handler;
    if (event_func_name && event_func_name[0] != '\0') {
        const char* name = event_func_name;
        if (name[0] == '@') name++;
        strncpy(comp->on_command_name, name, sizeof(comp->on_command_name) - 1);
        comp->on_command_name[sizeof(comp->on_command_name) - 1] = '\0';
    }
    return 0;
}
