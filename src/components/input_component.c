#include "input_component.h"
#include "../render.h"
#include "../backend.h"
#include "../event.h"
#include "../layer.h"
#include "../layer_update.h"
#include "../util.h"
#include <stdlib.h>
#include <string.h>

extern Layer* focused_layer;

static void input_component_trigger_on_change(InputComponent* component) {
    if (!component || !component->layer || !component->on_change) return;
    component->on_change(component->layer);
}

static void input_component_focus(InputComponent* component) {
    if (!component || !component->layer) return;
    Layer* layer = component->layer;

    if (focused_layer && focused_layer != layer) {
        CLEAR_STATE(focused_layer, LAYER_STATE_FOCUSED);
    }
    focused_layer = layer;
    SET_STATE(layer, LAYER_STATE_FOCUSED);

    backend_start_text_input();
    Rect rect = {
        layer->rect.x,
        layer->rect.y + layer->rect.h,
        layer->rect.w,
        0
    };
    backend_set_text_input_rect(&rect);
}

static void input_component_blur(InputComponent* component) {
    if (!component || !component->layer) return;
    Layer* layer = component->layer;

    CLEAR_STATE(layer, LAYER_STATE_FOCUSED);
    if (focused_layer == layer) {
        focused_layer = NULL;
    }
    backend_stop_text_input();
}

static int input_get_label_width(Layer* layer) {
    const char* label_text = layer_get_label(layer);
    if (!label_text[0] || !layer->font || !layer->font->default_font) {
        return 0;
    }

    Texture* label_texture = backend_render_texture(layer->font->default_font, label_text, layer->color);
    if (!label_texture) return 0;

    int label_width = 0;
    backend_query_texture(label_texture, NULL, NULL, &label_width, NULL);
    backend_render_text_destroy(label_texture);
    return label_width / yui_density;
}

static int input_get_line_height(Layer* layer) {
    if (!layer || !layer->font) return 16;

    if (layer->font->default_font) {
        Texture* tex = render_text(layer, "Ag", layer->color);
        if (tex) {
            int tw = 0, th = 0;
            backend_query_texture(tex, NULL, NULL, &tw, &th);
            backend_render_text_destroy(tex);
            int line_h = th / yui_density;
            if (line_h < 1) line_h = 1;
            return line_h;
        }
    }

    return layer->font->size > 0 ? layer->font->size + 2 : 16;
}

static void input_get_padding(Layer* layer, int* top, int* right, int* bottom, int* left) {
    const int k_default = 5;

    if (!layer) {
        *top = *right = *bottom = *left = k_default;
        return;
    }

    *top = layer_padding_get(layer, 0);
    *right = layer_padding_get(layer, 1);
    *bottom = layer_padding_get(layer, 2);
    *left = layer_padding_get(layer, 3);

    if (*top == 0 && *right == 0 && *bottom == 0 && *left == 0) {
        *top = *right = *bottom = *left = k_default;
    }
}

static void input_get_content_rect(Layer* layer, Rect* out) {
    if (!layer || !out) return;

    int pad_top, pad_right, pad_bottom, pad_left;
    input_get_padding(layer, &pad_top, &pad_right, &pad_bottom, &pad_left);

    int line_h = input_get_line_height(layer);
    int max_v_pad = layer->rect.h - line_h;
    if (max_v_pad < 0) max_v_pad = 0;
    int v_pad_total = pad_top + pad_bottom;
    if (v_pad_total > max_v_pad) {
        pad_top = max_v_pad / 2;
        pad_bottom = max_v_pad - pad_top;
    }

    int label_w = input_get_label_width(layer);
    int label_gap = label_w > 0 ? 5 : 0;

    out->x = layer->rect.x + pad_left + label_w + label_gap;
    out->y = layer->rect.y + pad_top;
    out->w = layer->rect.w - pad_left - pad_right - label_w - label_gap;
    out->h = layer->rect.h - pad_top - pad_bottom;
    if (out->w < 1) out->w = 1;
    if (out->h < 1) out->h = 1;
}

static int input_get_text_draw_y(Layer* layer, const Rect* content, int line_h) {
    if (!layer || !content) return 0;
    if (line_h < 1) line_h = 1;
    if (line_h > content->h) line_h = content->h;
    return content->y + (content->h - line_h) / 2;
}

