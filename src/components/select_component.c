#include "select_component.h"
#include "../backend.h"
#include "../event.h"
#include "../util.h"
#include "../layer.h"
#include "../popup_manager.h"
#include <stdlib.h>
#include <string.h>

// 外部变量声明
extern float scale;

// 创建 Select 组件
SelectComponent* select_component_create(Layer* layer) {
    SelectComponent* component = (SelectComponent*)malloc(sizeof(SelectComponent));
    if (!component) return NULL;
    
    // 初始化组件属性
    memset(component, 0, sizeof(SelectComponent));
    component->layer = layer;
    component->dropdown_layer = NULL;
    component->items = NULL;
    component->item_count = 0;
    component->selected_index = -1;
    component->expanded = 0;
    component->max_visible_items = 8;
    component->item_height = 28;
    component->hover_index = -1;
    component->scroll_position = 0;
    component->scrollbar_width = 12;
    component->is_dragging = 0;
    component->just_expanded = 0;
    component->placeholder_index = -1;
    
    // 默认颜色配置
    component->bg_color = (Color){255, 255, 255, 255};
    component->text_color = (Color){51, 51, 51, 255};
    component->border_color = (Color){204, 204, 204, 255};
    component->arrow_color = (Color){102, 102, 102, 255};
    component->dropdown_bg_color = (Color){255, 255, 255, 255};
    component->hover_bg_color = (Color){245, 245, 245, 255};
    component->selected_bg_color = (Color){0, 120, 215, 255};
    component->selected_text_color = (Color){255, 255, 255, 255};
    component->disabled_text_color = (Color){170, 170, 170, 255};
    component->scrollbar_color = (Color){180, 180, 180, 255};
    component->scrollbar_bg_color = (Color){240, 240, 240, 255};
    component->focus_border_color = (Color){0, 123, 255, 255};  // 焦点边框蓝色
    component->hover_border_color = (Color){0, 123, 255, 255};  // 悬停边框蓝色
    
    // 默认样式
    component->border_width = 1;
    component->border_radius = 4;
    component->font_size = 14;  // 默认字体大小
    component->font = NULL;  // 字体稍后加载
    
    component->user_data = NULL;
    component->on_selection_changed = NULL;
    component->on_dropdown_expanded = NULL;
    
    // 标准事件支持初始化
    component->on_change = NULL;
    component->change_name = NULL;
    
    // 设置组件和渲染函数
    layer->component = component;
    layer->render = select_component_render;
    layer->handle_mouse_event = select_component_handle_mouse_event;
    layer->handle_key_event = select_component_handle_key_event;
    layer->handle_scroll_event = select_component_handle_scroll_event;
    layer->register_event = select_component_register_event;
    layer->get_property = select_component_get_property;
    
    // 设置滚动事件回调
    if (!layer->event) {
        layer->event = malloc(sizeof(Event));
        memset(layer->event, 0, sizeof(Event));
    }
    layer->event->scroll = (void(*)(void*))select_component_scroll_callback;
    
    return component;
}

// 从 JSON 创建 Select 组件
SelectComponent* select_component_create_from_json(Layer* layer, cJSON* json_obj) {
    if (!layer || !json_obj) return NULL;
    
    SelectComponent* component = select_component_create(layer);
    if (!component) return NULL;
    
    // 解析 Select 特定属性
    // 优先从 selectConfig 读取配置（向后兼容）
    cJSON* selectConfig = cJSON_GetObjectItem(json_obj, "selectConfig");
    cJSON* config_source = selectConfig ? selectConfig : json_obj;
    
    // 最大可见项目数
    if (cJSON_HasObjectItem(config_source, "maxVisibleItems")) {
        component->max_visible_items = cJSON_GetObjectItem(config_source, "maxVisibleItems")->valueint;
    }
    
    // 项目高度
    if (cJSON_HasObjectItem(config_source, "itemHeight")) {
        component->item_height = cJSON_GetObjectItem(config_source, "itemHeight")->valueint;
    }
    
    // 边框样式
    if (cJSON_HasObjectItem(config_source, "borderWidth")) {
        component->border_width = cJSON_GetObjectItem(config_source, "borderWidth")->valueint;
    }
    if (cJSON_HasObjectItem(config_source, "borderRadius")) {
        component->border_radius = cJSON_GetObjectItem(config_source, "borderRadius")->valueint;
    }
    
    // 字体大小
    if (cJSON_HasObjectItem(config_source, "fontSize")) {
        component->font_size = cJSON_GetObjectItem(config_source, "fontSize")->valueint;
    }
    
    // 加载组件专用字体
    if (layer->font && strlen(layer->font->path) > 0) {
        char font_path[MAX_PATH];
        snprintf(font_path, sizeof(font_path), "%s", layer->font->path);
        component->font = backend_load_font(font_path, component->font_size);
    }
    
    // 解析选项数据
    cJSON* items = cJSON_GetObjectItem(config_source, "items");
    if (items && cJSON_IsArray(items)) {
        for (int i = 0; i < cJSON_GetArraySize(items); i++) {
            cJSON* item = cJSON_GetArrayItem(items, i);
            if (cJSON_IsString(item)) {
                select_component_add_item(component, item->valuestring, NULL);
            } else if (cJSON_IsObject(item)) {
                const char* text = "";
                const char* value = NULL;
                int disabled = 0;
                
                // 支持 label 字段（显示文本）
                if (cJSON_HasObjectItem(item, "label")) {
                    text = cJSON_GetObjectItem(item, "label")->valuestring;
                }
                // 兼容旧的 text 字段
                else if (cJSON_HasObjectItem(item, "text")) {
                    text = cJSON_GetObjectItem(item, "text")->valuestring;
                }
                
                // 支持 value 字段（存储值）
                if (cJSON_HasObjectItem(item, "value")) {
                    cJSON* value_obj = cJSON_GetObjectItem(item, "value");
                    if (cJSON_IsString(value_obj)) {
                        value = value_obj->valuestring;
                    } else {
                        // 将非字符串值转换为字符串存储
                        char* value_str = cJSON_Print(value_obj);
                        if (value_str) {
                            // 移除引号
                            int len = strlen(value_str);
                            if (len > 1 && value_str[0] == '"' && value_str[len-1] == '"') {
                                value_str[len-1] = '\0';
                                value = value_str + 1;
                            } else {
                                value = value_str;
                            }
                        }
                    }
                }
                
                if (cJSON_HasObjectItem(item, "disabled")) {
                    disabled = cJSON_IsTrue(cJSON_GetObjectItem(item, "disabled"));
                }
                
                select_component_add_item(component, text, (void*)value);
                if (disabled) {
                    select_component_set_item_disabled(component, component->item_count - 1, 1);
                }
            }
        }
    }
    
    // 解析占位符
    if (cJSON_HasObjectItem(config_source, "placeholder")) {
        select_component_add_placeholder(component, cJSON_GetObjectItem(config_source, "placeholder")->valuestring);
    }
    
    // 解析颜色配置
    cJSON* colors = cJSON_GetObjectItem(config_source, "colors");
    if (colors) {
        if (cJSON_HasObjectItem(colors, "bgColor")) {
            parse_color((char*)cJSON_GetObjectItem(colors, "bgColor")->valuestring, &component->bg_color);
        }
        if (cJSON_HasObjectItem(colors, "textColor")) {
            parse_color((char*)cJSON_GetObjectItem(colors, "textColor")->valuestring, &component->text_color);

        }
        if (cJSON_HasObjectItem(colors, "borderColor")) {
            parse_color((char*)cJSON_GetObjectItem(colors, "borderColor")->valuestring, &component->border_color);
        }
        if (cJSON_HasObjectItem(colors, "arrowColor")) {
            parse_color((char*)cJSON_GetObjectItem(colors, "arrowColor")->valuestring, &component->arrow_color);
        }
        if (cJSON_HasObjectItem(colors, "dropdownBgColor")) {
            char* color_str = (char*)cJSON_GetObjectItem(colors, "dropdownBgColor")->valuestring;
            parse_color(color_str, &component->dropdown_bg_color);
            printf("DEBUG: dropdownBgColor parsed: %s -> (%d,%d,%d,%d)\n", 
                   color_str, component->dropdown_bg_color.r, component->dropdown_bg_color.g, 
                   component->dropdown_bg_color.b, component->dropdown_bg_color.a);
        }
        if (cJSON_HasObjectItem(colors, "hoverBgColor")) {
            parse_color((char*)cJSON_GetObjectItem(colors, "hoverBgColor")->valuestring, &component->hover_bg_color);
        }
        if (cJSON_HasObjectItem(colors, "selectedBgColor")) {
            parse_color((char*)cJSON_GetObjectItem(colors, "selectedBgColor")->valuestring, &component->selected_bg_color);
        }
        if (cJSON_HasObjectItem(colors, "selectedTextColor")) {
            parse_color((char*)cJSON_GetObjectItem(colors, "selectedTextColor")->valuestring, &component->selected_text_color);
        }
        if (cJSON_HasObjectItem(colors, "disabledTextColor")) {
            parse_color((char*)cJSON_GetObjectItem(colors, "disabledTextColor")->valuestring, &component->disabled_text_color);

        }
        if (cJSON_HasObjectItem(colors, "scrollbarColor")) {
            parse_color((char*)cJSON_GetObjectItem(colors, "scrollbarColor")->valuestring, &component->scrollbar_color);
        }
        if (cJSON_HasObjectItem(colors, "scrollbarBgColor")) {
            parse_color((char*)cJSON_GetObjectItem(colors, "scrollbarBgColor")->valuestring, &component->scrollbar_bg_color);
        }
        if (cJSON_HasObjectItem(colors, "focusBorderColor")) {
            parse_color((char*)cJSON_GetObjectItem(colors, "focusBorderColor")->valuestring, &component->focus_border_color);
        }
        if (cJSON_HasObjectItem(colors, "hoverBorderColor")) {
            parse_color((char*)cJSON_GetObjectItem(colors, "hoverBorderColor")->valuestring, &component->hover_border_color);
        }
    }
    
    // 从 layer 的样式中获取基础样式
    if (layer->bg_color.a > 0) {
        component->bg_color = layer->bg_color;
    }
    if (layer->color.a > 0) {
        component->border_color = layer->color;
    }
    
    // 如果没有设置文字颜色，使用 layer 的文字颜色
    if (component->text_color.a == 0 && layer->color.a > 0) {
        component->text_color = layer->color;
    }
    
    // 加载组件专用字体（如果还没有加载的话）
    if (!component->font && layer->font && strlen(layer->font->path) > 0) {
        char font_path[MAX_PATH];
        snprintf(font_path, sizeof(font_path), "%s", layer->font->path);
        component->font = backend_load_font(font_path, component->font_size);
    }
    
    // 解析事件绑定
    cJSON* events = cJSON_GetObjectItem(json_obj, "events");
    if (events) {
        // 解析onChange事件
        if (cJSON_HasObjectItem(events, "onChange")) {
            cJSON* on_change_obj = cJSON_GetObjectItem(events, "onChange");
            if (cJSON_IsString(on_change_obj)) {
                const char* event_name = on_change_obj->valuestring;
                // 将事件名称存储在 change_name 中，稍后由事件系统处理
                component->change_name = strdup(event_name);
                EventHandler handler = find_event_by_name(event_name);
                component->on_change = handler;
            }
        }
    }

    return component;
}

