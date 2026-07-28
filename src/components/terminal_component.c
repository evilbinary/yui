#include "terminal_component.h"
#include "../render.h"
#include "../backend.h"
#include "../event.h"
#include "../util.h"
#include "../layer_update.h"
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>

static void append_output(TerminalComponent* comp, const char* text);

static int terminal_on_data_update(Layer* layer, cJSON* json) {
    if (!layer || !json) return 0;
    TerminalComponent* comp = (TerminalComponent*)layer->component;
    if (!comp) return 0;
    int i;
    for (i = 0; i < comp->output_count; i++) {
        free(comp->output_lines[i]);
    }
    comp->output_count = 0;
    if (!cJSON_IsArray(json)) return 1;
    int n = cJSON_GetArraySize(json);
    for (i = 0; i < n; i++) {
        cJSON* item = cJSON_GetArrayItem(json, i);
        if (cJSON_IsString(item)) {
            append_output(comp, item->valuestring);
        } else if (cJSON_IsObject(item)) {
            cJSON* text = cJSON_GetObjectItem(item, "text");
            if (text && cJSON_IsString(text)) {
                append_output(comp, text->valuestring);
            }
        }
    }
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

    comp->layer = layer;
    comp->cursor_pos = 0;
    comp->scroll_x = 0;
    comp->history_index = -1;
    comp->history_capacity = 32;
    comp->history = malloc(comp->history_capacity * sizeof(char*));
    comp->output_capacity = 256;
    comp->output_lines = malloc(comp->output_capacity * sizeof(char*));
    comp->output_scroll = 0;
    comp->input_height = 28;
    comp->input_padding = 8;
    comp->prompt_color = (Color){137, 180, 250, 255};
    comp->input_color = (Color){205, 214, 244, 255};
    comp->cursor_color = (Color){205, 214, 244, 255};
    comp->output_color = (Color){205, 214, 244, 255};
    strcpy(comp->prompt_text, "$ ");

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

static void append_output(TerminalComponent* comp, const char* text) {
    if (!text) return;
    if (comp->output_count >= comp->output_capacity) {
        comp->output_capacity *= 2;
        char** new_o = realloc(comp->output_lines, comp->output_capacity * sizeof(char*));
        if (!new_o) return;
        comp->output_lines = new_o;
    }
    comp->output_lines[comp->output_count] = strdup(text);
    comp->output_count++;
    int total = comp->output_count * comp->line_height;
    int visible = comp->layer->rect.h - comp->input_height - comp->input_padding * 2;
    if (total > visible) {
        comp->output_scroll = total - visible;
    } else {
        comp->output_scroll = 0;
    }
}

void terminal_component_append_output(TerminalComponent* comp, const char* text) {
    append_output(comp, text);
    mark_layer_dirty(comp->layer, DIRTY_COLOR);
}

void terminal_component_set_prompt(TerminalComponent* comp, const char* prompt) {
    if (!prompt) return;
    strncpy(comp->prompt_text, prompt, sizeof(comp->prompt_text) - 1);
    comp->prompt_text[sizeof(comp->prompt_text) - 1] = '\0';
}

static void dispatch_command(TerminalComponent* comp) {
    if (!comp) return;
    char* cmd = comp->layer->text;
    if (!cmd) cmd = "";

    add_history(comp, cmd);

    if (comp->on_command == NULL && comp->on_command_name[0] != '\0') {
        comp->on_command = find_event_by_name(comp->on_command_name);
    }
    if (comp->on_command) {
        comp->on_command(comp->layer);
    }

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
            } else if (strcmp(item->string, "bgColor") == 0) {
                parse_color(item->valuestring, &layer->bg_color);
            }
            item = item->next;
        }
    }

    return comp;
}

void terminal_component_destroy(TerminalComponent* comp) {
    if (!comp) return;
    int i;
    for (i = 0; i < comp->history_count; i++) {
        free(comp->history[i]);
    }
    free(comp->history);
    for (i = 0; i < comp->output_count; i++) {
        free(comp->output_lines[i]);
    }
    free(comp->output_lines);
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

    int w = layer->rect.w;
    int h = layer->rect.h;

    backend_render_fill_rect(&layer->rect, layer->bg_color);

    if (!layer->font || !layer->font->default_font) {
        if (layer->font) load_all_fonts(layer);
        if (!layer->font || !layer->font->default_font) return;
    }

    if (comp->line_height <= 0) {
        int fh = layer->font->size > 0 ? layer->font->size : 14;
        comp->line_height = fh + 4;
    }

    int input_area_top = h - comp->input_height - comp->input_padding;

    Rect prev_clip;
    render_clip_push(&layer->rect, &prev_clip);

    int output_top = layer->rect.y + comp->input_padding;
    int visible_lines = (input_area_top - comp->input_padding) / comp->line_height;
    int first_line = comp->output_scroll / comp->line_height;
    int i;
    for (i = 0; i < visible_lines && (first_line + i) < comp->output_count; i++) {
        int y = output_top + i * comp->line_height - (comp->output_scroll % comp->line_height);
        render_text_at(layer, layer->rect.x + comp->input_padding, y,
                       comp->output_lines[first_line + i], comp->output_color);
    }

    {
        int input_y = layer->rect.y + input_area_top;
        {
            Rect input_bg = { layer->rect.x, input_y, w, comp->input_height + comp->input_padding };
            Color ibg = { 30, 30, 46, 255 };
            backend_render_fill_rect(&input_bg, ibg);
        }

        int prompt_x = layer->rect.x + comp->input_padding;
        int prompt_y = input_y + (comp->input_height - comp->line_height) / 2 + 2;
        render_text_at(layer, prompt_x, prompt_y, comp->prompt_text, comp->prompt_color);

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

        int input_x = prompt_x + prompt_w;
        const char* input_text = layer->text ? layer->text : "";
        render_text_at(layer, input_x, prompt_y, input_text, comp->input_color);

        if (layer->state & LAYER_STATE_FOCUSED) {
            int cursor_x = input_x + comp->scroll_x;
            {
                if (comp->cursor_pos > 0) {
                    int before_len = comp->cursor_pos;
                    char* before = malloc(before_len + 1);
                    if (before) {
                        memcpy(before, input_text, before_len);
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
        int delta = event->delta_y * 20;
        int max_scroll = comp->output_count * comp->line_height -
                        (layer->rect.h - comp->input_height - comp->input_padding * 2);
        if (max_scroll < 0) max_scroll = 0;
        comp->output_scroll += delta;
        if (comp->output_scroll < 0) comp->output_scroll = 0;
        if (comp->output_scroll > max_scroll) comp->output_scroll = max_scroll;
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
