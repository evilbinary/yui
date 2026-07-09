#include "dialog_component.h"
#include "../render.h"
#include "../backend.h"
#include "../util.h"
#include "../popup_manager.h"
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"

// 外部变量
extern float scale;

// 默认颜色定义
static const Color DEFAULT_TITLE_COLOR = {50, 50, 50, 255};
static const Color DEFAULT_TEXT_COLOR = {80, 80, 80, 255};
static const Color DEFAULT_BG_COLOR = {255, 255, 255, 255};
static const Color DEFAULT_BORDER_COLOR = {200, 200, 200, 255};
static const Color DEFAULT_BUTTON_COLOR = {70, 130, 180, 255};
static const Color DEFAULT_BUTTON_HOVER_COLOR = {100, 149, 237, 255};
static const Color DEFAULT_BUTTON_TEXT_COLOR = {255, 255, 255, 255};

// 预定义颜色
static const Color INFO_COLOR = {70, 130, 180, 255};
static const Color WARNING_COLOR = {255, 165, 0, 255};
static const Color ERROR_COLOR = {220, 20, 60, 255};
static const Color QUESTION_COLOR = {60, 179, 113, 255};

static int dialog_layer_set_visible(Layer* layer, int visible);
static int dialog_get_button_at_position(DialogComponent* component, Layer* popup_layer, int x, int y);
static int dialog_measure_text_width(Layer* text_layer, const char* text, Color color);
static int dialog_get_line_height(Layer* text_layer, Color color);
static int dialog_render_wrapped_message(Layer* text_layer, DialogComponent* component,
    Rect* dialog_rect, int message_top, int message_area_height, int scroll_offset);
static void dialog_component_handle_scroll_event(Layer* layer, int scroll_delta);
static void dialog_get_message_area(DialogComponent* component, Layer* layer,
    int* message_top, int* message_area_height);
static int dialog_get_scrollbar_thumb(DialogComponent* component, Layer* layer,
    Rect* out_thumb, Rect* out_track);
static void dialog_render_message_scrollbar(DialogComponent* component, Layer* layer,
    int message_top, int message_area_height);

// 创建对话框组件
DialogComponent* dialog_component_create(Layer* layer) {
    if (!layer) {
        return NULL;
    }
    
    DialogComponent* component = (DialogComponent*)malloc(sizeof(DialogComponent));
    if (!component) {
        return NULL;
    }
    
    memset(component, 0, sizeof(DialogComponent));
    component->layer = layer;
    component->popup_layer = NULL;
    component->type = DIALOG_TYPE_INFO;
    component->title[0] = '\0';
    component->message[0] = '\0';
    component->buttons = NULL;
    component->button_count = 0;
    component->selected_button = -1;
    component->is_modal = 1;
    component->is_opened = 0;
    component->message_scroll_offset = 0;
    component->message_content_height = 0;
    component->message_area_height = 0;
    component->scrollbar_dragging = 0;
    component->scrollbar_drag_offset = 0;
    component->user_data = NULL;
    component->on_close = NULL;
    component->on_show = NULL;
    
    // 设置默认颜色
    component->title_color = DEFAULT_TITLE_COLOR;
    component->text_color = DEFAULT_TEXT_COLOR;
    component->bg_color = DEFAULT_BG_COLOR;
    component->border_color = DEFAULT_BORDER_COLOR;
    component->button_color = DEFAULT_BUTTON_COLOR;
    component->button_hover_color = DEFAULT_BUTTON_HOVER_COLOR;
    component->button_text_color = DEFAULT_BUTTON_TEXT_COLOR;
    
    // 模板图层不参与普通渲染，仅通过 popup 显示
    layer->component = component;
    layer->set_visible = dialog_layer_set_visible;
    layer->handle_mouse_event = dialog_component_handle_mouse_event;
    layer->handle_key_event = dialog_component_handle_key_event;
    
    return component;
}

static int dialog_layer_set_visible(Layer* layer, int visible) {
    if (!layer || !layer->component) {
        return 0;
    }

    DialogComponent* component = (DialogComponent*)layer->component;
    if (visible == VISIBLE) {
        if (dialog_component_is_opened(component)) {
            return 1;
        }

        int sw = 0, sh = 0;
        int dw = layer->rect.w > 0 ? layer->rect.w : 400;
        int dh = layer->rect.h > 0 ? layer->rect.h : 200;
        backend_get_windowsize(&sw, &sh);
        int x = sw > dw ? (sw - dw) / 2 : 0;
        int y = sh > dh ? (sh - dh) / 2 : 0;
        return dialog_component_show(component, x, y) ? 1 : 0;
    }

    if (dialog_component_is_opened(component)) {
        dialog_component_hide(component);
    }
    layer->visible = IN_VISIBLE;
    return 1;
}