// 销毁 Select 组件
void select_component_destroy(SelectComponent* component) {
    if (!component) return;
    
    // 收起下拉菜单（防止悬空指针）
    if (component->expanded) {
        component->expanded = 0;
    }
    
    // 释放所有选项
    select_component_clear_items(component);
    
    // 清理指针
    component->layer = NULL;
    component->dropdown_layer = NULL;
    component->user_data = NULL;
    component->on_selection_changed = NULL;
    component->on_dropdown_expanded = NULL;
    component->font = NULL;  // 字体由后端管理，不需要手动释放
    
    free(component);
}

// 添加选项
void select_component_add_item(SelectComponent* component, const char* text, void* user_data) {
    if (!component || !text) return;
    
    // 扩展选项数组
    SelectItem* new_items = (SelectItem*)realloc(component->items, (component->item_count + 1) * sizeof(SelectItem));
    if (!new_items) return;
    component->items = new_items;
    
    // 添加新选项
    SelectItem* item = &component->items[component->item_count];
    item->text = strdup(text);
    // 复制 user_data（假设是字符串）
    item->user_data = user_data ? strdup((const char*)user_data) : NULL;
    item->selected = 0;
    item->disabled = 0;
    
    component->item_count++;
    
    // 如果是第一个非占位符选项，自动选中
    if (component->selected_index == -1 && component->placeholder_index == -1) {
        select_component_set_selected(component, 0);
    }
}

// 添加占位符
void select_component_add_placeholder(SelectComponent* component, const char* text) {
    if (!component || !text) return;
    
    // 扩展选项数组
    SelectItem* new_items = (SelectItem*)realloc(component->items, (component->item_count + 1) * sizeof(SelectItem));
    if (!new_items) return;
    component->items = new_items;
    
    // 将现有选项后移
    for (int i = component->item_count; i > 0; i--) {
        component->items[i] = component->items[i - 1];
    }
    
    // 在开头插入占位符
    SelectItem* item = &component->items[0];
    item->text = strdup(text);
    item->user_data = NULL;
    item->selected = 0;
    item->disabled = 1;  // 占位符默认禁用
    
    component->item_count++;
    component->placeholder_index = 0;
    component->selected_index = 0;  // 默认选中占位符
}

// 移除选项
void select_component_remove_item(SelectComponent* component, int index) {
    if (!component || index < 0 || index >= component->item_count) return;
    
    // 释放选项文本和 user_data
    if (component->items[index].text) {
        free(component->items[index].text);
        component->items[index].text = NULL;
    }
    if (component->items[index].user_data) {
        free(component->items[index].user_data);
        component->items[index].user_data = NULL;
    }
    
    // 移动后面的选项
    for (int i = index; i < component->item_count - 1; i++) {
        component->items[i] = component->items[i + 1];
    }
    
    component->item_count--;
    
    // 调整占位符索引
    if (component->placeholder_index > index) {
        component->placeholder_index--;
    } else if (component->placeholder_index == index) {
        component->placeholder_index = -1;
    }
    
    // 调整选中索引
    if (component->selected_index >= component->item_count) {
        component->selected_index = component->item_count - 1;
    }
}

