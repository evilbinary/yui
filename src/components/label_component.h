#ifndef YUI_LABEL_COMPONENT_H
#define YUI_LABEL_COMPONENT_H

#include "../ytype.h"

// 标签组件结构体
typedef struct {
    Layer* layer;          // 关联的图层
    int text_alignment;    // 文本对齐方式
    int auto_size;         // 是否自动调整大小
} LabelComponent;

// 函数声明
LabelComponent* label_component_create(Layer* layer);
void label_component_destroy(LabelComponent* component);
void label_component_set_text(LabelComponent* component, const char* text);
void label_component_set_text_alignment(LabelComponent* component, int alignment);
void label_component_set_auto_size(LabelComponent* component, int auto_size);
void label_component_render(Layer* layer);

#endif  // YUI_LABEL_COMPONENT_H