// 销毁对话框组件
void dialog_component_destroy(DialogComponent* component) {
    if (!component) {
        return;
    }
    
    // 如果对话框正在显示，先关闭它
    if (component->is_opened && component->popup_layer) {
        dialog_component_hide(component);
    }
    
    if (component->buttons) {
        free(component->buttons);
    }
    free(component);
}

// 设置标题
void dialog_component_set_title(DialogComponent* component, const char* title) {
    if (!component || !title) {
        return;
    }
    
    strncpy(component->title, title, sizeof(component->title) - 1);
    component->title[sizeof(component->title) - 1] = '\0';
}

// 设置消息
void dialog_component_set_message(DialogComponent* component, const char* message) {
    if (!component || !message) {
        return;
    }
    
    strncpy(component->message, message, sizeof(component->message) - 1);
    component->message[sizeof(component->message) - 1] = '\0';
}

// 设置对话框类型
void dialog_component_set_type(DialogComponent* component, DialogType type) {
    if (!component) {
        return;
    }
    
    component->type = type;
    
    // 根据类型设置标题颜色
    switch (type) {
        case DIALOG_TYPE_INFO:
            component->title_color = INFO_COLOR;
            break;
        case DIALOG_TYPE_WARNING:
            component->title_color = WARNING_COLOR;
            break;
        case DIALOG_TYPE_ERROR:
            component->title_color = ERROR_COLOR;
            break;
        case DIALOG_TYPE_QUESTION:
            component->title_color = QUESTION_COLOR;
            break;
        default:
            component->title_color = DEFAULT_TITLE_COLOR;
            break;
    }
}

// 设置模态
void dialog_component_set_modal(DialogComponent* component, int modal) {
    if (!component) {
        return;
    }
    component->is_modal = modal;
}

// 设置用户数据
void dialog_component_set_user_data(DialogComponent* component, void* data) {
    if (!component) {
        return;
    }
    component->user_data = data;
}

// 添加按钮
void dialog_component_add_button(DialogComponent* component, const char* text, 
                                 void (*callback)(DialogComponent* dialog, void* user_data),
                                 void* user_data, int is_default, int is_cancel) {
    if (!component || !text) {
        return;
    }
    
    component->buttons = (DialogButton*)realloc(component->buttons, 
                                                (component->button_count + 1) * sizeof(DialogButton));
    if (!component->buttons) {
        return;
    }
    
    DialogButton* button = &component->buttons[component->button_count];
    memset(button, 0, sizeof(DialogButton));
    
    strncpy(button->text, text, sizeof(button->text) - 1);
    button->text[sizeof(button->text) - 1] = '\0';
    
    button->callback = callback;
    button->user_data = user_data;
    button->is_default = is_default;
    button->is_cancel = is_cancel;
    
    component->button_count++;
}

// 清空按钮
void dialog_component_clear_buttons(DialogComponent* component) {
    if (!component) {
        return;
    }
    
    if (component->buttons) {
        free(component->buttons);
        component->buttons = NULL;
    }
    component->button_count = 0;
    component->selected_button = -1;
}

// 获取按钮数量
int dialog_component_get_button_count(DialogComponent* component) {
    return component ? component->button_count : 0;
}

// 设置颜色
void dialog_component_set_colors(DialogComponent* component, Color title, Color text, 
                                Color bg, Color border, Color button, 
                                Color button_hover, Color button_text) {
    if (!component) {
        return;
    }
    
    component->title_color = title;
    component->text_color = text;
    component->bg_color = bg;
    component->border_color = border;
    component->button_color = button;
    component->button_hover_color = button_hover;
    component->button_text_color = button_text;
}

// 弹出对话框关闭回调
static void dialog_popup_close_callback(PopupLayer* popup) {
    if (!popup) return;
    
    // 先保存layer指针，因为在popup_manager_remove中，popup会被释放
    Layer* layer = popup->layer;
    if (!layer) return;
    
    DialogComponent* component = (DialogComponent*)layer->component;
    if (component) {
        component->is_opened = 0;
        // 注意：popup_layer 会在 popup_manager 中被释放，不要在这里再次设置为NULL
        // component->popup_layer = NULL;
        
        // 调用关闭回调
        if (component->on_close) {
            component->on_close(component, -1);  // -1表示未选择按钮
        }
    }
}