// 清空所有选项
void select_component_clear_items(SelectComponent* component) {
    if (!component) return;
    
    // 释放所有选项文本和 user_data
    if (component->items) {
        for (int i = 0; i < component->item_count; i++) {
            if (component->items[i].text) {
                free(component->items[i].text);
                component->items[i].text = NULL;
            }
            if (component->items[i].user_data) {
                free(component->items[i].user_data);
                component->items[i].user_data = NULL;
            }
        }
        
        // 释放选项数组
        free(component->items);
        component->items = NULL;
    }
    
    component->item_count = 0;
    component->selected_index = -1;
    component->placeholder_index = -1;
    component->scroll_position = 0;
    component->hover_index = -1;
}

// 插入选项
void select_component_insert_item(SelectComponent* component, int index, const char* text, void* user_data) {
    if (!component || !text || index < 0 || index > component->item_count) return;
    
    // 扩展选项数组
    SelectItem* new_items = (SelectItem*)realloc(component->items, (component->item_count + 1) * sizeof(SelectItem));
    if (!new_items) return;
    component->items = new_items;
    
    // 将后面的选项后移
    for (int i = component->item_count; i > index; i--) {
        component->items[i] = component->items[i - 1];
    }
    
    // 插入新选项
    SelectItem* item = &component->items[index];
    item->text = strdup(text);
    item->user_data = user_data ? strdup((const char*)user_data) : NULL;
    item->selected = 0;
    item->disabled = 0;
    
    component->item_count++;
    
    // 调整索引
    if (component->placeholder_index >= index) {
        component->placeholder_index++;
    }
    if (component->selected_index >= index) {
        component->selected_index++;
    }
}

// 设置选中项
void select_component_set_selected(SelectComponent* component, int index) {
    if (!component || index < -1 || index >= component->item_count) return;
    
    // 检查选项是否被禁用
    if (index >= 0 && component->items[index].disabled) return;
    
    int old_index = component->selected_index;
    component->selected_index = index;
    
    // 清除所有选中状态
    for (int i = 0; i < component->item_count; i++) {
        component->items[i].selected = 0;
    }
    
    // 设置新的选中状态
    if (index >= 0) {
        component->items[index].selected = 1;
    }
    
    // 自动收起下拉菜单
    if (component->expanded) {
        select_component_collapse(component);
    }
    
    // 调用回调函数
    if (old_index != index && component->on_selection_changed) {
        component->on_selection_changed(index, component->user_data);
    }
    
    // 触发标准 onChange 事件
    if (old_index != index) {
        select_component_trigger_on_change(component);
    }
}

// 获取选中项索引
int select_component_get_selected(SelectComponent* component) {
    if (!component) return -1;
    return component->selected_index;
}

// 获取选中项文本
const char* select_component_get_selected_text(SelectComponent* component) {
    if (!component || component->selected_index < 0 || component->selected_index >= component->item_count) {
        return NULL;
    }
    return component->items[component->selected_index].text;
}

// 获取选中项值（从 user_data 中获取）
const char* select_component_get_selected_value(SelectComponent* component) {
    if (!component || component->selected_index < 0 || component->selected_index >= component->item_count) {
        return NULL;
    }
    return (const char*)component->items[component->selected_index].user_data;
}

// 获取选中项用户数据
void* select_component_get_selected_data(SelectComponent* component) {
    if (!component || component->selected_index < 0 || component->selected_index >= component->item_count) {
        return NULL;
    }
    return component->items[component->selected_index].user_data;
}

// 清除选择
void select_component_clear_selection(SelectComponent* component) {
    if (!component) return;
    select_component_set_selected(component, component->placeholder_index >= 0 ? component->placeholder_index : -1);
}

// 设置选项禁用状态
void select_component_set_item_disabled(SelectComponent* component, int index, int disabled) {
    if (!component || index < 0 || index >= component->item_count) return;
    
    component->items[index].disabled = disabled;
    
    // 如果禁用了当前选中项，清除选择
    if (disabled && index == component->selected_index) {
        select_component_clear_selection(component);
    }
}

// 检查选项是否禁用
int select_component_is_item_disabled(SelectComponent* component, int index) {
    if (!component || index < 0 || index >= component->item_count) return 0;
    return component->items[index].disabled;
}

// 获取选项数量
int select_component_get_item_count(SelectComponent* component) {
    return component ? component->item_count : 0;
}

// 获取选项文本
const char* select_component_get_item_text(SelectComponent* component, int index) {
    if (!component || index < 0 || index >= component->item_count) return NULL;
    return component->items[index].text;
}

// 设置颜色
void select_component_set_colors(SelectComponent* component, 
                                Color bg, Color text, Color border, Color arrow,
                                Color dropdown_bg, Color hover, Color selected_bg, 
                                Color selected_text, Color disabled_text) {
    if (!component) return;
    
    component->bg_color = bg;
    component->text_color = text;
    component->border_color = border;
    component->arrow_color = arrow;
    component->dropdown_bg_color = dropdown_bg;
    component->hover_bg_color = hover;
    component->selected_bg_color = selected_bg;
    component->selected_text_color = selected_text;
    component->disabled_text_color = disabled_text;
}

void select_component_set_extended_colors(SelectComponent* component,
                                       Color focus_border, Color hover_border,
                                       Color scrollbar, Color scrollbar_bg) {
    if (!component) return;
    
    component->focus_border_color = focus_border;
    component->hover_border_color = hover_border;
    component->scrollbar_color = scrollbar;
    component->scrollbar_bg_color = scrollbar_bg;
}

// 设置边框样式
void select_component_set_border_style(SelectComponent* component, int width, int radius) {
    if (!component) return;
    component->border_width = width;
    component->border_radius = radius;
}

// 设置最大可见选项数
void select_component_set_max_visible_items(SelectComponent* component, int max_visible) {
    if (!component || max_visible <= 0) return;
    component->max_visible_items = max_visible;
}

// 设置字体大小
void select_component_set_font_size(SelectComponent* component, int font_size) {
    if (!component || font_size <= 0) return;
    
    component->font_size = font_size;
    
    // 重新加载字体
    if (component->layer && component->layer->font && strlen(component->layer->font->path) > 0) {
        char font_path[MAX_PATH];
        snprintf(font_path, sizeof(font_path), "%s", component->layer->font->path);
        component->font = backend_load_font(font_path, component->font_size);
    }
}

// 设置用户数据
void select_component_set_user_data(SelectComponent* component, void* data) {
    if (!component) return;
    component->user_data = data;
}

// 设置选择变化回调
void select_component_set_selection_changed_callback(SelectComponent* component, void (*callback)(int, void*)) {
    if (!component) return;
    component->on_selection_changed = callback;
}

// 设置展开状态变化回调
void select_component_set_dropdown_expanded_callback(SelectComponent* component, void (*callback)(int, void*)) {
    if (!component) return;
    component->on_dropdown_expanded = callback;
}