static void input_component_apply_style(Layer* layer, cJSON* style) {
    InputComponent* component;
    cJSON* item;

    if (!layer || !style || !cJSON_IsObject(style) || !layer->component) {
        return;
    }
    component = (InputComponent*)layer->component;

    cJSON* padding = cJSON_GetObjectItem(style, "padding");
    if (padding && layer_padding_apply_from_json(layer->padding, padding)) {
        if (layer->layout_manager) {
            memcpy(layer->layout_manager->padding, layer->padding, sizeof(layer->padding));
        }
        mark_layer_dirty(layer, DIRTY_LAYOUT);
    }

    item = cJSON_GetObjectItem(style, "borderColor");
    if (item && cJSON_IsString(item)) {
        parse_color(item->valuestring, &component->border_color);
    }
    item = cJSON_GetObjectItem(style, "focusBorderColor");
    if (item && cJSON_IsString(item)) {
        parse_color(item->valuestring, &component->focus_border_color);
    }
}

static int input_text_width(Layer* layer, const char* text, int byte_len) {
    if (!layer || !text || byte_len <= 0) return 0;

    if (byte_len >= MAX_TEXT) byte_len = MAX_TEXT - 1;
    char prefix[MAX_TEXT];
    memcpy(prefix, text, (size_t)byte_len);
    prefix[byte_len] = '\0';

    Texture* tex = render_text(layer, prefix, layer->color);
    if (!tex) return 0;

    int tw = 0, th = 0;
    backend_query_texture(tex, NULL, NULL, &tw, &th);
    backend_render_text_destroy(tex);
    return tw / yui_density;
}

static int input_has_selection(InputComponent* component) {
    return component && component->selection_start != component->selection_end;
}

static int input_sel_lo(InputComponent* component) {
    if (!component) return 0;
    return component->selection_start < component->selection_end ?
           component->selection_start : component->selection_end;
}

static int input_sel_hi(InputComponent* component) {
    if (!component) return 0;
    return component->selection_start > component->selection_end ?
           component->selection_start : component->selection_end;
}

static void input_sync_scroll(InputComponent* component) {
    if (!component || !component->layer) return;

    Layer* layer = component->layer;
    Rect content;
    input_get_content_rect(layer, &content);

    int visible_w = content.w;
    const char* text = layer->text ? layer->text : "";
    int len = (int)strlen(text);
    int full_w = input_text_width(layer, text, len);
    if (full_w <= visible_w) {
        component->scroll_x = 0;
        return;
    }

    int view_lo = input_text_width(layer, text, component->cursor_pos);
    int view_hi = view_lo;
    if (input_has_selection(component)) {
        view_lo = input_text_width(layer, text, input_sel_lo(component));
        view_hi = input_text_width(layer, text, input_sel_hi(component));
    }

    if (view_lo < component->scroll_x) {
        component->scroll_x = view_lo;
    }
    if (view_hi > component->scroll_x + visible_w) {
        component->scroll_x = view_hi - visible_w;
    }

    int max_scroll = full_w - visible_w;
    if (max_scroll < 0) max_scroll = 0;
    if (component->scroll_x < 0) component->scroll_x = 0;
    if (component->scroll_x > max_scroll) component->scroll_x = max_scroll;
}

static int input_pos_from_mouse_x(InputComponent* component, Layer* layer, int mouse_x) {
    if (!component || !layer) return 0;

    const char* text = layer->text ? layer->text : "";
    int len = (int)strlen(text);
    if (len <= 0) return 0;

    Rect content;
    input_get_content_rect(layer, &content);
    int local_x = mouse_x - content.x + component->scroll_x;
    if (local_x <= 0) return 0;

    int pos = 0;
    while (pos < len) {
        int next = pos + get_current_utf8_char_len(text, pos);
        if (next <= pos) next = pos + 1;
        int w_pos = input_text_width(layer, text, pos);
        int w_next = input_text_width(layer, text, next);
        if (w_next >= local_x) {
            return (local_x - w_pos) < (w_next - local_x) ? pos : next;
        }
        pos = next;
    }
    return len;
}

static void input_draw_text_part(Layer* layer, const char* text, Color color,
                                 int draw_x, int draw_y) {
    if (!text || !text[0]) return;

    Texture* tex = render_text(layer, text, color);
    if (!tex) return;

    int tw = 0, th = 0;
    backend_query_texture(tex, NULL, NULL, &tw, &th);
    int part_w = tw / yui_density;
    int part_h = th / yui_density;
    if (part_w < 1) part_w = 1;
    if (part_h < 1) part_h = 1;

    Rect dst = {draw_x, draw_y, part_w, part_h};
    backend_render_text_copy(tex, NULL, &dst);
    backend_render_text_destroy(tex);
}