// 显示对话框
bool dialog_component_show(DialogComponent* component, int x, int y) {
    if (!component || component->is_opened) {
        return false;
    }
    
    // 创建弹出对话框图层
    component->popup_layer = (Layer*)malloc(sizeof(Layer));
    if (!component->popup_layer) {
        return false;
    }
    
    memset(component->popup_layer, 0, sizeof(Layer));
    
    // 使用layer的尺寸
    int dialog_width = component->layer->rect.w;
    int dialog_height = component->layer->rect.h;
    
    // 如果没有设置尺寸，使用默认值并自动调整
    if (dialog_width == 0 && dialog_height == 0) {
        dialog_width = 400;
        dialog_height = 200;
        
        if (strlen(component->title) > 0) {
            dialog_height += 30;  // 标题空间
        }
        if (strlen(component->message) > 0) {
            dialog_height += 80;  // 消息空间
        }
        if (component->button_count > 0) {
            dialog_height += 50;  // 按钮空间
        }
        
        // 确保最小尺寸
        if (dialog_width < 300) dialog_width = 300;
        if (dialog_height < 150) dialog_height = 150;
    }
    
    // 设置弹出对话框的位置和大小
    component->popup_layer->rect.x = x;
    component->popup_layer->rect.y = y;
    component->popup_layer->rect.w = dialog_width;
    component->popup_layer->rect.h = dialog_height;
    
    // 复制基本属性
    component->popup_layer->radius = component->layer->radius > 0 ? component->layer->radius : 8;
    component->popup_layer->font = component->layer->font;
    component->popup_layer->assets = component->layer->assets;
    component->popup_layer->component = component;
    component->popup_layer->render = dialog_component_render;
    component->popup_layer->handle_mouse_event = dialog_component_handle_mouse_event;
    component->popup_layer->handle_key_event = dialog_component_handle_key_event;
    component->popup_layer->handle_scroll_event = dialog_component_handle_scroll_event;

    component->message_scroll_offset = 0;
    component->message_content_height = 0;
    component->message_area_height = 0;
    component->scrollbar_dragging = 0;
    component->scrollbar_drag_offset = 0;
    
    // 创建弹出层并添加到弹出管理器
    PopupLayer* popup = popup_layer_create(component->popup_layer, POPUP_TYPE_DIALOG, 
                                          component->is_modal ? 200 : 150);
    if (!popup) {
        free(component->popup_layer);
        component->popup_layer = NULL;
        return false;
    }
    
    // 设置关闭回调
    popup->close_callback = dialog_popup_close_callback;
    popup->auto_close = !component->is_modal;  // 模态对话框不自动关闭
    
    // 添加到弹出管理器
    if (!popup_manager_add(popup)) {
        free(component->popup_layer);
        component->popup_layer = NULL;
        // Since popup_manager_add failed, popup is not in the list, so we can safely free it
        free(popup);
        return false;
    }
    
    component->is_opened = 1;
    component->selected_button = -1;
    
    // 调用显示回调
    if (component->on_show) {
        component->on_show(component);
    }
    
    return true;
}

// 隐藏对话框
void dialog_component_hide(DialogComponent* component) {
    if (!component || !component->is_opened || !component->popup_layer) {
        return;
    }
    
    // 保存指针以便调用 popup_manager_remove
    Layer* popup_layer = component->popup_layer;
    component->popup_layer = NULL;
    component->is_opened = 0;
    component->scrollbar_dragging = 0;

    popup_manager_remove(popup_layer);
    free(popup_layer);
}

// 检查对话框是否打开
bool dialog_component_is_opened(DialogComponent* component) {
    return component ? component->is_opened : false;
}

// 获取按钮在指定位置的索引
static int dialog_get_button_at_position(DialogComponent* component, Layer* popup_layer, int x, int y) {
    if (!component || !popup_layer || component->button_count == 0) {
        return -1;
    }
    
    Rect* rect = &popup_layer->rect;
    
    // 按钮区域在对话框底部
    int button_area_y = rect->y + rect->h - 50;
    int button_height = 35;
    int button_width = 80;
    int button_spacing = 20;
    int total_width = component->button_count * button_width + (component->button_count - 1) * button_spacing;
    int start_x = rect->x + (rect->w - total_width) / 2;
    
    for (int i = 0; i < component->button_count; i++) {
        int button_x = start_x + i * (button_width + button_spacing);
        
        if (x >= button_x && x < button_x + button_width &&
            y >= button_area_y && y < button_area_y + button_height) {
            return i;
        }
    }
    
    return -1;
}