// 展开下拉菜单
void select_component_expand(SelectComponent* component) {
    if (!component || component->expanded || component->item_count == 0) {
        printf("DEBUG: expand early return - component=%p, expanded=%d, item_count=%d\n", 
               component, component ? component->expanded : -1, component ? component->item_count : -1);
        return;
    }
    
    // printf("DEBUG: Starting expansion - component=%p, item_count=%d\n", component, component->item_count);
    component->expanded = 1;
    component->hover_index = -1;
    component->scroll_position = 0;
    
    // 创建下拉菜单弹出层
    if (!component->dropdown_layer) {
        component->dropdown_layer = malloc(sizeof(Layer));
        if (component->dropdown_layer) {
            memset(component->dropdown_layer, 0, sizeof(Layer));
            
            // 设置弹出层位置和大小
            component->dropdown_layer->rect.x = component->layer->rect.x;
            
            // 计算下拉菜单高度
            int dropdown_height = component->item_height * component->item_count;
            if (dropdown_height > component->max_visible_items * component->item_height) {
                dropdown_height = component->max_visible_items * component->item_height;
            }
            component->dropdown_layer->rect.w = component->layer->rect.w;
            component->dropdown_layer->rect.h = dropdown_height;
            
            // 智能判断展开方向：检查下方是否有足够空间
            int window_width, window_height;
            backend_get_windowsize(&window_width, &window_height);
            
            int space_below = window_height - (component->layer->rect.y + component->layer->rect.h);
            int space_above = component->layer->rect.y;
            
            // 判断是否有足够空间向下展开
            if (space_below >= dropdown_height || space_below >= space_above) {
                // 向下展开
                component->dropdown_open_upward = 0;
                component->dropdown_layer->rect.y = component->layer->rect.y + component->layer->rect.h;
                printf("DEBUG: Dropdown opening DOWNWARD - space_below=%d, dropdown_height=%d\n", space_below, dropdown_height);
            } else {
                // 向上展开
                component->dropdown_open_upward = 1;
                component->dropdown_layer->rect.y = component->layer->rect.y - dropdown_height;
                printf("DEBUG: Dropdown opening UPWARD - space_below=%d, space_above=%d, dropdown_height=%d\n", 
                       space_below, space_above, dropdown_height);
            }
            
            // 设置渲染函数为独立的下拉渲染函数
            component->dropdown_layer->component = component;
            component->dropdown_layer->render = select_component_render_dropdown_only;
            component->dropdown_layer->handle_mouse_event = select_component_handle_dropdown_mouse_event;
            component->dropdown_layer->handle_key_event = select_component_handle_dropdown_key_event;
            component->dropdown_layer->handle_scroll_event = select_component_handle_dropdown_scroll_event;
        }
    }
    
    // 设置刚展开标志，防止立即关闭
    component->just_expanded = 1;
    
    // 添加到弹出层管理器
    if (component->dropdown_layer) {
        PopupLayer* popup = popup_layer_create(component->dropdown_layer, POPUP_TYPE_DROPDOWN, 100);
        if (popup) {
            popup->auto_close = false;  // 临时禁用自动关闭
            popup->close_callback = select_component_popup_close_callback;
            popup_manager_add(popup);
            printf("DEBUG: Added select dropdown to popup manager\n");
        }
    }
    
    if (component->on_dropdown_expanded) {
        component->on_dropdown_expanded(1, component->user_data);
    }
}

// 收起下拉菜单
void select_component_collapse(SelectComponent* component) {
    // printf("DEBUG: collapse called - component=%p, expanded=%d\n", component, component ? component->expanded : -1);
    if (!component || !component->expanded) return;
    
    // 先设置状态，避免在 popup 移除过程中出现状态不一致
    component->expanded = 0;
    component->hover_index = -1;
    component->is_dragging = 0;
    component->just_expanded = 0;
    
    // 触发回调
    if (component->on_dropdown_expanded) {
        component->on_dropdown_expanded(0, component->user_data);
    }
    
    // 从弹出层管理器移除
    if (component->dropdown_layer) {
        // 保存指针以便调用 popup_manager_remove
        Layer* dropdown_layer = component->dropdown_layer;
        component->dropdown_layer = NULL;  // 先设置为NULL，防止回调中再次使用
        
        popup_manager_remove(dropdown_layer);
        // 注意：dropdown_layer 会在 popup_manager_remove 中被释放，close_callback 会设置为 NULL
        printf("DEBUG: Removed select dropdown from popup manager\n");
    }
}

// 切换展开状态
void select_component_toggle(SelectComponent* component) {
    if (!component) return;
    
    if (component->expanded) {
        select_component_collapse(component);
    } else {
        select_component_expand(component);
    }
}

