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
    component->target_progress = 0.0f;
    component->animation_speed = 0.1f; // 默认动画速度
    component->animating = 0;
    component->direction = PROGRESS_DIRECTION_HORIZONTAL;
    component->fill_color = (Color){50, 150, 255, 255};
    component->show_percentage = 1;
    
    // 设置组件指针和自定义渲染函数
    layer->component = component;
    layer->render = progress_component_render;
    
    // 设置进度值
    if (layer->data && layer->data->json) {
        ProgressComponent* progress_component = (ProgressComponent*)layer->component;
        int value = layer->data->json->valueint;
        // 将0-100的值转换为0.0-1.0
        float progress = value / 100.0f;
        progress_component_set_progress(progress_component, progress);
    }
    
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
        progress = 0.0f;
    } else if (progress > 1.0f) {
        progress = 1.0f;
    }
    
    // 设置目标进度并标记为动画中
    component->target_progress = progress;
    component->animating = 1;
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
    
    component->layer->bg_color = color;
}

// 设置是否显示百分比文本
void progress_component_set_show_percentage(ProgressComponent* component, int show) {
    if (!component) {
        return;
    }
    
    component->show_percentage = show;
}

// 设置动画速度
void progress_component_set_animation_speed(ProgressComponent* component, float speed) {
    if (!component) {
        return;
    }
    
    // 确保速度在0.01到1.0之间
    if (speed < 0.01f) {
        component->animation_speed = 0.01f;
    } else if (speed > 1.0f) {
        component->animation_speed = 1.0f;
    } else {
        component->animation_speed = speed;
    }
}

// 渲染进度条组件
void progress_component_render(Layer* layer) {
    if (!layer || !layer->component) {
        return;
    }
    
    ProgressComponent* component = (ProgressComponent*)layer->component;
    
    // 调试信息：打印当前状态
    printf("Progress: %.2f, Target: %.2f, Animating: %d\n", 
           component->progress, component->target_progress, component->animating);
    
    // 处理动画更新
    if (component->animating) {
        // 计算当前帧应该移动的进度
        float diff = component->target_progress - component->progress;
        
        printf("Animation diff: %.4f\n", diff);
        
        if (fabs(diff) < 0.01f) {
            // 如果差值很小，直接设置为目标值并停止动画
            component->progress = component->target_progress;
            component->animating = 0;
            printf("Animation completed, progress set to: %.2f\n", component->progress);
        } else {
            // 否则，根据动画速度更新进度
            component->progress += diff * component->animation_speed;
            printf("Animation updated, new progress: %.2f\n", component->progress);
        }
    }
    
    // 绘制背景 - 移除透明度检查，确保背景总是被渲染
    if (layer->radius > 0) {
        // 使用圆角背景
        backend_render_rounded_rect(&layer->rect, layer->bg_color, layer->radius);
    } else {
        // 使用普通矩形背景
        backend_render_fill_rect(&layer->rect, layer->bg_color);
    }
    
    // 绘制边框 - 移到背景之后，进度条之前，避免遮挡进度条颜色
    if (layer->radius > 0) {
        // 使用backend_render_rounded_rect_color绘制边框
        backend_render_rounded_rect_color(&layer->rect, 150, 150, 150, 255, layer->radius);
    } else {
        // 使用普通边框
        backend_render_rect_color(&layer->rect, 150, 150, 150, 255);
    }
    
    // 计算填充区域
    Rect fill_rect = layer->rect;
    
    // 修复进度条长度计算，使用更精确的计算方式
    if (component->direction == PROGRESS_DIRECTION_HORIZONTAL) {
        // 水平进度条
        fill_rect.w = (int)(layer->rect.w * component->progress + 0.5f); // 添加0.5f进行四舍五入
    } else {
        // 垂直进度条
        fill_rect.h = (int)(layer->rect.h * component->progress + 0.5f); // 添加0.5f进行四舍五入
        fill_rect.y = layer->rect.y + (layer->rect.h - fill_rect.h);
    }
    
    // 绘制填充部分 - 移到最后，确保显示在最上层
    if (layer->radius > 0) {
        backend_render_rounded_rect(&fill_rect, component->fill_color, layer->radius);
    } else {
        backend_render_fill_rect(&fill_rect, component->fill_color);
    }
    
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