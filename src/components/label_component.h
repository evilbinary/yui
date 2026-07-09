#ifndef YUI_LABEL_COMPONENT_H
#define YUI_LABEL_COMPONENT_H

#include "../ytype.h"

// 标签组件结构体
typedef struct {
    Layer* layer;          // 关联的图层
    int text_alignment;    // 文本对齐方式
    int auto_size;         // 是否自动调整大小
    int has_overflow;      // 文本是否超出宽度
    Uint32 hover_start;    // 鼠标进入的时间戳
    int hovering;          // 鼠标是否在组件上
} LabelComponent;

// 函数声明
LabelComponent* label_component_create(Layer* layer);
void label_component_destroy(LabelComponent* component);
void label_component_set_text(LabelComponent* component, const char* text);
void label_component_set_text_alignment(LabelComponent* component, int alignment);
void label_component_set_auto_size(LabelComponent* component, int auto_size);
void label_component_render(Layer* layer);
void label_component_handle_mouse_event(Layer* layer, MouseEvent* event);

#endif  // YUI_LABEL_COMPONENT_H