// 处理鼠标事件
void select_component_handle_mouse_event(Layer* layer, MouseEvent* event) {
    if (!layer || !event || !layer->component) return;
    
    SelectComponent* component = (SelectComponent*)layer->component;
    

    
    // 如果正在拖拽，优先处理拖拽逻辑
    if (component->is_dragging && (event->state == SDL_MOUSEMOTION || event->state == SDL_PRESSED)) {
        printf("DEBUG: Dragging scrollbar, mouse_y=%d, start_y=%d, start_scroll=%d\n", 
               event->y, component->drag_start_y, component->drag_start_scroll);
        
        int dropdown_y;
        int dropdown_height = component->item_height * component->item_count;
        if (dropdown_height > component->max_visible_items * component->item_height) {
            dropdown_height = component->max_visible_items * component->item_height;
        }
        
        if (component->dropdown_open_upward) {
            dropdown_y = layer->rect.y - dropdown_height;
        } else {
            dropdown_y = layer->rect.y + layer->rect.h;
        }
        if (dropdown_height > component->max_visible_items * component->item_height) {
            dropdown_height = component->max_visible_items * component->item_height;
        }
        
        int total_items = component->item_count;
        int visible_items = component->max_visible_items;
        int track_height = dropdown_height;
        
        int mouse_delta = event->y - component->drag_start_y;
        int scroll_delta = (mouse_delta * (total_items - visible_items)) / track_height;
        
        int new_scroll = component->drag_start_scroll + scroll_delta;
        
        if (new_scroll < 0) new_scroll = 0;
        if (new_scroll > total_items - visible_items) new_scroll = total_items - visible_items;
        
        printf("DEBUG: mouse_delta=%d, scroll_delta=%d, new_scroll=%d\n", 
               mouse_delta, scroll_delta, new_scroll);
        
        component->scroll_position = new_scroll;
        return;
    }
    
    // 检查鼠标是否在 Select 区域内（用于悬停状态）
    bool in_select_area = (event->x >= layer->rect.x && event->x < layer->rect.x + layer->rect.w &&
                            event->y >= layer->rect.y && event->y < layer->rect.y + layer->rect.h);
    
    // 如果展开了下拉菜单，还要检查是否在下拉菜单区域内
    bool in_dropdown_area = false;
    if (component->expanded) {
        int dropdown_x = layer->rect.x;
        int dropdown_y;
        int dropdown_height = component->item_height * component->item_count;
        if (dropdown_height > component->max_visible_items * component->item_height) {
            dropdown_height = component->max_visible_items * component->item_height;
        }
        
        if (component->dropdown_open_upward) {
            dropdown_y = layer->rect.y - dropdown_height;
        } else {
            dropdown_y = layer->rect.y + layer->rect.h;
        }
        if (dropdown_height > component->max_visible_items * component->item_height) {
            dropdown_height = component->max_visible_items * component->item_height;
        }
        
        in_dropdown_area = (event->x >= dropdown_x && event->x < dropdown_x + layer->rect.w &&
                           event->y >= dropdown_y && event->y < dropdown_y + dropdown_height);
    }
    
    if (in_select_area || in_dropdown_area) {
        SET_STATE(layer, LAYER_STATE_HOVER);
    } else {
        CLEAR_STATE(layer, LAYER_STATE_HOVER);
    }
    
    if (event->state == SDL_PRESSED && event->button == SDL_BUTTON_LEFT) {
        // 检查是否点击在 Select 按钮上
        if (event->x >= layer->rect.x && event->x < layer->rect.x + layer->rect.w &&
            event->y >= layer->rect.y && event->y < layer->rect.y + layer->rect.h) {
            // 如果刚刚展开，不要立即关闭
            if (component->just_expanded) {
                component->just_expanded = 0;
                return;  // 不处理toggle，只是清除标志
            }
            select_component_toggle(component);
        }
        // 检查是否点击在下拉菜单选项上
        else if (component->expanded) {
            int dropdown_x = layer->rect.x;
            int dropdown_y;
            int dropdown_height = component->item_height * component->item_count;
            if (dropdown_height > component->max_visible_items * component->item_height) {
                dropdown_height = component->max_visible_items * component->item_height;
            }
            
            if (component->dropdown_open_upward) {
                dropdown_y = layer->rect.y - dropdown_height;
            } else {
                dropdown_y = layer->rect.y + layer->rect.h;
            }
            if (dropdown_height > component->max_visible_items * component->item_height) {
                dropdown_height = component->max_visible_items * component->item_height;
            }
            
            // 计算内容区域（排除滚动条）
            int content_width = layer->rect.w;
            int has_scrollbar = component->item_count > component->max_visible_items;
            if (has_scrollbar) {
                content_width -= component->scrollbar_width;
            }
            
            // 检查是否点击在下拉菜单内容区域
            if (event->x >= dropdown_x && event->x < dropdown_x + content_width &&
                event->y >= dropdown_y && event->y < dropdown_y + dropdown_height) {
                int clicked_index = (event->y - dropdown_y) / component->item_height + component->scroll_position;
                
                if (clicked_index >= 0 && clicked_index < component->item_count) {
                    printf("DEBUG: Main dropdown item %d clicked: %s\n", clicked_index, component->items[clicked_index].text);
                    if (!component->items[clicked_index].disabled) {
                        select_component_set_selected(component, clicked_index);
                    }
                }
            }
            // 检查是否点击在滚动条区域
            else if (has_scrollbar && 
                     event->x >= dropdown_x + content_width && 
                     event->x < dropdown_x + layer->rect.w &&
                     event->y >= dropdown_y && 
                     event->y < dropdown_y + dropdown_height) {
                
                // 计算滚动条滑块位置和大小
                int total_items = component->item_count;
                int visible_items = component->max_visible_items;
                int track_height = dropdown_height;
                int thumb_height = (track_height * visible_items) / total_items;
                if (thumb_height < 20) thumb_height = 20; // 最小高度
                
                int max_thumb_y = track_height - thumb_height;
                int thumb_y;
                if (total_items > visible_items) {
                    thumb_y = dropdown_y + (component->scroll_position * max_thumb_y) / (total_items - visible_items);
                } else {
                    thumb_y = dropdown_y;
                }
                
                // 检查是否点击在滑块上
                int scrollbar_x = dropdown_x + content_width;
                Rect thumb_rect = {scrollbar_x + 2, thumb_y, component->scrollbar_width - 4, thumb_height};
                
                printf("DEBUG: Click check - event_x=%d, event_y=%d\n", event->x, event->y);
                printf("DEBUG: Thumb rect - x=%d, y=%d, w=%d, h=%d\n", 
                       thumb_rect.x, thumb_rect.y, thumb_rect.w, thumb_rect.h);
                
                if (event->x >= thumb_rect.x && event->x < thumb_rect.x + thumb_rect.w &&
                    event->y >= thumb_rect.y && event->y < thumb_rect.y + thumb_rect.h) {
                    // 点击在滑块上，开始拖拽
                    component->is_dragging = 1;
                    component->drag_start_y = event->y;
                    component->drag_start_scroll = component->scroll_position;
                    printf("DEBUG: Started dragging scrollbar at y=%d, scroll=%d, thumb_rect=(%d,%d,%d,%d)\n", 
                           event->y, component->scroll_position, thumb_rect.x, thumb_rect.y, thumb_rect.w, thumb_rect.h);
                } else {
                    // 点击在滑块外，滚动到点击位置
                    int click_y = event->y - dropdown_y;
                    int new_scroll = (click_y * (total_items - visible_items)) / track_height;
                    
                    if (new_scroll < 0) new_scroll = 0;
                    if (new_scroll > total_items - visible_items) new_scroll = total_items - visible_items;
                    
                    component->scroll_position = new_scroll;
                    component->is_dragging = 1;
                    component->drag_start_y = event->y;
                    component->drag_start_scroll = component->scroll_position;
                    printf("DEBUG: Clicked scrollbar track, scrolled to %d\n", new_scroll);
                }
            }
        }
    } else if (event->state == SDL_RELEASED && event->button == SDL_BUTTON_LEFT) {
        // 停止拖拽
        if (component->is_dragging) {
            component->is_dragging = 0;
        }
    } else if (event->state == SDL_MOUSEMOTION) {
        // printf("DEBUG: Mouse motion - x=%d, y=%d, is_dragging=%d\n", 
            //    event->x, event->y, component->is_dragging);
        
        if (component->expanded) {
            // 更新悬停状态
            int dropdown_x = layer->rect.x;
            int dropdown_y;
            int dropdown_height = component->item_height * component->item_count;
            if (dropdown_height > component->max_visible_items * component->item_height) {
                dropdown_height = component->max_visible_items * component->item_height;
            }
            
            if (component->dropdown_open_upward) {
                dropdown_y = layer->rect.y - dropdown_height;
            } else {
                dropdown_y = layer->rect.y + layer->rect.h;
            }
            
            int content_width = layer->rect.w;
            int has_scrollbar = component->item_count > component->max_visible_items;
            if (has_scrollbar) {
                content_width -= component->scrollbar_width;
            }
            
            if (event->x >= dropdown_x && event->x < dropdown_x + content_width) {
                int hover_index = (event->y - dropdown_y) / component->item_height + component->scroll_position;
                if (hover_index >= 0 && hover_index < component->item_count) {
                    component->hover_index = hover_index;
                } else {
                    component->hover_index = -1;
                }
            } else {
                component->hover_index = -1;
            }
        }
    }
}

// 处理滚动事件
void select_component_handle_scroll_event(Layer* layer, int scroll_delta) {
    if (!layer || !layer->component) return;
    
    SelectComponent* component = (SelectComponent*)layer->component;
    
    if (component->expanded) {
        printf("DEBUG: Select scroll event - delta=%d, current_scroll=%d\n", 
               scroll_delta, component->scroll_position);
        
        int total_items = component->item_count;
        int visible_items = component->max_visible_items;
        
        if (total_items > visible_items) {
            int new_scroll = component->scroll_position + scroll_delta;
            
            if (new_scroll < 0) new_scroll = 0;
            if (new_scroll > total_items - visible_items) new_scroll = total_items - visible_items;
            
            component->scroll_position = new_scroll;
            printf("DEBUG: Scrolled to position %d\n", new_scroll);
        }
    }
}

// 滚动事件回调函数（用于 layer->event->scroll）
void select_component_scroll_callback(Layer* layer) {
    if (!layer || !layer->component) return;
    
    SelectComponent* component = (SelectComponent*)layer->component;
    if (component->expanded) {
        printf("DEBUG: Select scroll callback triggered\n");
        // 这个回调主要用于重新渲染
    }
}