// 创建输入组件
static void input_layer_destroy(Layer* layer) {
    if (!layer || !layer->component) return;
    input_component_destroy((InputComponent*)layer->component);
    layer->component = NULL;
}

InputComponent* input_component_create(Layer* layer) {
    if (!layer) {
        return NULL;
    }

    InputComponent* component = (InputComponent*)malloc(sizeof(InputComponent));
    if (!component) {
        return NULL;
    }

    memset(component, 0, sizeof(InputComponent));
    component->layer = layer;
    component->max_length = MAX_TEXT - 1;
    component->cursor_pos = 0;
    component->selection_start = 0;
    component->selection_end = 0;
    component->scroll_x = 0;
    component->is_selecting = 0;

    // 设置组件指针和自定义渲染函数
    layer->component = component;
    layer->render = input_component_render;
    layer->handle_pointer_event = input_component_handle_pointer_event;
    layer->handle_key_event = input_component_handle_key_event;
    layer->register_event = input_component_register_event;
    layer->set_style = input_component_apply_style;

    // 设置组件为可聚焦
    layer->focusable = 1;

    component->cursor_color = (Color){255, 0, 0, 255};
    component->selection_color = (Color){137, 180, 250, 255};
    component->border_color = (Color){150, 150, 150, 255};
    component->focus_border_color = (Color){70, 130, 180, 255};
    layer->on_destroy = input_layer_destroy;
    return component;
}

// 从JSON创建复选框组件
InputComponent* input_component_create_from_json(Layer* layer, cJSON* json_obj) {
    InputComponent* component = input_component_create(layer);
    
    // 解析placeholder属性
    if (cJSON_HasObjectItem(json_obj, "placeholder")) {
        input_component_set_placeholder(layer->component, cJSON_GetObjectItem(json_obj, "placeholder")->valuestring);
    }
    
    // 解析maxLength属性
    if (cJSON_HasObjectItem(json_obj, "maxLength")) {
        input_component_set_max_length(layer->component, cJSON_GetObjectItem(json_obj, "maxLength")->valueint);
    }

    cJSON* events = cJSON_GetObjectItem(json_obj, "events");
    if (events && cJSON_HasObjectItem(events, "onChange")) {
        cJSON* on_change_obj = cJSON_GetObjectItem(events, "onChange");
        if (cJSON_IsString(on_change_obj)) {
            const char* event_name = on_change_obj->valuestring;
            if (event_name[0] == '@') event_name++;
            InputComponent* comp = (InputComponent*)layer->component;
            if (comp->change_name) free(comp->change_name);
            comp->change_name = strdup(event_name);
        }
    }

    cJSON* style = cJSON_GetObjectItem(json_obj, "style");
    if (style) {
        input_component_apply_style(layer, style);
    }

    return component;
}

// 销毁输入组件
void input_component_destroy(InputComponent* component) {
    if (component) {
        if (component->change_name) free(component->change_name);
        free(component);
    }
}

// 设置输入文本
void input_component_set_text(InputComponent* component, const char* text) {
    if (!component || !text || !component->layer) {
        return;
    }

    layer_set_text(component->layer, text);
    component->cursor_pos = strlen(component->layer->text);
    component->selection_start = component->cursor_pos;
    component->selection_end = component->cursor_pos;
    input_sync_scroll(component);
}

// 设置占位文本
void input_component_set_placeholder(InputComponent* component, const char* placeholder) {
    if (!component || !placeholder) {
        return;
    }
    
    strncpy(component->placeholder, placeholder, MAX_TEXT - 1);
    component->placeholder[MAX_TEXT - 1] = '\0';
}

// 设置最大输入长度
void input_component_set_max_length(InputComponent* component, int max_length) {
    if (!component || !component->layer) {
        return;
    }

    if (max_length > 0 && max_length < MAX_TEXT) {
        component->max_length = max_length;

        // 如果当前文本超过新的最大长度，截断它
        const char* current_text = component->layer->text;
        if (current_text && strlen(current_text) > max_length) {
            char truncated[MAX_TEXT];
            strncpy(truncated, current_text, max_length);
            truncated[max_length] = '\0';
            layer_set_text(component->layer, truncated);
            component->cursor_pos = max_length;
            component->selection_start = max_length;
            component->selection_end = max_length;
        }
    }
}



