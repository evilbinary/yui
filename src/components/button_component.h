#ifndef YUI_BUTTON_COMPONENT_H
#define YUI_BUTTON_COMPONENT_H

#include "../ytype.h"

// 按钮状态枚举
typedef enum {
    BUTTON_STATE_NORMAL,
    BUTTON_STATE_HOVER,
    BUTTON_STATE_PRESSED,
    BUTTON_STATE_DISABLED
} ButtonState;

// 按钮组件结构体
typedef struct {
    Layer* layer;          // 关联的图层
    ButtonState state;     // 按钮状态
    Color colors[4];       // 不同状态下的颜色 [NORMAL, HOVER, PRESSED, DISABLED]
    void* user_data;       // 用户数据
} ButtonComponent;

// 函数声明
ButtonComponent* button_component_create(Layer* layer);
void button_component_destroy(ButtonComponent* component);
void button_component_set_text(ButtonComponent* component, const char* text);
void button_component_set_state(ButtonComponent* component, ButtonState state);
void button_component_set_color(ButtonComponent* component, ButtonState state, Color color);
void button_component_set_user_data(ButtonComponent* component, void* data);
void button_component_handle_mouse_event(Layer* layer, MouseEvent* event);
void button_component_render(Layer* layer);

#endif  // YUI_BUTTON_COMPONENT_H