// 处理键盘事件
void select_component_handle_key_event(Layer* layer, KeyEvent* event) {
    if (!layer || !event || !layer->component) return;
    
    SelectComponent* component = (SelectComponent*)layer->component;
    
    if (event->type == KEY_EVENT_DOWN) {
        switch (event->data.key.key_code) {
            case SDLK_UP:
                if (component->expanded) {
                    // 在展开状态下向上导航
                    for (int i = component->hover_index - 1; i >= 0; i--) {
                        if (!component->items[i].disabled) {
                            component->hover_index = i;
                            break;
                        }
                    }
                } else {
                    // 在收起状态下切换到上一个选项
                    for (int i = component->selected_index - 1; i >= 0; i--) {
                        if (!component->items[i].disabled) {
                            select_component_set_selected(component, i);
                            break;
                        }
                    }
                }
                break;
                
            case SDLK_DOWN:
                if (component->expanded) {
                    // 在展开状态下向下导航
                    for (int i = component->hover_index + 1; i < component->item_count; i++) {
                        if (!component->items[i].disabled) {
                            component->hover_index = i;
                            break;
                        }
                    }
                } else {
                    // 在收起状态下切换到下一个选项
                    for (int i = component->selected_index + 1; i < component->item_count; i++) {
                        if (!component->items[i].disabled) {
                            select_component_set_selected(component, i);
                            break;
                        }
                    }
                }
                break;
                
            case SDLK_RETURN:
            case SDLK_SPACE:
                if (component->expanded && component->hover_index >= 0 && 
                    component->hover_index < component->item_count && 
                    !component->items[component->hover_index].disabled) {
                    select_component_set_selected(component, component->hover_index);
                } else {
                    select_component_toggle(component);
                }
                break;
                
            case SDLK_ESCAPE:
                select_component_collapse(component);
                break;
        }
    }
}

// 渲染 Select 组件
void select_component_render(Layer* layer) {
    if (!layer || !layer->component) return;
    
    SelectComponent* component = (SelectComponent*)layer->component;

    
    // 绘制 Select 背景和边框
    Rect rect = {layer->rect.x, layer->rect.y, layer->rect.w, layer->rect.h};
    
    // 使用组件自己的背景色，这样确保不会被透明的 layer 背景色覆盖
    Color bg_color = component->bg_color;
    
    // 根据状态选择边框颜色
    Color border_color = component->border_color;
    if (layer->state == LAYER_STATE_FOCUSED) {
        border_color = component->focus_border_color;
    } else if (layer->state == LAYER_STATE_HOVER) {
        border_color = component->hover_border_color;
    } else if (component->expanded) {
        border_color = component->focus_border_color;  // 展开时使用焦点颜色
    }
    
    if (component->border_radius > 0) {
        backend_render_rounded_rect_with_border(&rect, bg_color, component->border_radius, 
                                                 component->border_width, border_color);
    } else {
        backend_render_rounded_rect_with_border(&rect, bg_color, 0, 
                                                 component->border_width, border_color);
    }
    
    // 绘制选中的文本
    if (component->selected_index >= 0 && component->selected_index < component->item_count) {
        SelectItem* item = &component->items[component->selected_index];
        
        // 使用占位符文本颜色或普通文本颜色
        Color text_color = (component->selected_index == component->placeholder_index) ? 
                           component->disabled_text_color : component->text_color;
        

        
        // 使用组件专用字体渲染文本
        DFont* font_to_use = component->font;
        if (!font_to_use && layer->font) {
            font_to_use = layer->font->default_font;
        }
        
        if (!font_to_use) {
            return;
        }
        
        Texture* text_texture = backend_render_texture(font_to_use, item->text, text_color);
        if (text_texture) {
            int text_width, text_height;
            backend_query_texture(text_texture, NULL, NULL, &text_width, &text_height);
            
            // 文本居中垂直对齐，左对齐
            Rect text_rect = {
                layer->rect.x + 12,
                layer->rect.y + (layer->rect.h - text_height / scale) / 2,
                text_width / scale,
                text_height / scale
            };
            
            // 文本裁剪
            int max_text_width = layer->rect.w - 40; // 为箭头留出空间
            if (text_width / scale > max_text_width) {
                text_rect.w = max_text_width;
            }
            
            backend_render_text_copy(text_texture, NULL, &text_rect);
            backend_render_text_destroy(text_texture);
        }
    }
    
    // 绘制下拉箭头
    int arrow_size = 8;
    int arrow_x = layer->rect.x + layer->rect.w - 20;
    int arrow_y = layer->rect.y + (layer->rect.h - arrow_size) / 2;
    
    // 根据展开状态绘制不同方向的箭头
    if (component->expanded) {
        // 向上箭头
        for (int i = 0; i < arrow_size; i++) {
            int bar_width = (i + 1) * 2;
            if (bar_width > 12) bar_width = 12;
            
            Rect bar_rect = {
                arrow_x + (12 - bar_width) / 2,
                arrow_y + arrow_size - 1 - i,
                bar_width,
                1
            };
            backend_render_rect(&bar_rect, component->arrow_color);
        }
    } else {
        // 向下箭头
        for (int i = 0; i < arrow_size; i++) {
            int bar_width = (i + 1) * 2;
            if (bar_width > 12) bar_width = 12;
            
            Rect bar_rect = {
                arrow_x + (12 - bar_width) / 2,
                arrow_y + i,
                bar_width,
                1
            };
            backend_render_rect(&bar_rect, component->arrow_color);
        }
    }
}

