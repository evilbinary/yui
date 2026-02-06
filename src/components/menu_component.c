#include "menu_component.h"
#include "../render.h"
#include "../backend.h"
#include "../util.h"
#include "../popup_manager.h"
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"

// 创建菜单组件
MenuComponent* menu_component_create(Layer* layer) {
    if (!layer) {
        return NULL;
    }
    
    MenuComponent* component = (MenuComponent*)malloc(sizeof(MenuComponent));
    if (!component) {
        return NULL;
    }
    
    memset(component, 0, sizeof(MenuComponent));
    component->layer = layer;
    component->popup_layer = NULL;
    component->item_count = 0;
    component->hovered_item = -1;
    component->opened_item = -1;
    component->is_popup = 0;
    component->is_submenu = 0;
    component->parent_menu = NULL;
    component->item_height = 30;
    component->min_width = 150;
    component->user_data = NULL;
    component->on_popup_closed = NULL;
    
    // 设置默认颜色
    component->bg_color = (Color){255, 255, 255, 255};
    component->text_color = (Color){50, 50, 50, 255};
    component->hover_color = (Color){70, 130, 180, 255};
    component->disabled_color = (Color){150, 150, 150, 255};
    component->separator_color = (Color){200, 200, 200, 255};
    
    // 设置组件指针和自定义渲染函数
    layer->component = component;
    layer->render = menu_component_render;
    
    // 绑定事件处理函数
    layer->handle_mouse_event = menu_component_handle_mouse_event;
    layer->handle_key_event = menu_component_handle_key_event;
    
    return component;
}

// 销毁菜单组件
void menu_component_destroy(MenuComponent* component) {
    if (!component) {
        return;
    }
    
    // 如果弹出菜单正在显示，先关闭它
    if (component->is_popup && component->popup_layer) {
        menu_component_hide_popup(component);
    }
    
    if (component->items) {
        free(component->items);
    }
    free(component);
}

// 从JSON创建菜单组件
MenuComponent* menu_component_create_from_json(Layer* layer, cJSON* json) {
    if (!layer || !json) {
        return NULL;
    }
    
    MenuComponent* component = menu_component_create(layer);
    if (!component) {
        return NULL;
    }
    
    // 解析菜单项
    cJSON* items = cJSON_GetObjectItem(json, "items");
    if (items && cJSON_IsArray(items)) {
        int item_count = cJSON_GetArraySize(items);
        for (int i = 0; i < item_count; i++) {
            cJSON* item = cJSON_GetArrayItem(items, i);
            if (item) {
                const char* text = "";
                const char* shortcut = "";
                int enabled = 1;
                int checked = 0;
                int separator = 0;
                
                if (cJSON_HasObjectItem(item, "text")) {
                    text = cJSON_GetObjectItem(item, "text")->valuestring;
                }
                if (cJSON_HasObjectItem(item, "shortcut")) {
                    shortcut = cJSON_GetObjectItem(item, "shortcut")->valuestring;
                }
                if (cJSON_HasObjectItem(item, "enabled")) {
                    enabled = cJSON_IsTrue(cJSON_GetObjectItem(item, "enabled"));
                }
                if (cJSON_HasObjectItem(item, "checked")) {
                    checked = cJSON_IsTrue(cJSON_GetObjectItem(item, "checked"));
                }
                if (cJSON_HasObjectItem(item, "separator")) {
                    separator = cJSON_IsTrue(cJSON_GetObjectItem(item, "separator"));
                }
                
                menu_component_add_item(component, text, shortcut, enabled, checked, separator, NULL, NULL);
            }
        }
    }
    
    // 解析样式
    cJSON* style = cJSON_GetObjectItem(json, "style");
    if (style) {
        if (cJSON_HasObjectItem(style, "bgColor")) {
            Color bg_color;
            parse_color(cJSON_GetObjectItem(style, "bgColor")->valuestring, &bg_color);
            component->bg_color = bg_color;
        }
        if (cJSON_HasObjectItem(style, "textColor")) {
            Color text_color;
            parse_color(cJSON_GetObjectItem(style, "textColor")->valuestring, &text_color);
            component->text_color = text_color;
        }
        if (cJSON_HasObjectItem(style, "hoverColor")) {
            Color hover_color;
            parse_color(cJSON_GetObjectItem(style, "hoverColor")->valuestring, &hover_color);
            component->hover_color = hover_color;
        }
        if (cJSON_HasObjectItem(style, "disabledColor")) {
            Color disabled_color;
            parse_color(cJSON_GetObjectItem(style, "disabledColor")->valuestring, &disabled_color);
            component->disabled_color = disabled_color;
        }
        if (cJSON_HasObjectItem(style, "separatorColor")) {
            Color separator_color;
            parse_color(cJSON_GetObjectItem(style, "separatorColor")->valuestring, &separator_color);
            component->separator_color = separator_color;
        }
        if (cJSON_HasObjectItem(style, "itemHeight")) {
            component->item_height = cJSON_GetObjectItem(style, "itemHeight")->valueint;
        }
    }
    
    return component;
}

