#include "label_component.h"
#include "../render.h"
#include "../backend.h"
#include <stdlib.h>
#include <string.h>

// 创建标签组件
LabelComponent* label_component_create(Layer* layer) {
    if (!layer) {
        return NULL;
    }
    
    LabelComponent* component = (LabelComponent*)malloc(sizeof(LabelComponent));
    if (!component) {
        return NULL;
    }
    
    memset(component, 0, sizeof(LabelComponent));
    component->layer = layer;
    component->text_alignment = LAYOUT_CENTER;
    component->auto_size = 0;
    
    // 设置组件指针和自定义渲染函数
    layer->component = component;
    layer->render = label_component_render;
    
    return component;
}

// 销毁标签组件
void label_component_destroy(LabelComponent* component) {
    if (component) {
        free(component);
    }
}

// 设置标签文本
void label_component_set_text(LabelComponent* component, const char* text) {
    if (!component || !text) {
        return;
    }
    
    strncpy(component->layer->text, text, MAX_TEXT - 1);
    component->layer->text[MAX_TEXT - 1] = '\0';
    
    // 如果启用了自动调整大小，则更新图层大小
        if (component->auto_size && component->layer->font && component->layer->font->default_font) {
            int text_width, text_height;
            // 使用现有的函数来获取文本尺寸
            Texture* text_texture = backend_render_texture(component->layer->font->default_font, text, (Color){0, 0, 0, 255});
            if (text_texture) {
                backend_query_texture(text_texture, NULL, NULL, &text_width, &text_height);
                backend_render_text_destroy(text_texture);
                component->layer->rect.w = text_width / scale + 10; // 左右各留5像素边距
                component->layer->rect.h = text_height / scale + 10; // 上下各留5像素边距
            }
        }
}

// 设置文本对齐方式
void label_component_set_text_alignment(LabelComponent* component, int alignment) {
    if (!component) {
        return;
    }
    
    component->text_alignment = alignment;
}

// 设置自动调整大小
void label_component_set_auto_size(LabelComponent* component, int auto_size) {
    if (!component) {
        return;
    }
    
    component->auto_size = auto_size;
    
    // 如果启用了自动调整大小且已有文本，则更新图层大小
    if (auto_size && strlen(component->layer->text) > 0 && 
        component->layer->font && component->layer->font->default_font) {
        label_component_set_text(component, component->layer->text); // 重用设置文本的逻辑来调整大小
    }
}

// 渲染标签组件
void label_component_render(Layer* layer) {
    if (!layer || !layer->component) {
        return;
    }
    
    LabelComponent* component = (LabelComponent*)layer->component;
    
    // 绘制背景
    if (layer->bg_color.a > 0) {
        if (layer->radius > 0) {
            backend_render_rounded_rect(&layer->rect, layer->bg_color, layer->radius);
        } else {
            backend_render_fill_rect(&layer->rect, layer->bg_color);
        }
    }
    
    // 渲染文本
    Color text_color = layer->color;
    Texture* text_texture = render_text(layer, layer->text, text_color);
    
    if (text_texture) {
        int text_width, text_height;
        backend_query_texture(text_texture, NULL, NULL, &text_width, &text_height);
        
        Rect text_rect;
        text_rect.h = text_height / scale;
        text_rect.w = text_width / scale;
        text_rect.y = layer->rect.y + (layer->rect.h - text_rect.h) / 2;
        
        // 根据对齐方式设置X坐标
        switch (component->text_alignment) {
            case LAYOUT_LEFT:
            case LAYOUT_ALIGN_LEFT:
                text_rect.x = layer->rect.x + 5;  // 左侧留5像素边距
                break;
            case LAYOUT_RIGHT:
            case LAYOUT_ALIGN_RIGHT:
                text_rect.x = layer->rect.x + layer->rect.w - text_rect.w - 5;  // 右侧留5像素边距
                break;
            case LAYOUT_CENTER:
            case LAYOUT_ALIGN_CENTER:
            default:
                text_rect.x = layer->rect.x + (layer->rect.w - text_rect.w) / 2;
                break;
        }
        
        // 确保文本不会超出标签边界
        if (text_rect.x < layer->rect.x) {
            text_rect.x = layer->rect.x;
        }
        if (text_rect.x + text_rect.w > layer->rect.x + layer->rect.w) {
            text_rect.w = layer->rect.x + layer->rect.w - text_rect.x;
        }
        
        if (text_rect.y < layer->rect.y) {
            text_rect.y = layer->rect.y;
        }
        if (text_rect.y + text_rect.h > layer->rect.y + layer->rect.h) {
            text_rect.h = layer->rect.y + layer->rect.h - text_rect.y;
        }
        
        backend_render_text_copy(text_texture, NULL, &text_rect);
        backend_render_text_destroy(text_texture);
    }
}