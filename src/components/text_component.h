#ifndef YUI_TEXT_COMPONENT_H
#define YUI_TEXT_COMPONENT_H

#include "../ytype.h"

typedef struct Layer Layer;
typedef struct KeyEvent KeyEvent;

// 多行文本组件结构体
typedef struct {
    Layer* layer;          // 关联的图层
    char text[MAX_TEXT * 4];   // 输入文本（支持更多内容）
    char placeholder[MAX_TEXT];  // 占位文本
    int cursor_pos;        // 光标位置
    int selection_start;   // 选择起始位置
    int selection_end;     // 选择结束位置
    int max_length;        // 最大输入长度
    Color cursor_color;    // 光标颜色
    int scroll_x;          // 水平滚动位置
    int scroll_y;          // 垂直滚动位置
    int line_height;       // 行高
    int multiline;         // 是否为多行模式
    int editable;          // 是否可编辑
    int show_line_numbers; // 是否显示行号
    int line_number_width; // 行号区域宽度
    Color line_number_color; // 行号颜色
    Color line_number_bg_color; // 行号背景颜色
} TextComponent;

// 函数声明
TextComponent* text_component_create(Layer* layer);
TextComponent* text_component_create_from_json(Layer* layer,cJSON* json_obj);
void text_component_destroy(TextComponent* component);
void text_component_set_text(TextComponent* component, const char* text);
void text_component_set_placeholder(TextComponent* component, const char* placeholder);
void text_component_set_max_length(TextComponent* component, int max_length);
void text_component_set_cursor_color(TextComponent* component, Color cursor_color);
void text_component_set_multiline(TextComponent* component, int multiline);
void text_component_set_editable(TextComponent* component, int editable);
void text_component_set_show_line_numbers(TextComponent* component, int show_line_numbers);
void text_component_set_line_number_width(TextComponent* component, int width);
void text_component_set_line_number_color(TextComponent* component, Color color);
void text_component_set_line_number_bg_color(TextComponent* component, Color color);
void text_component_handle_key_event(Layer* layer, KeyEvent* event);
void text_component_handle_mouse_event(Layer* layer, MouseEvent* event);
void text_component_render(Layer* layer);

#endif  // YUI_TEXT_COMPONENT_H