#ifndef YUI_IMAGE_COMPONENT_H
#define YUI_IMAGE_COMPONENT_H

#include "../ytype.h"

// 图片组件结构体
typedef struct {
    Layer* layer;          // 关联的图层
    char source[MAX_PATH]; // 图片源文件路径
    ImageMode image_mode;  // 图片显示模式
    int preserve_aspect;   // 是否保持宽高比
} ImageComponent;

// 函数声明
ImageComponent* image_component_create(Layer* layer);
void image_component_destroy(ImageComponent* component);
void image_component_set_source(ImageComponent* component, const char* source);
void image_component_set_image_mode(ImageComponent* component, ImageMode mode);
void image_component_set_preserve_aspect(ImageComponent* component, int preserve);
void image_component_render(Layer* layer);

#endif  // YUI_IMAGE_COMPONENT_H