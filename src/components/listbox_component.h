#ifndef YUI_LISTBOX_COMPONENT_H
#define YUI_LISTBOX_COMPONENT_H

#include "../ytype.h"

// 列表框项目结构体
typedef struct {
    char* text;           // 项目文本
    void* user_data;      // 用户数据
    int selected;         // 是否选中
} ListBoxItem;

// 列表框组件结构体
typedef struct {
    Layer* layer;             // 关联的图层
    ListBoxItem* items;       // 项目列表
    int item_count;           // 项目数量
    int selected_index;        // 当前选中项索引
    int visible_count;         // 可见项目数量
    int scroll_position;       // 滚动位置
    Color bg_color;           // 背景颜色
    Color text_color;         // 文本颜色
    Color selected_bg_color;   // 选中项背景颜色
    Color selected_text_color; // 选中项文本颜色
    int multi_select;          // 是否允许多选
    void* user_data;           // 用户数据
    
    // 回调函数
    void (*on_selection_changed)(int index, void* user_data);
    void (*on_item_double_click)(int index, void* user_data);
} ListBoxComponent;

// 函数声明
ListBoxComponent* listbox_component_create(Layer* layer);
void listbox_component_destroy(ListBoxComponent* component);
void listbox_component_add_item(ListBoxComponent* component, const char* text, void* user_data);
void listbox_component_remove_item(ListBoxComponent* component, int index);
void listbox_component_clear_items(ListBoxComponent* component);
void listbox_component_set_selected(ListBoxComponent* component, int index);
int listbox_component_get_selected(ListBoxComponent* component);
void listbox_component_set_multi_select(ListBoxComponent* component, int multi_select);
void listbox_component_set_colors(ListBoxComponent* component, Color bg, Color text, Color selected_bg, Color selected_text);
void listbox_component_set_user_data(ListBoxComponent* component, void* data);
void listbox_component_set_selection_changed_callback(ListBoxComponent* component, void (*callback)(int, void*));
void listbox_component_set_item_double_click_callback(ListBoxComponent* component, void (*callback)(int, void*));
void listbox_component_handle_mouse_event(Layer* layer, MouseEvent* event);
void listbox_component_handle_key_event(Layer* layer, KeyEvent* event);
void listbox_component_render(Layer* layer);

#endif  // YUI_LISTBOX_COMPONENT_H