// 设置光标颜色
void input_component_set_cursor_color(InputComponent* component, Color cursor_color) {
    if (!component) {
        return;
    }
    
    component->cursor_color = cursor_color;
}

// 处理键盘事件
int input_component_handle_key_event(Layer* layer,  KeyEvent* event) {
    InputComponent* component = (InputComponent*)layer->component;
    if (!component || !event || HAS_STATE(layer, LAYER_STATE_DISABLED)) {
        return 0;
    }
    if(!HAS_STATE(layer, LAYER_STATE_FOCUSED)){
        return 0;
    }

    char buf[MAX_TEXT];
    const char* src = layer->text ? layer->text : "";
    strncpy(buf, src, MAX_TEXT - 1);
    buf[MAX_TEXT - 1] = '\0';

    int current_length = strlen(buf);
    int text_changed = 0;
    int view_changed = 0;

    if (component->selection_start != component->selection_end) {
        component->selection_start = utf8_safe_prefix_bytes(buf, component->selection_start);
        component->selection_end = utf8_safe_prefix_bytes(buf, component->selection_end);
    }
    component->cursor_pos = utf8_safe_prefix_bytes(buf, component->cursor_pos);
    if (component->cursor_pos > current_length) component->cursor_pos = current_length;

    switch (event->type) {
        case KEY_EVENT_TEXT_INPUT:
            if (current_length < component->max_length) {
                if (component->selection_start != component->selection_end) {
                    int start = component->selection_start < component->selection_end ?
                                component->selection_start : component->selection_end;
                    int end = component->selection_start > component->selection_end ?
                              component->selection_start : component->selection_end;

                    memmove(buf + start, buf + end,
                           current_length - end + 1);
                    component->cursor_pos = start;
                    current_length -= (end - start);
                    text_changed = 1;
                }

                if (strlen(event->data.text.text) > 0) {
                    int text_len = strlen(event->data.text.text);
                    int available_space = component->max_length - current_length;
                    int copy_len = text_len < available_space ? text_len : available_space;

                    memmove(buf + component->cursor_pos + copy_len,
                           buf + component->cursor_pos,
                           current_length - component->cursor_pos + 1);

                    memcpy(buf + component->cursor_pos, event->data.text.text, copy_len);

                    component->cursor_pos += copy_len;
                    text_changed = 1;
                }
            }
            break;

        case KEY_EVENT_DOWN: {
            int shift = event->data.key.mod & KMOD_SHIFT;
            int old_cursor = component->cursor_pos;
            switch (event->data.key.key_code) {
                case SDLK_BACKSPACE:
                    if (component->selection_start != component->selection_end) {
                        int start = component->selection_start < component->selection_end ?
                                    component->selection_start : component->selection_end;
                        int end = component->selection_start > component->selection_end ?
                                  component->selection_start : component->selection_end;

                        memmove(buf + start, buf + end,
                               current_length - end + 1);
                        component->cursor_pos = start;
                        text_changed = 1;
                    } else if (current_length > 0 && component->cursor_pos > 0) {
                        int char_len = get_prev_utf8_char_len(buf, component->cursor_pos);
                        if (char_len <= 0) char_len = 1;
                        memmove(buf + component->cursor_pos - char_len,
                               buf + component->cursor_pos,
                               current_length - component->cursor_pos + 1);
                        component->cursor_pos -= char_len;
                        text_changed = 1;
                    }
                    break;

                case SDLK_DELETE:
                    if (component->selection_start != component->selection_end) {
                        int start = component->selection_start < component->selection_end ?
                                    component->selection_start : component->selection_end;
                        int end = component->selection_start > component->selection_end ?
                                  component->selection_start : component->selection_end;

                        memmove(buf + start, buf + end,
                               current_length - end + 1);
                        component->cursor_pos = start;
                        text_changed = 1;
                    } else if (current_length > 0 && component->cursor_pos < current_length) {
                        int char_len = get_current_utf8_char_len(buf, component->cursor_pos);
                        if (char_len <= 0) char_len = 1;
                        memmove(buf + component->cursor_pos,
                               buf + component->cursor_pos + char_len,
                               current_length - component->cursor_pos - char_len + 1);
                        text_changed = 1;
                    }
                    break;

                case SDLK_LEFT:
                    if (component->cursor_pos > 0) {
                        int char_len = get_prev_utf8_char_len(buf, component->cursor_pos);
                        if (char_len <= 0) char_len = 1;
                        component->cursor_pos -= char_len;
                    }
                    break;

                case SDLK_RIGHT:
                    if (component->cursor_pos < current_length) {
                        int char_len = get_current_utf8_char_len(buf, component->cursor_pos);
                        if (char_len <= 0) char_len = 1;
                        component->cursor_pos += char_len;
                    }
                    break;

                case SDLK_HOME:
                    component->cursor_pos = 0;
                    break;

                case SDLK_END:
                    component->cursor_pos = current_length;
                    break;
            }

            if (shift) {
                if (component->selection_start == component->selection_end) {
                    component->selection_start = old_cursor;
                }
                component->selection_end = component->cursor_pos;
            } else if (event->data.key.key_code == SDLK_LEFT ||
                       event->data.key.key_code == SDLK_RIGHT ||
                       event->data.key.key_code == SDLK_HOME ||
                       event->data.key.key_code == SDLK_END) {
                if (input_has_selection(component)) {
                    component->cursor_pos = event->data.key.key_code == SDLK_LEFT ||
                                            event->data.key.key_code == SDLK_HOME
                                            ? input_sel_lo(component) : input_sel_hi(component);
                }
                component->selection_start = component->cursor_pos;
                component->selection_end = component->cursor_pos;
            } else {
                component->selection_start = component->cursor_pos;
                component->selection_end = component->cursor_pos;
            }
            view_changed = 1;
            break;
        }
    }

    if (text_changed) {
        layer_set_text(layer, buf);
        input_component_trigger_on_change(component);
    }
    if (text_changed || view_changed) {
        input_sync_scroll(component);
        mark_layer_dirty(layer, DIRTY_TEXT);
    }
    return 0;
}

