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
    
    // 设置组件指针和渲染函数
    layer->component = component;
    layer->render = dialog_component_render;
    layer->handle_mouse_event = dialog_component_handle_mouse_event;
    layer->handle_key_event = dialog_component_handle_key_event;
    
    return component;
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
    component->popup_layer->radius = 8;  // 圆角
    component->popup_layer->component = component;
    component->popup_layer->render = dialog_component_render;
    component->popup_layer->handle_mouse_event = dialog_component_handle_mouse_event;
    component->popup_layer->handle_key_event = dialog_component_handle_key_event;
    
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
    component->popup_layer = NULL;  // 先设置为NULL，防止回调中再次使用
    
    // 从弹出管理器中移除
    popup_manager_remove(popup_layer);
    
    component->is_opened = 0;
}

// 检查对话框是否打开
bool dialog_component_is_opened(DialogComponent* component) {
    return component ? component->is_opened : false;
}

// 获取按钮在指定位置的索引
static int dialog_get_button_at_position(DialogComponent* component, int x, int y) {
    if (!component || !component->popup_layer || component->button_count == 0) {
        return -1;
    }
    
    Rect* rect = &component->popup_layer->rect;
    
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
    if (!component || !component->is_opened) {
        return;
    }
    
    if (event->state == 0) { // 鼠标移动
        int old_selected = component->selected_button;
        component->selected_button = dialog_get_button_at_position(component, event->x, event->y);
        
    } else if (event->state == 1 && event->button == 1) { // 左键点击
        int clicked_button = dialog_get_button_at_position(component, event->x, event->y);
        
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
        Texture* title_texture = render_text(layer, component->title, component->title_color);
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
    
    // 绘制消息
    if (strlen(component->message) > 0) {
        Texture* message_texture = render_text(layer, component->message, component->text_color);
        if (message_texture) {
            int message_width, message_height;
            backend_query_texture(message_texture, NULL, NULL, &message_width, &message_height);
            
            // 自动换行处理
            int max_width = rect->w - 40;
            if (message_width / scale > max_width) {
                // 简单的截断处理，实际应用中可以实现更复杂的文本换行
                Rect message_rect = {
                    rect->x + 20,
                    current_y,
                    max_width,
                    message_height / scale
                };
                backend_render_text_copy(message_texture, NULL, &message_rect);
            } else {
                Rect message_rect = {
                    rect->x + (rect->w - message_width / scale) / 2,
                    current_y,
                    message_width / scale,
                    message_height / scale
                };
                backend_render_text_copy(message_texture, NULL, &message_rect);
            }
            
            backend_render_text_destroy(message_texture);
        }
        current_y += 100;
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
            Texture* button_texture = render_text(layer, button->text, component->button_text_color);
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
    
    // 解析标题
    if (cJSON_HasObjectItem(json, "title")) {
        dialog_component_set_title(component, cJSON_GetObjectItem(json, "title")->valuestring);
    }
    
    // 解析消息
    if (cJSON_HasObjectItem(json, "message")) {
        dialog_component_set_message(component, cJSON_GetObjectItem(json, "message")->valuestring);
    }
    
    // 解析类型
    if (cJSON_HasObjectItem(json, "type")) {
        const char* type_str = cJSON_GetObjectItem(json, "type")->valuestring;
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