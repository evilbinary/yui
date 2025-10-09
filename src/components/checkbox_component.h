#ifndef YUI_CHECKBOX_COMPONENT_H
#define YUI_CHECKBOX_COMPONENT_H

#include "../ytype.h"

// 复选框组件结构体
typedef struct {
    Layer* layer;          // 关联的图层
    int checked;           // 是否选中
    // 不再需要bg_color字段，使用layer->bgColor代替
    Color border_color;    // 边框颜色
    Color check_color;     // 勾选颜色
    // 不再需要label_color字段，使用layer->color代替
    // 不再需要单独的label字段，使用layer->label代替
    void* user_data;       // 用户数据
} CheckboxComponent;

// 函数声明
CheckboxComponent* checkbox_component_create(Layer* layer, int default_checked);
CheckboxComponent* checkbox_component_create_from_json(Layer* layer, cJSON* json_obj);
void checkbox_component_destroy(CheckboxComponent* component);
void checkbox_component_set_checked(CheckboxComponent* component, int checked);
int checkbox_component_is_checked(CheckboxComponent* component);
void checkbox_component_set_colors(CheckboxComponent* component, Color bg_color, Color border_color, Color check_color);
void checkbox_component_set_user_data(CheckboxComponent* component, void* data);
void checkbox_component_set_label(CheckboxComponent* component, const char* label, Color color);
// 添加禁用/启用函数
void checkbox_component_set_disabled(CheckboxComponent* component, int disabled);
int checkbox_component_is_disabled(CheckboxComponent* component);
void checkbox_component_handle_mouse_event(Layer* layer, MouseEvent* event);
void checkbox_component_render(Layer* layer);

#endif  // YUI_CHECKBOX_COMPONENT_H