// 只渲染下拉菜单部分（用于弹出层）
void select_component_render_dropdown_only(Layer* layer) {
    if (!layer || !layer->component) return;
    
    SelectComponent* component = (SelectComponent*)layer->component;
    
    if (!component->expanded || component->item_count == 0) {
        printf("DEBUG: Dropdown render early return - expanded=%d, item_count=%d\n", 
               component->expanded, component->item_count);
        return;
    }
    

    
    int dropdown_x = component->layer->rect.x;
    int dropdown_y;
    
    // 根据展开方向确定Y坐标
    if (component->dropdown_open_upward) {
        // 向上展开：计算高度并从select组件上方开始
        int dropdown_height = component->item_height * component->item_count;
        if (dropdown_height > component->max_visible_items * component->item_height) {
            dropdown_height = component->max_visible_items * component->item_height;
        }
        dropdown_y = component->layer->rect.y - dropdown_height;
        // printf("DEBUG: Rendering dropdown UPWARD at y=%d\n", dropdown_y);
    } else {
        // 向下展开：从select组件下方开始
        dropdown_y = component->layer->rect.y + component->layer->rect.h;
        // printf("DEBUG: Rendering dropdown DOWNWARD at y=%d\n", dropdown_y);
    }
    
    // 计算下拉菜单高度
    int dropdown_height = component->item_height * component->item_count;
    int visible_count = component->item_count;
    int has_scrollbar = 0;
    
    if (dropdown_height > component->max_visible_items * component->item_height) {
        dropdown_height = component->max_visible_items * component->item_height;
        visible_count = component->max_visible_items;
        has_scrollbar = 1;
    }
    
    int content_width = component->layer->rect.w;
    if (has_scrollbar) {
        // content_width += component->scrollbar_width;
    }
    
      // 设置裁剪区域
    Rect clip_rect = {dropdown_x, dropdown_y, content_width, dropdown_height};
    Rect prev_clip;
    backend_render_get_clip_rect(&prev_clip);
    backend_render_set_clip_rect(&clip_rect);

    // 绘制下拉菜单阴影
    Color shadow_color = {0, 0, 0, 120}; // 更深的阴影表示层级更高
    Rect shadow_rect = {dropdown_x + 3, dropdown_y + 3, component->layer->rect.w, dropdown_height};
    backend_render_rounded_rect(&shadow_rect, shadow_color, component->border_radius);
    
    // 绘制下拉菜单背景和边框
    Rect dropdown_rect = {dropdown_x, dropdown_y, component->layer->rect.w, dropdown_height};
    backend_render_rounded_rect_with_border(&dropdown_rect, component->dropdown_bg_color, component->border_radius,
                                             component->border_width, component->border_color);
    
    // 绘制可见选项
    for (int i = 0; i < visible_count; i++) {
        int item_index = i + component->scroll_position;
        
        if (item_index >= 0 && item_index < component->item_count) {
            SelectItem* item = &component->items[item_index];
            int item_y = dropdown_y + i * component->item_height;
            
            // 绘制选项背景
            Rect item_rect = {dropdown_x + 1, item_y, content_width - 2, component->item_height};
            
            if (item->selected) {
                backend_render_fill_rect(&item_rect, component->selected_bg_color);
            } else if (item_index == component->hover_index && !item->disabled) {
                backend_render_fill_rect(&item_rect, component->hover_bg_color);
            }
            
            // 绘制选项文本
            Color text_color;
            if (item->disabled) {
                text_color = component->disabled_text_color;
            } else if (item->selected) {
                text_color = component->selected_text_color;
            } else {
                text_color = component->text_color;
            }
            
            // 使用组件专用字体渲染文本
            DFont* font_to_use = component->font;
            if (!font_to_use && component->layer->font) {
                font_to_use = component->layer->font->default_font;
            }
            if (!font_to_use) return;
            
            Texture* text_texture = backend_render_texture(font_to_use, item->text, text_color);
            if (text_texture) {
                int text_width, text_height;
                backend_query_texture(text_texture, NULL, NULL, &text_width, &text_height);
                
                Rect text_rect = {
                    dropdown_x + 12,
                    item_y + (component->item_height - text_height / scale) / 2,
                    text_width / scale,
                    text_height / scale
                };
                
                // 文本裁剪
                if (text_rect.x + text_rect.w > dropdown_x + content_width - 5) {
                    text_rect.w = dropdown_x + content_width - 5 - text_rect.x;
                    if (text_rect.w > 0) {
                        backend_render_text_copy(text_texture, NULL, &text_rect);
                    }
                } else {
                    backend_render_text_copy(text_texture, NULL, &text_rect);
                }
                
                backend_render_text_destroy(text_texture);
            }
            
            // 绘制选项分割线
            Color line_color = {220, 220, 220, 255};
            Rect line_rect = {dropdown_x + 1, item_y + component->item_height - 1, content_width - 2, 1};
            backend_render_rect(&line_rect, line_color);
        }
    }
    
    
    // 绘制滚动条
    if (has_scrollbar) {
        int scrollbar_x = dropdown_x + component->layer->rect.w - component->scrollbar_width;
        
        // 绘制滚动条背景
        Rect scrollbar_bg_rect = {scrollbar_x, dropdown_y, component->scrollbar_width, dropdown_height};
        
        backend_render_fill_rect(&scrollbar_bg_rect, component->scrollbar_bg_color);
        
        // 计算滚动条滑块位置和大小
        int total_items = component->item_count;
        int visible_items = visible_count;
        int track_height = dropdown_height;
        int thumb_height = (track_height * visible_items) / total_items;
        if (thumb_height < 20) thumb_height = 20; // 最小高度
        
        int max_thumb_y = track_height - thumb_height;
        int thumb_y;
        if (total_items > visible_items) {
            thumb_y = dropdown_y + (component->scroll_position * max_thumb_y) / (total_items - visible_items);
        } else {
            thumb_y = dropdown_y;
        }
        
        // 绘制滚动条滑块
        Rect thumb_rect = {scrollbar_x + 2, thumb_y, component->scrollbar_width - 4, thumb_height};
        
        backend_render_fill_rect(&thumb_rect, component->scrollbar_color);
    }

    // 清除裁剪区域
    backend_render_set_clip_rect(&prev_clip);
}

// 弹出层专用鼠标事件处理
void select_component_handle_dropdown_mouse_event(Layer* layer, MouseEvent* event) {
    if (!layer || !event || !layer->component) return;
    
    SelectComponent* component = (SelectComponent*)layer->component;
    
    // 计算下拉菜单Y坐标（根据展开方向）
    int dropdown_y;
    int dropdown_height = component->item_height * component->item_count;
    if (dropdown_height > component->max_visible_items * component->item_height) {
        dropdown_height = component->max_visible_items * component->item_height;
    }
    
    if (component->dropdown_open_upward) {
        dropdown_y = component->layer->rect.y - dropdown_height;
    } else {
        dropdown_y = component->layer->rect.y + component->layer->rect.h;
    }
    
    // 如果正在拖拽，优先处理拖拽逻辑
    if (component->is_dragging && (event->state == SDL_MOUSEMOTION || event->state == SDL_PRESSED)) {
        // printf("DEBUG: Dragging scrollbar, mouse_y=%d, start_y=%d, start_scroll=%d\n", 
        //       event->y, component->drag_start_y, component->drag_start_scroll);
        
        int total_items = component->item_count;
        int visible_items = component->max_visible_items;
        int track_height = dropdown_height;
        
        int mouse_delta = event->y - component->drag_start_y;
        int scroll_delta = (mouse_delta * (total_items - visible_items)) / track_height;
        
        int new_scroll = component->drag_start_scroll + scroll_delta;
        
        if (new_scroll < 0) new_scroll = 0;
        if (new_scroll > total_items - visible_items) new_scroll = total_items - visible_items;
        
        component->scroll_position = new_scroll;
        return;
    }
    
    int dropdown_x = component->layer->rect.x;
    
    // 计算内容区域（排除滚动条）
    int content_width = component->layer->rect.w;
    int has_scrollbar = component->item_count > component->max_visible_items;
    if (has_scrollbar) {
        content_width -= component->scrollbar_width;
    }
    
    if (event->state == SDL_PRESSED && event->button == SDL_BUTTON_LEFT) {
        // 检查是否点击在下拉菜单内容区域
        if (event->x >= dropdown_x && event->x < dropdown_x + content_width &&
            event->y >= dropdown_y && event->y < dropdown_y + dropdown_height) {
                int clicked_index = (event->y - dropdown_y) / component->item_height + component->scroll_position;
                
                if (clicked_index >= 0 && clicked_index < component->item_count) {
                    printf("DEBUG: Dropdown item %d clicked: %s\n", clicked_index, component->items[clicked_index].text);
                    if (!component->items[clicked_index].disabled) {
                        select_component_set_selected(component, clicked_index);
                    }
                }
        }
        // 检查是否点击在滚动条区域
        else if (has_scrollbar && 
                 event->x >= dropdown_x + content_width && 
                 event->x < dropdown_x + component->layer->rect.w &&
                 event->y >= dropdown_y && 
                 event->y < dropdown_y + dropdown_height) {
            
            // 计算滚动条滑块位置和大小
            int total_items = component->item_count;
            int visible_items = component->max_visible_items;
            int track_height = dropdown_height;
            int thumb_height = (track_height * visible_items) / total_items;
            if (thumb_height < 20) thumb_height = 20; // 最小高度
            
            int max_thumb_y = track_height - thumb_height;
            int thumb_y;
            if (total_items > visible_items) {
                thumb_y = dropdown_y + (component->scroll_position * max_thumb_y) / (total_items - visible_items);
            } else {
                thumb_y = dropdown_y;
            }
            
            // 检查是否点击在滑块上
            int scrollbar_x = dropdown_x + content_width;
            Rect thumb_rect = {scrollbar_x + 2, thumb_y, component->scrollbar_width - 4, thumb_height};
            
            if (event->x >= thumb_rect.x && event->x < thumb_rect.x + thumb_rect.w &&
                event->y >= thumb_rect.y && event->y < thumb_rect.y + thumb_rect.h) {
                // 点击在滑块上，开始拖拽
                component->is_dragging = 1;
                component->drag_start_y = event->y;
                component->drag_start_scroll = component->scroll_position;
            } else {
                // 点击在滑块外，滚动到点击位置
                int click_y = event->y - dropdown_y;
                int new_scroll = (click_y * (total_items - visible_items)) / track_height;
                
                if (new_scroll < 0) new_scroll = 0;
                if (new_scroll > total_items - visible_items) new_scroll = total_items - visible_items;
                
                component->scroll_position = new_scroll;
                component->is_dragging = 1;
                component->drag_start_y = event->y;
                component->drag_start_scroll = component->scroll_position;
            }
        }
    } else if (event->state == SDL_RELEASED && event->button == SDL_BUTTON_LEFT) {
        // 停止拖拽
        if (component->is_dragging) {
            component->is_dragging = 0;
        }
    } else if (event->state == SDL_MOUSEMOTION) {
        if (component->expanded) {
            // 更新悬停状态
            if (event->x >= dropdown_x && event->x < dropdown_x + content_width) {
                int hover_index = (event->y - dropdown_y) / component->item_height + component->scroll_position;
                if (hover_index >= 0 && hover_index < component->item_count) {
                    component->hover_index = hover_index;
                } else {
                    component->hover_index = -1;
                }
            } else {
                component->hover_index = -1;
            }
        }
    }
}

