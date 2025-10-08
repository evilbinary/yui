#ifndef YUI_CHECKBOX_COMPONENT_H
#define YUI_CHECKBOX_COMPONENT_H

#include "../ytype.h"

// 复选框组件结构体
typedef struct {
    Layer* layer;          // 关联的图层
    int checked;           // 是否选中
    Color bg_color;        // 背景颜色
    Color border_color;    // 边框颜色
    Color check_color;     // 勾选颜色
    void* user_data;       // 用户数据
} CheckboxComponent;

// 函数声明
CheckboxComponent* checkbox_component_create(Layer* layer);
void checkbox_component_destroy(CheckboxComponent* component);
void checkbox_component_set_checked(CheckboxComponent* component, int checked);
int checkbox_component_is_checked(CheckboxComponent* component);
void checkbox_component_set_colors(CheckboxComponent* component, Color bg_color, Color border_color, Color check_color);
void checkbox_component_set_user_data(CheckboxComponent* component, void* data);
void checkbox_component_handle_mouse_event(Layer* layer, MouseEvent* event);
void checkbox_component_render(Layer* layer);

#endif  // YUI_CHECKBOX_COMPONENT_H