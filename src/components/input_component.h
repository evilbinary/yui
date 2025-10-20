#ifndef YUI_INPUT_COMPONENT_H
#define YUI_INPUT_COMPONENT_H

#include "../ytype.h"

typedef struct Layer Layer;
typedef struct KeyEvent KeyEvent;

// 输入组件结构体
typedef struct {
    Layer* layer;          // 关联的图层
    char text[MAX_TEXT];   // 输入文本
    char placeholder[MAX_TEXT];  // 占位文本
    int cursor_pos;        // 光标位置
    int selection_start;   // 选择起始位置
    int selection_end;     // 选择结束位置
    int max_length;        // 最大输入长度
    int password_mode;     // 是否为密码模式
    Color cursor_color;    // 光标颜色
} InputComponent;

// 函数声明
InputComponent* input_component_create(Layer* layer);
InputComponent* input_component_create_from_json(Layer* layer, cJSON* json_obj);
void input_component_destroy(InputComponent* component);
void input_component_set_text(InputComponent* component, const char* text);
void input_component_set_placeholder(InputComponent* component, const char* placeholder);
void input_component_set_max_length(InputComponent* component, int max_length);
void input_component_set_cursor_color(InputComponent* component, Color cursor_color);
void input_component_handle_key_event(Layer* layer, KeyEvent* event);
void input_component_handle_mouse_event(Layer* layer,MouseEvent* event);
void input_component_render(Layer* layer);

#endif  // YUI_INPUT_COMPONENT_H