// 弹出层专用键盘事件处理
void select_component_handle_dropdown_key_event(Layer* layer, KeyEvent* event) {
    if (!layer || !event || !layer->component) return;
    
    SelectComponent* component = (SelectComponent*)layer->component;
    
    if (event->type == KEY_EVENT_DOWN) {
        switch (event->data.key.key_code) {
            case SDLK_UP:
                // 在展开状态下向上导航
                for (int i = component->hover_index - 1; i >= 0; i--) {
                    if (!component->items[i].disabled) {
                        component->hover_index = i;
                        break;
                    }
                }
                break;
                
            case SDLK_DOWN:
                // 在展开状态下向下导航
                for (int i = component->hover_index + 1; i < component->item_count; i++) {
                    if (!component->items[i].disabled) {
                        component->hover_index = i;
                        break;
                    }
                }
                break;
                
            case SDLK_RETURN:
            case SDLK_SPACE:
                if (component->hover_index >= 0 && 
                    component->hover_index < component->item_count && 
                    !component->items[component->hover_index].disabled) {
                    select_component_set_selected(component, component->hover_index);
                }
                break;
                
            case SDLK_ESCAPE:
                select_component_collapse(component);
                break;
        }
    }
}

// 弹出层专用滚动事件处理
void select_component_handle_dropdown_scroll_event(Layer* layer, int scroll_delta) {
    if (!layer || !layer->component) return;
    
    SelectComponent* component = (SelectComponent*)layer->component;
    
    int total_items = component->item_count;
    int visible_items = component->max_visible_items;
    
    if (total_items > visible_items) {
        int new_scroll = component->scroll_position + scroll_delta;
        
        if (new_scroll < 0) new_scroll = 0;
        if (new_scroll > total_items - visible_items) new_scroll = total_items - visible_items;
        
        component->scroll_position = new_scroll;
    }
}

// 弹出层关闭回调
void select_component_popup_close_callback(PopupLayer* popup) {
    if (!popup) return;
    
    // 先保存layer指针，因为在popup_manager_remove中，popup会被释放
    Layer* layer = popup->layer;
    if (!layer) return;
    
    SelectComponent* component = (SelectComponent*)layer->component;
    if (component) {
        // 确保状态一致，但避免重复触发回调
        component->expanded = 0;
        component->hover_index = -1;
        component->is_dragging = 0;
        component->just_expanded = 0;
        
        // 注意：dropdown_layer 会在 popup_manager 中被释放，我们只需要设置为 NULL
        // 注意：不要在这里再次设置为NULL，因为在collapse函数中已经设置过了
        // component->dropdown_layer = NULL;
        
        // 注意：不在这里触发 on_dropdown_expanded 回调，因为它应该已经在 collapse 中被调用了
    }
}

// 设置 onChange 回调函数
void select_component_set_on_change(SelectComponent* component, EventHandler callback, void* user_data) {
    if (!component) {
        return;
    }
    
    component->on_change = callback;
    component->change_name = (char*)user_data;
}

// 调用 onChange 回调函数
void select_component_trigger_on_change(SelectComponent* component) {
    if (!component) {
        return;
    }
    
    // 如果没有事件处理器但有事件名称，尝试查找事件处理器
    if(component->on_change == NULL && component->change_name != NULL){
        EventHandler handler = find_event_by_name(component->change_name);
        component->on_change = handler;
    }
    
    // 检查是否有可用的事件处理器
    if (component->on_change) {
        // 调用事件处理器
        component->on_change(component->layer);
    } else if (component->change_name) {
        // 只有在指定了事件名称但找不到处理器时才打印警告
        printf("select_component_trigger_on_change not found onchange event %s\n", component->change_name);
        print_registered_events();
    }
    // 如果既没有处理器也没有事件名称，则静默处理
}

// 注册事件处理函数
int select_component_register_event(Layer* layer, const char* event_name, const char* event_func_name, EventHandler event_handler){
    if(strcmp(event_name,"change")==0 || strcmp(event_name,"onChange")==0){
        SelectComponent* component = (SelectComponent*)layer->component;
        component->on_change = event_handler;
        component->change_name = strdup(event_func_name);
        return 0;
    }
    return -1;
}

// 通用属性获取函数
cJSON* select_component_get_property(Layer* layer, const char* property_name) {
    if (!layer || !property_name || !layer->component) {
        return NULL;
    }
    
    SelectComponent* component = (SelectComponent*)layer->component;
    
    if (strcmp(property_name, "value") == 0) {
        // 获取选中项的值
        const char* value = select_component_get_selected_value(component);
        if (value) {
            return cJSON_CreateString(value);
        }
        return cJSON_CreateNull();
    }
    else if (strcmp(property_name, "text") == 0) {
        // 获取选中项的文本
        const char* text = select_component_get_selected_text(component);
        if (text) {
            return cJSON_CreateString(text);
        }
        return cJSON_CreateNull();
    }
    else if (strcmp(property_name, "selectedIndex") == 0) {
        // 获取选中项的索引
        return cJSON_CreateNumber(select_component_get_selected(component));
    }
    else if (strcmp(property_name, "itemCount") == 0) {
        // 获取选项数量
        return cJSON_CreateNumber(select_component_get_item_count(component));
    }
    
    // 未知的属性，返回 NULL
    return NULL;
}