// 添加菜单项
void menu_component_add_item(MenuComponent* component, const char* text, const char* shortcut, 
                            int enabled, int checked, int separator, 
                            void (*callback)(void*), void* user_data) {
    if (!component) {
        return;
    }
    
    // 扩展菜单项数组
    component->items = (MenuItem*)realloc(component->items, 
                                         (component->item_count + 1) * sizeof(MenuItem));
    if (!component->items) {
        return;
    }
    
    MenuItem* item = &component->items[component->item_count];
    memset(item, 0, sizeof(MenuItem));
    
    if (text) {
        strncpy(item->text, text, sizeof(item->text) - 1);
        item->text[sizeof(item->text) - 1] = '\0';
    }
    
    if (shortcut) {
        strncpy(item->shortcut, shortcut, sizeof(item->shortcut) - 1);
        item->shortcut[sizeof(item->shortcut) - 1] = '\0';
    }
    
    item->enabled = enabled;
    item->checked = checked;
    item->separator = separator;
    item->callback = callback;
    item->user_data = user_data;
    
    component->item_count++;
}

// 清空菜单项
void menu_component_clear_items(MenuComponent* component) {
    if (!component) {
        return;
    }
    
    if (component->items) {
        free(component->items);
        component->items = NULL;
    }
    component->item_count = 0;
    component->hovered_item = -1;
    component->opened_item = -1;
}

// 设置颜色
void menu_component_set_colors(MenuComponent* component, Color bg, Color text, 
                              Color hover, Color disabled, Color separator) {
    if (!component) {
        return;
    }
    
    component->bg_color = bg;
    component->text_color = text;
    component->hover_color = hover;
    component->disabled_color = disabled;
    component->separator_color = separator;
}

// 设置菜单项高度
void menu_component_set_item_height(MenuComponent* component, int height) {
    if (!component) {
        return;
    }
    component->item_height = height;
}

// 设置最小宽度
void menu_component_set_min_width(MenuComponent* component, int width) {
    if (!component) {
        return;
    }
    component->min_width = width;
}

// 设置用户数据
void menu_component_set_user_data(MenuComponent* component, void* data) {
    if (!component) {
        return;
    }
    component->user_data = data;
}

// 获取指定位置对应的菜单项索引
int menu_component_get_item_at_position(MenuComponent* component, int x, int y) {
    if (!component) {
        return -1;
    }
    
    // 使用弹出菜单图层的矩形，如果没有弹出菜单则使用普通图层
    Rect* rect = component->popup_layer ? &component->popup_layer->rect : &component->layer->rect;
    
    // 检查是否在菜单范围内
    if (x < rect->x || x >= rect->x + rect->w || 
        y < rect->y || y >= rect->y + rect->h) {
        return -1;
    }
    
    // 计算菜单项索引
    int relative_y = y - rect->y;
    int index = relative_y / component->item_height;
    
    if (index >= 0 && index < component->item_count) {
        return index;
    }
    
    return -1;
}

// 处理键盘事件
void menu_component_handle_key_event(Layer* layer, KeyEvent* event) {
    MenuComponent* component = (MenuComponent*)layer->component;
    if (!component || component->item_count == 0) {
        return;
    }
    
    if (event->type == KEY_EVENT_DOWN) {
        switch (event->data.key.key_code) {
            case 38: // 上箭头
                if (component->hovered_item <= 0) {
                    component->hovered_item = component->item_count - 1;
                } else {
                    component->hovered_item--;
                }
                // 跳过分隔符
                while (component->hovered_item >= 0 && 
                       component->items[component->hovered_item].separator) {
                    if (component->hovered_item <= 0) {
                        component->hovered_item = component->item_count - 1;
                    } else {
                        component->hovered_item--;
                    }
                }
                break;
                
            case 40: // 下箭头
                if (component->hovered_item < 0 || component->hovered_item >= component->item_count - 1) {
                    component->hovered_item = 0;
                } else {
                    component->hovered_item++;
                }
                // 跳过分隔符
                while (component->hovered_item < component->item_count && 
                       component->items[component->hovered_item].separator) {
                    if (component->hovered_item >= component->item_count - 1) {
                        component->hovered_item = 0;
                    } else {
                        component->hovered_item++;
                    }
                }
                break;
                
            case 13: // 回车键
            case 32: // 空格键
                if (component->hovered_item >= 0 && component->hovered_item < component->item_count) {
                    MenuItem* item = &component->items[component->hovered_item];
                    if (!item->separator && item->enabled && item->callback) {
                        item->callback(item->user_data);
                    }
                    
                    // 如果是弹出菜单，选择后自动关闭
                    if (component->is_popup) {
                        menu_component_hide_popup(component);
                    }
                }
                break;
                
            case 27: // ESC键
                component->hovered_item = -1;
                // 如果是弹出菜单，ESC键关闭菜单
                if (component->is_popup) {
                    menu_component_hide_popup(component);
                }
                break;
        }
    }
}