static int dialog_measure_text_width(Layer* text_layer, const char* text, Color color) {
    Texture* tex = render_text(text_layer, text, color);
    if (!tex) {
        return 0;
    }
    int w = 0, h = 0;
    backend_query_texture(tex, NULL, NULL, &w, &h);
    backend_render_text_destroy(tex);
    return w / (int)scale;
}

static int dialog_get_line_height(Layer* text_layer, Color color) {
    Texture* tex = render_text(text_layer, "A", color);
    if (!tex) {
        return 18;
    }
    int w = 0, h = 0;
    backend_query_texture(tex, NULL, NULL, &w, &h);
    backend_render_text_destroy(tex);
    return h / (int)scale + 4;
}

static void dialog_get_message_area(DialogComponent* component, Layer* layer,
    int* message_top, int* message_area_height) {
    Rect* rect = &layer->rect;
    int current_y = rect->y + 20;

    if (component && strlen(component->title) > 0) {
        current_y += 40;
    }

    int top = current_y;
    int button_area_y = rect->y + rect->h - 50;
    int area_height = button_area_y - top - 10;
    if (area_height < 0) {
        area_height = 0;
    }

    if (message_top) {
        *message_top = top;
    }
    if (message_area_height) {
        *message_area_height = area_height;
    }
}

static int dialog_get_scrollbar_thumb(DialogComponent* component, Layer* layer,
    Rect* out_thumb, Rect* out_track) {
    if (!component || !layer) {
        return 0;
    }

    int message_top = 0;
    int message_area_height = 0;
    dialog_get_message_area(component, layer, &message_top, &message_area_height);

    int content_h = component->message_content_height;
    int visible_h = message_area_height;
    if (content_h <= visible_h || visible_h <= 0) {
        return 0;
    }

    const int scrollbar_width = 8;
    int scrollbar_x = layer->rect.x + layer->rect.w - scrollbar_width - 8;
    int max_scroll = content_h - visible_h;
    int scroll_offset = component->message_scroll_offset;
    if (scroll_offset < 0) {
        scroll_offset = 0;
    } else if (scroll_offset > max_scroll) {
        scroll_offset = max_scroll;
    }

    int thumb_h = (int)((float)visible_h / content_h * visible_h);
    if (thumb_h < 20) {
        thumb_h = 20;
    }
    if (thumb_h > visible_h) {
        thumb_h = visible_h;
    }

    int thumb_y = message_top;
    if (max_scroll > 0) {
        thumb_y = message_top + (int)((float)scroll_offset / max_scroll * (visible_h - thumb_h));
    }

    if (out_track) {
        *out_track = (Rect){scrollbar_x, message_top, scrollbar_width, visible_h};
    }
    if (out_thumb) {
        *out_thumb = (Rect){scrollbar_x, thumb_y, scrollbar_width, thumb_h};
    }
    return 1;
}

static void dialog_render_message_scrollbar(DialogComponent* component, Layer* layer,
    int message_top, int message_area_height) {
    Rect track = {0};
    Rect thumb = {0};
    if (!dialog_get_scrollbar_thumb(component, layer, &thumb, &track)) {
        return;
    }

    Color track_color = {100, 100, 100, 60};
    backend_render_fill_rect(&track, track_color);

    Color thumb_color = component->border_color;
    if (thumb_color.a == 0) {
        thumb_color = (Color){130, 130, 130, 200};
    }
    backend_render_fill_rect(&thumb, thumb_color);
}