int input_component_register_event(Layer* layer, const char* event_name,
                                   const char* event_func_name, EventHandler event_handler) {
    if (!layer || !layer->component) return -1;
    if (strcmp(event_name, "change") != 0 && strcmp(event_name, "onChange") != 0) {
        return -1;
    }

    InputComponent* component = (InputComponent*)layer->component;
    component->on_change = event_handler;
    if (event_func_name && event_func_name[0] == '@') {
        event_func_name++;
    }
    if (component->change_name) free(component->change_name);
    component->change_name = event_func_name ? strdup(event_func_name) : NULL;
    return 0;
}

// 处理鼠标事件
int input_component_handle_pointer_event(Layer* layer, PointerEvent* event) {
    if (!layer || !layer->component || !event) {
        return 0;
    }
    InputComponent* component = (InputComponent*)layer->component;
    Point pt = {event->x, event->y};

    Rect content;
    input_get_content_rect(layer, &content);
    int is_inside = point_in_rect(pt, layer->rect);
    int in_content = point_in_rect(pt, content);

    if (is_inside) {
        SET_STATE(layer, LAYER_STATE_HOVER);
    } else {
        CLEAR_STATE(layer, LAYER_STATE_HOVER);
    }

    if (event->phase == POINTER_MOVE && component->is_selecting) {
        int pos = in_content ? input_pos_from_mouse_x(component, layer, event->x) :
                  (event->x < content.x ? 0 : (layer->text ? (int)strlen(layer->text) : 0));
        component->cursor_pos = pos;
        component->selection_end = pos;
        input_sync_scroll(component);
        mark_layer_dirty(layer, DIRTY_TEXT);
        return 1;
    }

    if (event->phase == POINTER_DOWN && event->button == SDL_BUTTON_LEFT) {
        if (is_inside) {
            input_component_focus(component);
            int pos = in_content ? input_pos_from_mouse_x(component, layer, event->x) :
                      (event->x < content.x ? 0 : (layer->text ? (int)strlen(layer->text) : 0));
            component->cursor_pos = pos;
            component->selection_start = pos;
            component->selection_end = pos;
            component->is_selecting = 1;
            input_sync_scroll(component);
            mark_layer_dirty(layer, DIRTY_TEXT);
            return 1;
        }
        if (focused_layer == layer) {
            input_component_blur(component);
            component->is_selecting = 0;
        }
    } else if (event->phase == POINTER_UP && event->button == SDL_BUTTON_LEFT) {
        if (component->is_selecting) {
            component->is_selecting = 0;
            if (component->selection_start == component->selection_end) {
                component->selection_start = component->cursor_pos;
                component->selection_end = component->cursor_pos;
            }
            mark_layer_dirty(layer, DIRTY_TEXT);
            return 1;
        }
    }

    return is_inside;
}

