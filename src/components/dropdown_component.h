#ifndef YUI_DROPDOWN_COMPONENT_H
#define YUI_DROPDOWN_COMPONENT_H

#include "../ytype.h"

// 下拉菜单项目结构体
typedef struct {
    char* text;           // 项目文本
    void* user_data;      // 用户数据
    int selected;         // 是否选中
} DropdownItem;

// 下拉菜单组件结构体
typedef struct {
    Layer* layer;             // 关联的图层
    Layer* dropdown_layer;    // 下拉菜单图层
    DropdownItem* items;      // 项目列表
    int item_count;           // 项目数量
    int selected_index;       // 当前选中项索引
    int expanded;             // 是否展开
    int max_visible_items;    // 最大可见项目数
    Color bg_color;           // 背景颜色
    Color text_color;         // 文本颜色
    Color arrow_color;        // 箭头颜色
    Color dropdown_bg_color;  // 下拉菜单背景颜色
    Color selected_bg_color;  // 选中项背景颜色
    Color selected_text_color;// 选中项文本颜色
    void* user_data;          // 用户数据
    
    // 回调函数
    void (*on_selection_changed)(int index, void* user_data);
} DropdownComponent;

// 函数声明
DropdownComponent* dropdown_component_create(Layer* layer);
void dropdown_component_destroy(DropdownComponent* component);
void dropdown_component_add_item(DropdownComponent* component, const char* text, void* user_data);
void dropdown_component_remove_item(DropdownComponent* component, int index);
void dropdown_component_clear_items(DropdownComponent* component);
void dropdown_component_set_selected(DropdownComponent* component, int index);
int dropdown_component_get_selected(DropdownComponent* component);
void dropdown_component_set_colors(DropdownComponent* component, Color bg, Color text, Color arrow, 
                                  Color dropdown_bg, Color selected_bg, Color selected_text);
void dropdown_component_set_user_data(DropdownComponent* component, void* data);
void dropdown_component_set_selection_changed_callback(DropdownComponent* component, void (*callback)(int, void*));
void dropdown_component_handle_mouse_event(Layer* layer, MouseEvent* event);
void dropdown_component_handle_key_event(Layer* layer, KeyEvent* event);
void dropdown_component_render(Layer* layer);

#endif  // YUI_DROPDOWN_COMPONENT_H