static int dialog_render_wrapped_message(Layer* text_layer, DialogComponent* component,
    Rect* dialog_rect, int message_top, int message_area_height, int scroll_offset) {
    const char* text = component->message;
    if (!text || !text[0] || message_area_height <= 0) {
        return 0;
    }

    int max_width = dialog_rect->w - 52;
    int x = dialog_rect->x + 20;
    int line_height = dialog_get_line_height(text_layer, component->text_color);
    int rel_y = 0;
    const char* paragraph = text;

    Rect clip = {dialog_rect->x, message_top, dialog_rect->w, message_area_height};
    Rect prev_clip;
    backend_render_get_clip_rect(&prev_clip);
    backend_render_set_clip_rect(&clip);

    while (*paragraph) {
        const char* next_nl = strchr(paragraph, '\n');
        int para_len = next_nl ? (int)(next_nl - paragraph) : (int)strlen(paragraph);
        const char* line_start = paragraph;
        const char* para_end = paragraph + para_len;

        while (line_start < para_end) {
            char buf[512];
            int remain = (int)(para_end - line_start);
            if (remain >= (int)sizeof(buf)) {
                remain = (int)sizeof(buf) - 1;
            }
            memcpy(buf, line_start, remain);
            buf[remain] = '\0';

            int fit_len = remain;
            if (dialog_measure_text_width(text_layer, buf, component->text_color) > max_width) {
                fit_len = 0;
                int pos = 0;
                while (line_start + pos < para_end) {
                    int clen = utf8_char_len((unsigned char)line_start[pos]);
                    if (clen <= 0) {
                        clen = 1;
                    }
                    if (pos + clen >= (int)sizeof(buf)) {
                        break;
                    }
                    memcpy(buf, line_start, pos + clen);
                    buf[pos + clen] = '\0';
                    if (dialog_measure_text_width(text_layer, buf, component->text_color) > max_width) {
                        break;
                    }
                    pos += clen;
                    fit_len = pos;
                }
                if (fit_len == 0) {
                    fit_len = utf8_char_len((unsigned char)line_start[0]);
                    if (fit_len <= 0) {
                        fit_len = 1;
                    }
                }
                memcpy(buf, line_start, fit_len);
                buf[fit_len] = '\0';
            }

            int draw_y = message_top + rel_y - scroll_offset;
            if (draw_y + line_height > message_top && draw_y < message_top + message_area_height) {
                Texture* line_tex = render_text(text_layer, buf, component->text_color);
                if (line_tex) {
                    int tw = 0, th = 0;
                    backend_query_texture(line_tex, NULL, NULL, &tw, &th);
                    Rect line_rect = {x, draw_y, tw / (int)scale, th / (int)scale};
                    backend_render_text_copy(line_tex, NULL, &line_rect);
                    backend_render_text_destroy(line_tex);
                }
            }

            line_start += fit_len;
            rel_y += line_height;
        }

        if (next_nl) {
            paragraph = next_nl + 1;
        } else {
            break;
        }
    }

    if (prev_clip.w > 0 && prev_clip.h > 0) {
        backend_render_set_clip_rect(&prev_clip);
    } else {
        backend_render_set_clip_rect(NULL);
    }

    return rel_y;
}

static void dialog_component_handle_scroll_event(Layer* layer, int scroll_delta) {
    DialogComponent* component = (DialogComponent*)layer->component;
    if (!component || !component->is_opened) {
        return;
    }

    int max_scroll = component->message_content_height - component->message_area_height;
    if (max_scroll <= 0) {
        return;
    }

    component->message_scroll_offset += scroll_delta * 20;
    if (component->message_scroll_offset < 0) {
        component->message_scroll_offset = 0;
    } else if (component->message_scroll_offset > max_scroll) {
        component->message_scroll_offset = max_scroll;
    }
}

// 处理键盘事件
void dialog_component_handle_key_event(Layer* layer, KeyEvent* event) {
    DialogComponent* component = (DialogComponent*)layer->component;
    if (!component || !component->is_opened) {
        return;
    }
    
    if (event->type == KEY_EVENT_DOWN) {
        switch (event->data.key.key_code) {
            case 13: // 回车键
                // 触发默认按钮
                for (int i = 0; i < component->button_count; i++) {
                    if (component->buttons[i].is_default) {
                        if (component->buttons[i].callback) {
                            component->buttons[i].callback(component, component->buttons[i].user_data);
                        }
                        if (component->on_close) {
                            component->on_close(component, i);
                        }
                        dialog_component_hide(component);
                        return;
                    }
                }
                // 如果没有默认按钮，触发第一个按钮
                if (component->button_count > 0) {
                    if (component->buttons[0].callback) {
                        component->buttons[0].callback(component, component->buttons[0].user_data);
                    }
                    if (component->on_close) {
                        component->on_close(component, 0);
                    }
                    dialog_component_hide(component);
                }
                break;
                
            case 27: // ESC键
                // 触发取消按钮
                for (int i = 0; i < component->button_count; i++) {
                    if (component->buttons[i].is_cancel) {
                        if (component->buttons[i].callback) {
                            component->buttons[i].callback(component, component->buttons[i].user_data);
                        }
                        if (component->on_close) {
                            component->on_close(component, i);
                        }
                        dialog_component_hide(component);
                        return;
                    }
                }
                // 如果没有取消按钮，直接关闭对话框
                if (component->on_close) {
                    component->on_close(component, -1);
                }
                dialog_component_hide(component);
                break;
                
            case 9: // Tab键
                // 切换按钮选择
                if (component->button_count > 0) {
                    component->selected_button++;
                    if (component->selected_button >= component->button_count) {
                        component->selected_button = 0;
                    }
                }
                break;
        }
    }
}