// 处理鼠标事件
void menu_component_handle_mouse_event(Layer* layer, MouseEvent* event) {
    MenuComponent* component = (MenuComponent*)layer->component;
    if (!component) {
        return;
    }
    
    if (event->state == 0) { // 鼠标移动
        int old_hovered = component->hovered_item;
        component->hovered_item = menu_component_get_item_at_position(component, event->x, event->y);
        
        // 如果悬停项发生变化，需要重新渲染
        if (old_hovered != component->hovered_item) {
            // 可以在这里添加重绘逻辑
        }
        
    } else if (event->state == 1 && event->button == 1) { // 左键点击
        int clicked_item = menu_component_get_item_at_position(component, event->x, event->y);
        
        if (clicked_item >= 0 && clicked_item < component->item_count) {
            MenuItem* item = &component->items[clicked_item];
            if (!item->separator && item->enabled && item->callback) {
                item->callback(item->user_data);
            }
            
            // 如果是弹出菜单，点击后自动关闭
            if (component->is_popup) {
                menu_component_hide_popup(component);
            }
        }
    }
}

// 渲染菜单组件
void menu_component_render(Layer* layer) {
    if (!layer || !layer->component) {
        return;
    }
    
    MenuComponent* component = (MenuComponent*)layer->component;
    Rect* rect = &layer->rect;
    
    // 绘制背景
    if (component->bg_color.a > 0) {
        if (layer->radius > 0) {
            backend_render_rounded_rect(rect, component->bg_color, layer->radius);
        } else {
            backend_render_fill_rect(rect, component->bg_color);
        }
    }
    
    // 绘制边框
    Color border_color = (Color){200, 200, 200, 255};
    if (layer->radius > 0) {
        backend_render_rounded_rect_with_border(rect, component->bg_color, layer->radius, 1, border_color);
    } else {
        backend_render_rect_color(rect, border_color.r, border_color.g, border_color.b, border_color.a);
    }
    
    // 绘制菜单项
    for (int i = 0; i < component->item_count; i++) {
        MenuItem* item = &component->items[i];
        int item_y = rect->y + i * component->item_height;
        Rect item_rect = {rect->x, item_y, rect->w, component->item_height};
        
        if (item->separator) {
            // 绘制分隔符
            int separator_y = item_y + component->item_height / 2;
            backend_render_line(rect->x + 5, separator_y, rect->x + rect->w - 5, separator_y, 
                               component->separator_color);
        } else {
            // 绘制菜单项背景（如果是悬停状态）
            if (i == component->hovered_item) {
                Color hover_bg = component->hover_color;
                if (layer->radius > 0) {
                    backend_render_rounded_rect(&item_rect, hover_bg, 0);
                } else {
                    backend_render_fill_rect(&item_rect, hover_bg);
                }
            }
            
            // 确定文本颜色
            Color text_color = item->enabled ? component->text_color : component->disabled_color;
            
            // 绘制复选框（如果有的话）
            if (item->checked) {
                Rect check_rect = {rect->x + 8, item_y + component->item_height / 2 - 6, 12, 12};
                backend_render_rect(&check_rect, text_color);
                // 绘制勾选标记
                backend_render_line(check_rect.x + 2, check_rect.y + 6, 
                                  check_rect.x + 5, check_rect.y + 9, text_color);
                backend_render_line(check_rect.x + 5, check_rect.y + 9, 
                                  check_rect.x + 10, check_rect.y + 4, text_color);
            }
            
            // 绘制菜单项文本
            int text_x = rect->x + (item->checked ? 28 : 12);
            Texture* text_texture = render_text(layer, item->text, text_color);
            if (text_texture) {
                int text_width, text_height;
                backend_query_texture(text_texture, NULL, NULL, &text_width, &text_height);
                
                Rect text_rect = {
                    text_x,
                    item_y + (component->item_height - text_height / scale) / 2,
                    text_width / scale,
                    text_height / scale
                };
                
                backend_render_text_copy(text_texture, NULL, &text_rect);
                backend_render_text_destroy(text_texture);
            }
            
            // 绘制快捷键文本
            if (strlen(item->shortcut) > 0) {
                Texture* shortcut_texture = render_text(layer, item->shortcut, text_color);
                if (shortcut_texture) {
                    int shortcut_width, shortcut_height;
                    backend_query_texture(shortcut_texture, NULL, NULL, &shortcut_width, &shortcut_height);
                    
                    int shortcut_x = rect->x + rect->w - shortcut_width / scale - 12;
                    Rect shortcut_rect = {
                        shortcut_x,
                        item_y + (component->item_height - shortcut_height / scale) / 2,
                        shortcut_width / scale,
                        shortcut_height / scale
                    };
                    
                    backend_render_text_copy(shortcut_texture, NULL, &shortcut_rect);
                    backend_render_text_destroy(shortcut_texture);
                }
            }
        }
    }
}

