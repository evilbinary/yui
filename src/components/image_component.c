#include "image_component.h"
#include "../render.h"
#include "../backend.h"
#include <stdlib.h>
#include <string.h>

// 创建图片组件
ImageComponent* image_component_create(Layer* layer) {
    if (!layer) {
        return NULL;
    }
    
    ImageComponent* component = (ImageComponent*)malloc(sizeof(ImageComponent));
    if (!component) {
        return NULL;
    }
    
    memset(component, 0, sizeof(ImageComponent));
    component->layer = layer;
    component->image_mode = IMAGE_MODE_STRETCH;
    component->preserve_aspect = 1;
    
    // 设置组件指针和自定义渲染函数
    layer->component = component;
    layer->render = image_component_render;
    
    return component;
}

// 销毁图片组件
void image_component_destroy(ImageComponent* component) {
    if (component) {
        // 释放图片纹理
        if (component->layer && component->layer->texture) {
            backend_render_text_destroy(component->layer->texture);
            component->layer->texture = NULL;
        }
        free(component);
    }
}

// 设置图片源文件路径
void image_component_set_source(ImageComponent* component, const char* source) {
    if (!component || !source) {
        return;
    }
    
    strncpy(component->source, source, MAX_PATH - 1);
    component->source[MAX_PATH - 1] = '\0';
    
    // 更新layer的source属性
    strncpy(component->layer->source, source, MAX_PATH - 1);
    component->layer->source[MAX_PATH - 1] = '\0';
    
    // 释放旧的纹理
    if (component->layer->texture) {
        backend_render_text_destroy(component->layer->texture);
        component->layer->texture = NULL;
    }
}

// 设置图片显示模式
void image_component_set_image_mode(ImageComponent* component, ImageMode mode) {
    if (!component) {
        return;
    }
    
    component->image_mode = mode;
    component->layer->image_mode = mode;
}

// 设置是否保持宽高比
void image_component_set_preserve_aspect(ImageComponent* component, int preserve) {
    if (!component) {
        return;
    }
    
    component->preserve_aspect = preserve;
    
    // 如果设置为不保持宽高比，则强制使用拉伸模式
    if (!preserve) {
        component->image_mode = IMAGE_MODE_STRETCH;
        component->layer->image_mode = IMAGE_MODE_STRETCH;
    }
}

// 渲染图片组件
void image_component_render(Layer* layer) {
    if (!layer || !layer->component) {
        return;
    }
    
    ImageComponent* component = (ImageComponent*)layer->component;
    // 图片类型渲染：从文件路径加载并渲染图片（支持多种格式）
    if (strlen(layer->source) > 0 && !layer->texture) {
        // 修改为使用image支持多种格式
        load_textures(layer);
    }
    
    
    // 渲染图片纹理
    if (layer->texture) {
        // 根据图片模式进行不同的渲染
        if (layer->image_mode == IMAGE_MODE_STRETCH || !component->preserve_aspect) {
            // 拉伸模式：直接填充整个区域
            backend_render_text_copy(layer->texture, NULL, &layer->rect);
        } else {
            // 获取图片原始尺寸
            int img_width, img_height;
            backend_query_texture(layer->texture, NULL, NULL, &img_width, &img_height);
            
            // 计算缩放比例
            float scale_x = (float)layer->rect.w / img_width;
            float scale_y = (float)layer->rect.h / img_height;
            
            Rect render_rect;
            render_rect.x = layer->rect.x;
            render_rect.y = layer->rect.y;
            render_rect.w = img_width;
            render_rect.h = img_height;
            
            if (layer->image_mode == IMAGE_MODE_ASPECT_FIT) {
                // 自适应模式：完整显示图片，可能有空白区域
                float scale = fmin(scale_x, scale_y);
                render_rect.w = (int)(img_width * scale);
                render_rect.h = (int)(img_height * scale);
                render_rect.x = layer->rect.x + (layer->rect.w - render_rect.w) / 2;
                render_rect.y = layer->rect.y + (layer->rect.h - render_rect.h) / 2;
            } else if (layer->image_mode == IMAGE_MODE_ASPECT_FILL) {
                // 填充模式：填满整个区域，可能裁剪图片
                float scale = fmax(scale_x, scale_y);
                render_rect.w = (int)(img_width * scale);
                render_rect.h = (int)(img_height * scale);
                render_rect.x = layer->rect.x + (layer->rect.w - render_rect.w) / 2;
                render_rect.y = layer->rect.y + (layer->rect.h - render_rect.h) / 2;
            }
            
            backend_render_text_copy(layer->texture, NULL, &render_rect);
        }
    } else {
        // 如果图片加载失败，绘制一个占位符
        Color placeholder_color = {200, 200, 200, 255};
        if (layer->radius > 0) {
            backend_render_rounded_rect(&layer->rect, placeholder_color, layer->radius);
        } else {
            backend_render_fill_rect(&layer->rect, placeholder_color);
        }
        
        // 绘制边框
        backend_render_rect_color(&layer->rect, 150, 150, 150, 255);
    }
}