// 处理鼠标事件
void dialog_component_handle_mouse_event(Layer* layer, MouseEvent* event) {
    DialogComponent* component = (DialogComponent*)layer->component;
    if (!component || !component->is_opened || !event) {
        return;
    }

    Rect thumb = {0};
    Rect track = {0};
    int has_scrollbar = dialog_get_scrollbar_thumb(component, layer, &thumb, &track);
    int message_area_height = component->message_area_height;
    if (message_area_height <= 0) {
        int message_top = 0;
        dialog_get_message_area(component, layer, &message_top, &message_area_height);
    }

    if (event->state == SDL_MOUSEMOTION) {
        if (component->scrollbar_dragging && has_scrollbar) {
            int max_scroll = component->message_content_height - message_area_height;
            if (max_scroll > 0) {
                int thumb_h = thumb.h;
                int track_range = track.h - thumb_h;
                int new_thumb_y = event->y - component->scrollbar_drag_offset;
                if (new_thumb_y < track.y) {
                    new_thumb_y = track.y;
                }
                if (new_thumb_y > track.y + track_range) {
                    new_thumb_y = track.y + track_range;
                }
                if (track_range > 0) {
                    component->message_scroll_offset =
                        (int)((float)(new_thumb_y - track.y) / track_range * max_scroll);
                }
            }
            return;
        }
        component->selected_button = dialog_get_button_at_position(component, layer, event->x, event->y);
    } else if (event->state == SDL_PRESSED && event->button == SDL_BUTTON_LEFT) {
        if (has_scrollbar &&
            event->x >= thumb.x && event->x < thumb.x + thumb.w &&
            event->y >= thumb.y && event->y < thumb.y + thumb.h) {
            component->scrollbar_dragging = 1;
            component->scrollbar_drag_offset = event->y - thumb.y;
            return;
        }
        if (has_scrollbar &&
            event->x >= track.x && event->x < track.x + track.w &&
            event->y >= track.y && event->y < track.y + track.h) {
            int max_scroll = component->message_content_height - message_area_height;
            if (max_scroll > 0) {
                int thumb_h = thumb.h;
                int page = message_area_height;
                if (event->y < thumb.y) {
                    component->message_scroll_offset -= page;
                } else if (event->y >= thumb.y + thumb_h) {
                    component->message_scroll_offset += page;
                }
                if (component->message_scroll_offset < 0) {
                    component->message_scroll_offset = 0;
                } else if (component->message_scroll_offset > max_scroll) {
                    component->message_scroll_offset = max_scroll;
                }
            }
            return;
        }
    } else if (event->state == SDL_RELEASED && event->button == SDL_BUTTON_LEFT) {
        if (component->scrollbar_dragging) {
            component->scrollbar_dragging = 0;
            return;
        }

        int clicked_button = dialog_get_button_at_position(component, layer, event->x, event->y);
        
        if (clicked_button >= 0 && clicked_button < component->button_count) {
            DialogButton* button = &component->buttons[clicked_button];
            if (button->callback) {
                button->callback(component, button->user_data);
            }
            
            // 调用关闭回调
            if (component->on_close) {
                component->on_close(component, clicked_button);
            }
            
            dialog_component_hide(component);
        }
    }
}