// 弹出菜单关闭回调
static void menu_popup_close_callback(PopupLayer* popup) {
    if (!popup) return;
    
    // 先保存layer指针，因为在popup_manager_remove中，popup会被释放
    Layer* layer = popup->layer;
    if (!layer) return;
    
    MenuComponent* component = (MenuComponent*)layer->component;
    if (component) {
        component->is_popup = 0;
        // 注意：popup_layer 会在 popup_manager 中被释放，不要在这里再次设置为NULL
        // component->popup_layer = NULL;
        
        // 调用用户自定义的关闭回调
        if (component->on_popup_closed) {
            component->on_popup_closed(component);
        }
    }
}

// 显示弹出菜单
bool menu_component_show_popup(MenuComponent* component, int x, int y) {
    if (!component || component->is_popup) {
        return false;
    }
    
    // 创建弹出菜单图层
    component->popup_layer = (Layer*)malloc(sizeof(Layer));
    if (!component->popup_layer) {
        return false;
    }
    
    memset(component->popup_layer, 0, sizeof(Layer));
    
    // 计算菜单大小
    int menu_width = component->min_width;
    int menu_height = component->item_count * component->item_height;
    
    // 设置弹出菜单的位置和大小
    component->popup_layer->rect.x = x;
    component->popup_layer->rect.y = y;
    component->popup_layer->rect.w = menu_width;
    component->popup_layer->rect.h = menu_height;
    
    // 复制基本属性
    component->popup_layer->radius = 4;  // 默认圆角
    component->popup_layer->component = component;
    component->popup_layer->render = menu_component_render;
    component->popup_layer->handle_mouse_event = menu_component_handle_mouse_event;
    component->popup_layer->handle_key_event = menu_component_handle_key_event;
    
    // 创建弹出层并添加到弹出管理器
    PopupLayer* popup = popup_layer_create(component->popup_layer, POPUP_TYPE_MENU, 100);
    if (!popup) {
        free(component->popup_layer);
        component->popup_layer = NULL;
        return false;
    }
    
    // 设置关闭回调
    popup->close_callback = menu_popup_close_callback;
    popup->auto_close = true;
    
    // 添加到弹出管理器
    if (!popup_manager_add(popup)) {
        free(component->popup_layer);
        component->popup_layer = NULL;
        // Since popup_manager_add failed, popup is not in the list, so we can safely free it
        free(popup);
        return false;
    }
    
    component->is_popup = 1;
    component->hovered_item = -1;
    
    return true;
}

// 隐藏弹出菜单
void menu_component_hide_popup(MenuComponent* component) {
    if (!component || !component->is_popup || !component->popup_layer) {
        return;
    }
    
    // 保存指针以便调用 popup_manager_remove
    Layer* popup_layer = component->popup_layer;
    component->popup_layer = NULL;  // 先设置为NULL，防止回调中再次使用
    
    // 从弹出管理器中移除
    popup_manager_remove(popup_layer);
    
    component->is_popup = 0;
}

// 检查弹出菜单是否打开
bool menu_component_is_popup_opened(MenuComponent* component) {
    return component ? component->is_popup : false;
}

// 设置弹出菜单关闭回调
void menu_component_set_popup_closed_callback(MenuComponent* component, void (*callback)(MenuComponent* menu)) {
    if (component) {
        component->on_popup_closed = callback;
    }
}