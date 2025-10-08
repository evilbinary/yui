#ifndef YUI_RADIOBOX_COMPONENT_H
#define YUI_RADIOBOX_COMPONENT_H

#include "../ytype.h"

// 单选框组结构体
typedef struct {
    char group_id[50];     // 组ID
    CheckboxComponent** radios; // 组内的单选框列表
    int radio_count;       // 单选框数量
} RadioGroup;

// 单选框组件结构体
typedef struct {
    Layer* layer;          // 关联的图层
    int checked;           // 是否选中
    Color bg_color;        // 背景颜色
    Color border_color;    // 边框颜色
    Color dot_color;       // 圆点颜色
    char group_id[50];     // 所属组ID
    void* user_data;       // 用户数据
} RadioboxComponent;

// 函数声明
RadioboxComponent* radiobox_component_create(Layer* layer, const char* group_id);
void radiobox_component_destroy(RadioboxComponent* component);
void radiobox_component_set_checked(RadioboxComponent* component, int checked);
int radiobox_component_is_checked(RadioboxComponent* component);
void radiobox_component_set_colors(RadioboxComponent* component, Color bg_color, Color border_color, Color dot_color);
void radiobox_component_set_group(RadioboxComponent* component, const char* group_id);
void radiobox_component_set_user_data(RadioboxComponent* component, void* data);
void radiobox_component_handle_mouse_event(Layer* layer, MouseEvent* event);
void radiobox_component_render(Layer* layer);

// 单选框组相关函数
RadioGroup* radiobox_get_group(const char* group_id);
void radiobox_add_to_group(RadioboxComponent* component);
void radiobox_remove_from_group(RadioboxComponent* component);
void radiobox_set_group_checked(const char* group_id, RadioboxComponent* component);

#endif  // YUI_RADIOBOX_COMPONENT_H