// 渲染对话框组件
void dialog_component_render(Layer* layer) {
    if (!layer || !layer->component) {
        return;
    }
    
    DialogComponent* component = (DialogComponent*)layer->component;
    if (!component->is_opened) {
        return;
    }

    Layer* text_layer = component->layer ? component->layer : layer;
    Rect* rect = &layer->rect;
    
    // 绘制背景
    if (component->bg_color.a > 0) {
        backend_render_fill_rect(rect, component->bg_color);
    }
    
    // 绘制边框
    if (component->border_color.a > 0) {
        backend_render_rect_color(rect, component->border_color.r, 
                                  component->border_color.g, 
                                  component->border_color.b, 
                                  component->border_color.a);
    }
    
    // 绘制圆角
    if (layer->radius > 0) {
        backend_render_rounded_rect_with_border(rect, component->bg_color, layer->radius, 
                                               1, component->border_color);
    }
    
    int current_y = rect->y + 20;
    
    // 绘制标题
    if (strlen(component->title) > 0) {
        Texture* title_texture = render_text(text_layer, component->title, component->title_color);
        if (title_texture) {
            int title_width, title_height;
            backend_query_texture(title_texture, NULL, NULL, &title_width, &title_height);
            
            Rect title_rect = {
                rect->x + (rect->w - title_width / scale) / 2,
                current_y,
                title_width / scale,
                title_height / scale
            };
            
            backend_render_text_copy(title_texture, NULL, &title_rect);
            backend_render_text_destroy(title_texture);
        }
        current_y += 40;
    }
    
    int message_top = 0;
    int message_area_height = 0;
    dialog_get_message_area(component, layer, &message_top, &message_area_height);
    component->message_area_height = message_area_height;

    // 绘制消息（支持 \n 换行、自动折行与滚动）
    if (strlen(component->message) > 0 && message_area_height > 0) {
        component->message_content_height = dialog_render_wrapped_message(
            text_layer, component, rect, message_top, message_area_height,
            component->message_scroll_offset);

        int max_scroll = component->message_content_height - message_area_height;
        if (max_scroll < 0) {
            max_scroll = 0;
        }
        if (component->message_scroll_offset > max_scroll) {
            component->message_scroll_offset = max_scroll;
        }

        if (max_scroll > 0) {
            dialog_render_message_scrollbar(component, layer, message_top, message_area_height);
        }
    } else {
        component->message_content_height = 0;
    }
    
    // 绘制按钮
    if (component->button_count > 0) {
        int button_area_y = rect->y + rect->h - 50;
        int button_height = 35;
        int button_width = 80;
        int button_spacing = 20;
        int total_width = component->button_count * button_width + (component->button_count - 1) * button_spacing;
        int start_x = rect->x + (rect->w - total_width) / 2;
        
        for (int i = 0; i < component->button_count; i++) {
            DialogButton* button = &component->buttons[i];
            int button_x = start_x + i * (button_width + button_spacing);
            Rect button_rect = {button_x, button_area_y, button_width, button_height};
            
            // 选择按钮颜色
            Color button_color = component->button_color;
            if (i == component->selected_button) {
                button_color = component->button_hover_color;
            }
            
            // 绘制按钮背景
            backend_render_fill_rect(&button_rect, button_color);
            
            // 绘制按钮文本
            Texture* button_texture = render_text(text_layer, button->text, component->button_text_color);
            if (button_texture) {
                int text_width, text_height;
                backend_query_texture(button_texture, NULL, NULL, &text_width, &text_height);
                
                Rect text_rect = {
                    button_x + (button_width - text_width / scale) / 2,
                    button_area_y + (button_height - text_height / scale) / 2,
                    text_width / scale,
                    text_height / scale
                };
                
                backend_render_text_copy(button_texture, NULL, &text_rect);
                backend_render_text_destroy(button_texture);
            }
        }
    }
}

// 便捷函数实现
static void default_ok_callback(DialogComponent* dialog, void* user_data) {
    // 默认的确定按钮回调
}

static void default_cancel_callback(DialogComponent* dialog, void* user_data) {
    // 默认的取消按钮回调
}

DialogComponent* dialog_component_show_info(const char* title, const char* message) {
    // 这里需要创建一个临时的layer，实际使用时应该由调用者提供
    // 这个函数主要用于演示目的
    return NULL;
}

DialogComponent* dialog_component_show_warning(const char* title, const char* message) {
    return NULL;
}

DialogComponent* dialog_component_show_error(const char* title, const char* message) {
    return NULL;
}

DialogComponent* dialog_component_show_question(const char* title, const char* message) {
    return NULL;
}

