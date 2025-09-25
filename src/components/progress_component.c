#include "progress_component.h"
#include "../render.h"
#include "../backend.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// 创建进度条组件
ProgressComponent* progress_component_create(Layer* layer) {
    if (!layer) {
        return NULL;
    }
    
    ProgressComponent* component = (ProgressComponent*)malloc(sizeof(ProgressComponent));
    if (!component) {
        return NULL;
    }
    
    memset(component, 0, sizeof(ProgressComponent));
    component->layer = layer;
    component->progress = 0.0f;
    component->direction = PROGRESS_DIRECTION_HORIZONTAL;
    component->fill_color = (Color){50, 150, 255, 255};
    component->show_percentage = 1;
    
    // 设置组件指针和自定义渲染函数
    layer->component = component;
    layer->render = progress_component_render;
    
    return component;
}

// 销毁进度条组件
void progress_component_destroy(ProgressComponent* component) {
    if (component) {
        free(component);
    }
}

// 设置当前进度
void progress_component_set_progress(ProgressComponent* component, float progress) {
    if (!component) {
        return;
    }
    
    // 确保进度在0.0到1.0之间
    if (progress < 0.0f) {
        component->progress = 0.0f;
    } else if (progress > 1.0f) {
        component->progress = 1.0f;
    } else {
        component->progress = progress;
    }
}

// 设置进度条方向
void progress_component_set_direction(ProgressComponent* component, ProgressDirection direction) {
    if (!component) {
        return;
    }
    
    component->direction = direction;
}

// 设置填充颜色
void progress_component_set_fill_color(ProgressComponent* component, Color color) {
    if (!component) {
        return;
    }
    
    component->fill_color = color;
}

// 设置背景颜色
void progress_component_set_background_color(ProgressComponent* component, Color color) {
    if (!component) {
        return;
    }
    
    component->layer->bgColor = color;
}

// 设置是否显示百分比文本
void progress_component_set_show_percentage(ProgressComponent* component, int show) {
    if (!component) {
        return;
    }
    
    component->show_percentage = show;
}

// 渲染进度条组件
void progress_component_render(Layer* layer) {
    if (!layer || !layer->component) {
        return;
    }
    
    ProgressComponent* component = (ProgressComponent*)layer->component;
    
      // 按钮类型渲染：绘制背景和边框
    if(layer->bgColor.a>0){
        backend_render_fill_rect(&layer->rect,layer->bgColor);
    }

    if(layer->data!=NULL){
        Rect fill_rect = {
                layer->rect.x ,
                layer->rect.y,
                layer->rect.w *(layer->data->json->valueint/100.0),
                layer->rect.h
            };
        backend_render_fill_rect(&fill_rect,layer->color);
    }
    
    // // 绘制背景
    // if (layer->radius > 0) {
    //     backend_render_rounded_rect(&layer->rect, layer->bgColor, layer->radius);
    // } else {
    //     backend_render_fill_rect(&layer->rect, layer->bgColor);
    // }
    
    // 计算填充区域
    Rect fill_rect = layer->rect;
    
    if (component->direction == PROGRESS_DIRECTION_HORIZONTAL) {
        // 水平进度条
        fill_rect.w = (int)(layer->rect.w * component->progress);
    } else {
        // 垂直进度条
        fill_rect.h = (int)(layer->rect.h * component->progress);
        fill_rect.y = layer->rect.y + (layer->rect.h - fill_rect.h);
    }
    
    // 绘制填充部分
    if (layer->radius > 0) {
        backend_render_rounded_rect(&fill_rect, component->fill_color, layer->radius);
    } else {
        backend_render_fill_rect(&fill_rect, component->fill_color);
    }
    
    // 绘制边框
    backend_render_rect_color(&layer->rect, 150, 150, 150, 255);
    
    // 显示百分比文本
    if (component->show_percentage && layer->font && layer->font->default_font) {
        char percentage_text[10];
        int percentage = (int)(component->progress * 100.0f);
        snprintf(percentage_text, sizeof(percentage_text), "%d%%", percentage);
        
        Color text_color = (Color){0, 0, 0, 255};
        Texture* text_texture = render_text(layer, percentage_text, text_color);
        
        if (text_texture) {
            int text_width, text_height;
            backend_query_texture(text_texture, NULL, NULL, &text_width, &text_height);
            
            Rect text_rect = {
                layer->rect.x + (layer->rect.w - text_width / scale) / 2,  // 居中
                layer->rect.y + (layer->rect.h - text_height / scale) / 2,
                text_width / scale,
                text_height / scale
            };
            
            // 确保文本不会超出进度条边界
            if (text_rect.w > layer->rect.w - 10) {
                text_rect.w = layer->rect.w - 10;
                text_rect.x = layer->rect.x + 5;
            }
            
            if (text_rect.h > layer->rect.h - 10) {
                text_rect.h = layer->rect.h - 10;
                text_rect.y = layer->rect.y + 5;
            }
            
            backend_render_text_copy(text_texture, NULL, &text_rect);
            backend_render_text_destroy(text_texture);
        }
    }
}