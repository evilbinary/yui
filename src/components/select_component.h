#ifndef YUI_SELECT_COMPONENT_H
#define YUI_SELECT_COMPONENT_H

#include "../ytype.h"
#include "../render.h"
#include <SDL.h>

// 前向声明
typedef struct PopupLayer PopupLayer;

// Select 选项结构体
typedef struct {
    char* text;           // 选项文本
    void* user_data;      // 用户数据
    int selected;         // 是否选中
    int disabled;         // 是否禁用
} SelectItem;

// Select 组件结构体
typedef struct {
    Layer* layer;                    // 关联的图层
    Layer* dropdown_layer;            // 下拉菜单图层
    
    SelectItem* items;                // 选项数组
    int item_count;                   // 选项数量
    int selected_index;               // 当前选中索引
    int expanded;                      // 是否展开
    
    int max_visible_items;            // 最大可见选项数
    int item_height;                  // 选项高度
    int hover_index;                  // 悬停索引
    int scroll_position;              // 滚动位置
    int scrollbar_width;              // 滚动条宽度
    int is_dragging;                  // 是否正在拖动滚动条
    int drag_start_y;                 // 拖动开始Y坐标
    int drag_start_scroll;            // 拖动开始滚动位置
    int just_expanded;                // 刚刚展开标志
    
    // 颜色配置
    Color bg_color;                   // 背景颜色
    Color text_color;                 // 文本颜色
    Color border_color;               // 边框颜色
    Color arrow_color;                // 箭头颜色
    Color dropdown_bg_color;          // 下拉菜单背景颜色
    Color hover_bg_color;             // 悬停背景颜色
    Color selected_bg_color;          // 选中背景颜色
    Color selected_text_color;        // 选中文本颜色
    Color disabled_text_color;        // 禁用文本颜色
    Color scrollbar_color;            // 滚动条颜色
    Color scrollbar_bg_color;        // 滚动条背景颜色
    Color focus_border_color;         // 焦点边框颜色
    Color hover_border_color;         // 悬停边框颜色
    
    // 样式配置
    int border_width;                 // 边框宽度
    int border_radius;                // 边框圆角
    int placeholder_index;            // 占位符索引 (-1 表示无占位符)
    int font_size;                    // 字体大小
    DFont* font;                      // 组件专用字体
    
    // 回调函数
    void* user_data;                  // 用户数据
    void (*on_selection_changed)(int index, void* user_data);     // 选择变化回调
    void (*on_dropdown_expanded)(int expanded, void* user_data);  // 展开状态变化回调
} SelectComponent;

// 函数声明
SelectComponent* select_component_create(Layer* layer);
SelectComponent* select_component_create_from_json(Layer* layer, cJSON* json_obj);
void select_component_destroy(SelectComponent* component);

// 选项管理
void select_component_add_item(SelectComponent* component, const char* text, void* user_data);
void select_component_add_placeholder(SelectComponent* component, const char* text);
void select_component_remove_item(SelectComponent* component, int index);
void select_component_clear_items(SelectComponent* component);
void select_component_insert_item(SelectComponent* component, int index, const char* text, void* user_data);

// 选择操作
void select_component_set_selected(SelectComponent* component, int index);
int select_component_get_selected(SelectComponent* component);
const char* select_component_get_selected_text(SelectComponent* component);
void* select_component_get_selected_data(SelectComponent* component);
void select_component_clear_selection(SelectComponent* component);

// 选项状态
void select_component_set_item_disabled(SelectComponent* component, int index, int disabled);
int select_component_is_item_disabled(SelectComponent* component, int index);
int select_component_get_item_count(SelectComponent* component);
const char* select_component_get_item_text(SelectComponent* component, int index);

// 样式设置
void select_component_set_colors(SelectComponent* component, 
                                Color bg, Color text, Color border, Color arrow,
                                Color dropdown_bg, Color hover, Color selected_bg, 
                                Color selected_text, Color disabled_text);

void select_component_set_extended_colors(SelectComponent* component,
                                       Color focus_border, Color hover_border,
                                       Color scrollbar, Color scrollbar_bg);
void select_component_set_border_style(SelectComponent* component, int width, int radius);
void select_component_set_max_visible_items(SelectComponent* component, int max_visible);
void select_component_set_font_size(SelectComponent* component, int font_size);

// 回调设置
void select_component_set_user_data(SelectComponent* component, void* data);
void select_component_set_selection_changed_callback(SelectComponent* component, void (*callback)(int, void*));
void select_component_set_dropdown_expanded_callback(SelectComponent* component, void (*callback)(int, void*));

// 控制操作
void select_component_expand(SelectComponent* component);
void select_component_collapse(SelectComponent* component);
void select_component_toggle(SelectComponent* component);

// 事件处理
void select_component_handle_mouse_event(Layer* layer, MouseEvent* event);
void select_component_handle_key_event(Layer* layer, KeyEvent* event);
void select_component_handle_scroll_event(Layer* layer, int scroll_delta);
void select_component_scroll_callback(Layer* layer);
void select_component_render(Layer* layer);

// 弹出层专用函数
void select_component_render_dropdown_only(Layer* layer);
void select_component_handle_dropdown_mouse_event(Layer* layer, MouseEvent* event);
void select_component_handle_dropdown_key_event(Layer* layer, KeyEvent* event);
void select_component_handle_dropdown_scroll_event(Layer* layer, int scroll_delta);
void select_component_popup_close_callback(PopupLayer* popup);

#endif  // YUI_SELECT_COMPONENT_H