// 从JSON创建对话框组件
DialogComponent* dialog_component_create_from_json(Layer* layer, cJSON* json) {
    if (!layer || !json) {
        return NULL;
    }
    
    DialogComponent* component = dialog_component_create(layer);
    if (!component) {
        return NULL;
    }

    // 未配置 visible 时默认隐藏，通过 show 弹出显示
    if (!cJSON_HasObjectItem(json, "visible")) {
        layer->visible = IN_VISIBLE;
    }
    
    // 解析标题
    if (cJSON_HasObjectItem(json, "title")) {
        dialog_component_set_title(component, cJSON_GetObjectItem(json, "title")->valuestring);
    }
    
    // 解析消息
    if (cJSON_HasObjectItem(json, "message")) {
        dialog_component_set_message(component, cJSON_GetObjectItem(json, "message")->valuestring);
    }
    
    // 解析类型（dialogType 优先，避免与图层 type 字段冲突）
    cJSON* type_item = cJSON_GetObjectItem(json, "dialogType");
    if (!type_item) {
        type_item = cJSON_GetObjectItem(json, "type");
        if (type_item && type_item->valuestring &&
            (strcmp(type_item->valuestring, "Dialog") == 0 ||
             strcmp(type_item->valuestring, "dialog") == 0)) {
            type_item = NULL;
        }
    }
    if (type_item && type_item->valuestring) {
        const char* type_str = type_item->valuestring;
        if (strcmp(type_str, "info") == 0) {
            dialog_component_set_type(component, DIALOG_TYPE_INFO);
        } else if (strcmp(type_str, "warning") == 0) {
            dialog_component_set_type(component, DIALOG_TYPE_WARNING);
        } else if (strcmp(type_str, "error") == 0) {
            dialog_component_set_type(component, DIALOG_TYPE_ERROR);
        } else if (strcmp(type_str, "question") == 0) {
            dialog_component_set_type(component, DIALOG_TYPE_QUESTION);
        } else {
            dialog_component_set_type(component, DIALOG_TYPE_CUSTOM);
        }
    }
    
    // 解析模态属性
    if (cJSON_HasObjectItem(json, "modal")) {
        dialog_component_set_modal(component, cJSON_IsTrue(cJSON_GetObjectItem(json, "modal")));
    }
    
    // 解析按钮
    cJSON* buttons = cJSON_GetObjectItem(json, "buttons");
    if (buttons && cJSON_IsArray(buttons)) {
        int button_count = cJSON_GetArraySize(buttons);
        for (int i = 0; i < button_count; i++) {
            cJSON* button = cJSON_GetArrayItem(buttons, i);
            if (button) {
                const char* text = "OK";
                int is_default = 0;
                int is_cancel = 0;
                
                if (cJSON_HasObjectItem(button, "text")) {
                    text = cJSON_GetObjectItem(button, "text")->valuestring;
                }
                if (cJSON_HasObjectItem(button, "default")) {
                    is_default = cJSON_IsTrue(cJSON_GetObjectItem(button, "default"));
                }
                if (cJSON_HasObjectItem(button, "cancel")) {
                    is_cancel = cJSON_IsTrue(cJSON_GetObjectItem(button, "cancel"));
                }
                
                dialog_component_add_button(component, text, NULL, NULL, is_default, is_cancel);
            }
        }
    }
    
    // 解析样式
    cJSON* style = cJSON_GetObjectItem(json, "style");
    if (style) {
        if (cJSON_HasObjectItem(style, "titleColor")) {
            Color title_color;
            parse_color(cJSON_GetObjectItem(style, "titleColor")->valuestring, &title_color);
            component->title_color = title_color;
        }
        if (cJSON_HasObjectItem(style, "textColor")) {
            Color text_color;
            parse_color(cJSON_GetObjectItem(style, "textColor")->valuestring, &text_color);
            component->text_color = text_color;
        }
        if (cJSON_HasObjectItem(style, "bgColor")) {
            Color bg_color;
            parse_color(cJSON_GetObjectItem(style, "bgColor")->valuestring, &bg_color);
            component->bg_color = bg_color;
        }
        if (cJSON_HasObjectItem(style, "borderColor")) {
            Color border_color;
            parse_color(cJSON_GetObjectItem(style, "borderColor")->valuestring, &border_color);
            component->border_color = border_color;
        }
        if (cJSON_HasObjectItem(style, "buttonColor")) {
            Color button_color;
            parse_color(cJSON_GetObjectItem(style, "buttonColor")->valuestring, &button_color);
            component->button_color = button_color;
        }
        if (cJSON_HasObjectItem(style, "buttonHoverColor")) {
            Color button_hover_color;
            parse_color(cJSON_GetObjectItem(style, "buttonHoverColor")->valuestring, &button_hover_color);
            component->button_hover_color = button_hover_color;
        }
        if (cJSON_HasObjectItem(style, "buttonTextColor")) {
            Color button_text_color;
            parse_color(cJSON_GetObjectItem(style, "buttonTextColor")->valuestring, &button_text_color);
            component->button_text_color = button_text_color;
        }
    }
    
    return component;
}