// 渲染输入组件
void input_component_render(Layer* layer) {
    if (!layer || !layer->component) {
        return;
    }
    
    InputComponent* component = (InputComponent*)layer->component;
    const char* label_text = layer_get_label(layer);
    Color border_color = component->border_color;
    if (HAS_STATE(layer, LAYER_STATE_FOCUSED)) {
        border_color = component->focus_border_color;
    }
    
    if (layer->bg_color.a > 0) {
        if (layer->radius > 0) {
            backend_render_rounded_rect_with_border(&layer->rect, layer->bg_color, layer->radius, 2, border_color);
        } else {
            backend_render_fill_rect(&layer->rect, layer->bg_color);
            backend_render_rect_color(&layer->rect, border_color.r, border_color.g, border_color.b, border_color.a);
        }
    }

    int pad_top, pad_right, pad_bottom, pad_left;
    input_get_padding(layer, &pad_top, &pad_right, &pad_bottom, &pad_left);

    if (label_text[0] != '\0') {
        Color text_color = layer->color;
        Texture* text_texture = render_text(layer, label_text, text_color);
        if (text_texture) {
            int text_width, text_height;
            backend_query_texture(text_texture, NULL, NULL, &text_width, &text_height);
            Rect text_rect = {
                layer->rect.x + pad_left,
                layer->rect.y + (layer->rect.h - text_height / yui_density) / 2,
                text_width / yui_density,
                text_height / yui_density
            };
            backend_render_text_copy(text_texture, NULL, &text_rect);
            backend_render_text_destroy(text_texture);
        }
    }

    Color text_color = layer->color;
    const char* display_text = layer->text ? layer->text : "";
    int is_placeholder = 0;
    if ((!layer->text || layer->text[0] == '\0') && !HAS_STATE(layer, LAYER_STATE_FOCUSED) &&
        strlen(component->placeholder) > 0) {
        display_text = component->placeholder;
        text_color = (Color){150, 150, 150, 150};
        is_placeholder = 1;
    }

    Rect content;
    input_get_content_rect(layer, &content);
    Rect prev_clip;
    backend_render_get_clip_rect(&prev_clip);
    backend_render_set_clip_rect(&content);

    int line_h = input_get_line_height(layer);
    int draw_x = content.x - component->scroll_x;
    int draw_y = input_get_text_draw_y(layer, &content, line_h);
    int buflen = is_placeholder ? 0 : (int)strlen(display_text);

    if (buflen > 0 && layer->font && layer->font->default_font) {
        if (!is_placeholder && input_has_selection(component)) {
            Color sel_fg = {17, 17, 27, 255};
            int lo = utf8_safe_prefix_bytes(display_text, input_sel_lo(component));
            int hi = utf8_safe_prefix_bytes(display_text, input_sel_hi(component));
            int x_lo = draw_x + input_text_width(layer, display_text, lo);
            int x_hi = draw_x + input_text_width(layer, display_text, hi);
            if (x_hi > x_lo) {
                Rect sel = {x_lo, draw_y, x_hi - x_lo, line_h};
                backend_render_fill_rect(&sel, component->selection_color);
            }

            input_draw_text_part(layer, display_text, text_color, draw_x, draw_y);

            if (hi > lo) {
                char part[MAX_TEXT];
                int sel_len = hi - lo;
                if (sel_len >= MAX_TEXT) sel_len = MAX_TEXT - 1;
                memcpy(part, display_text + lo, (size_t)sel_len);
                part[sel_len] = '\0';
                input_draw_text_part(layer, part, sel_fg, x_lo, draw_y);
            }
        } else {
            input_draw_text_part(layer, display_text, text_color, draw_x, draw_y);
        }
    } else if (is_placeholder) {
        input_draw_text_part(layer, display_text, text_color, content.x, draw_y);
    }

    if (HAS_STATE(layer, LAYER_STATE_FOCUSED) && !is_placeholder) {
        int cursor_x = draw_x;
        if (component->cursor_pos > 0) {
            cursor_x += input_text_width(layer, display_text, component->cursor_pos);
        }
        int cursor_y = draw_y;
        int cursor_height = line_h;
        if (cursor_height > content.h) cursor_height = content.h;
        if (cursor_x < content.x) cursor_x = content.x;
        if (cursor_x > content.x + content.w - 1) cursor_x = content.x + content.w - 1;
        backend_render_line(cursor_x, cursor_y, cursor_x, cursor_y + cursor_height, component->cursor_color);
    }

    backend_render_set_clip_rect(&prev_clip);
}