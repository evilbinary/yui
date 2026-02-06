#include "progress_component.h"
#include "../render.h"
#include "../backend.h"
#include "../util.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "cJSON.h"


#define printf

// 辅助函数：绘制圆弧（用于圆形进度条）- 使用后端抗锯齿渲染
static void render_circle_arc(int center_x, int center_y, int radius, int start_angle, int end_angle, Color color, int line_width) {
    backend_render_arc(center_x, center_y, radius, (float)start_angle, (float)end_angle, color, line_width);
}

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
    component->shape = PROGRESS_SHAPE_RECTANGLE; // 默认为长条形
    component->direction = PROGRESS_DIRECTION_HORIZONTAL;
    component->fill_color = (Color){50, 150, 255, 255};
    component->show_percentage = 1;
    component->circle_width = 8; // 默认圆形宽度为8px
    
    // 设置组件指针和自定义渲染函数
    layer->component = component;
    layer->render = progress_component_render;
    
    return component;
}

// 从 JSON 创建进度条组件
ProgressComponent* progress_component_create_from_json(Layer* layer, cJSON* json_obj) {
    // 先创建基础组件
    ProgressComponent* component = progress_component_create(layer);
    if (!component) {
        return NULL;
    }
    
    cJSON* style = json_obj ? cJSON_GetObjectItem(json_obj, "style") : NULL;
    int shape_specified = 0;
    int value_specified = 0;
    int direction_specified = 0;
    
    // 从 JSON 配置中读取属性
    if (json_obj) {
        // 读取进度值
        if (cJSON_HasObjectItem(json_obj, "value")) {
            cJSON* value_item = cJSON_GetObjectItem(json_obj, "value");
            if (cJSON_IsNumber(value_item)) {
                int value = value_item->valueint;
                // 将0-100的值转换为0.0-1.0
                float progress = value / 100.0f;
                progress_component_set_progress(component, progress);
                value_specified = 1;
            }
        } else if (cJSON_HasObjectItem(json_obj, "data")) {
            cJSON* data_item = cJSON_GetObjectItem(json_obj, "data");
            if (cJSON_IsNumber(data_item)) {
                int value = data_item->valueint;
                float progress = value / 100.0f;
                progress_component_set_progress(component, progress);
                value_specified = 1;
            }
        }
        
        // 读取形状配置
        cJSON* shape_item = cJSON_GetObjectItem(json_obj, "shape");
        if (!shape_item && style) {
            shape_item = cJSON_GetObjectItem(style, "shape");
        }
        if (shape_item && cJSON_IsString(shape_item)) {
            const char* shape_str = shape_item->valuestring;
            if (strcmp(shape_str, "circle") == 0) {
                component->shape = PROGRESS_SHAPE_CIRCLE;
            } else {
                component->shape = PROGRESS_SHAPE_RECTANGLE;
            }
            shape_specified = 1;
        }
        
        // 读取方向配置（仅对长条形有效）
        if (cJSON_HasObjectItem(json_obj, "direction")) {
            cJSON* direction_item = cJSON_GetObjectItem(json_obj, "direction");
            if (cJSON_IsString(direction_item)) {
                direction_specified = 1;
                const char* direction_str = direction_item->valuestring;
                if (strcmp(direction_str, "vertical") == 0) {
                    component->direction = PROGRESS_DIRECTION_VERTICAL;
                } else {
                    component->direction = PROGRESS_DIRECTION_HORIZONTAL;
                }
            }
        }
        
        // 读取是否显示百分比
        if (cJSON_HasObjectItem(json_obj, "showPercentage")) {
            cJSON* show_item = cJSON_GetObjectItem(json_obj, "showPercentage");
            if (cJSON_IsBool(show_item)) {
                component->show_percentage = cJSON_IsTrue(show_item) ? 1 : 0;
            }
        }
        
        // 读取填充颜色
        cJSON* color_item = cJSON_GetObjectItem(json_obj, "fillColor");
        if (!color_item && style) {
            color_item = cJSON_GetObjectItem(style, "fillColor");
        }
        if (color_item && cJSON_IsString(color_item)) {
            // 解析颜色字符串（如 "#ff0000"）
            const char* color_str = color_item->valuestring;
            if (color_str[0] == '#') {
                unsigned int r, g, b;
                sscanf(color_str + 1, "%02x%02x%02x", &r, &g, &b);
                component->fill_color = (Color){r, g, b, 255};
            }
        }
        
        // 读取动画速度
        if (cJSON_HasObjectItem(json_obj, "animationSpeed")) {
            cJSON* speed_item = cJSON_GetObjectItem(json_obj, "animationSpeed");
            if (cJSON_IsNumber(speed_item)) {
                component->animation_speed = speed_item->valuedouble;
            }
        }
        
        // 读取圆形进度条宽度
        cJSON* width_item = cJSON_GetObjectItem(json_obj, "circleWidth");
        if (!width_item && style) {
            width_item = cJSON_GetObjectItem(style, "circleWidth");
        }
        if (width_item && cJSON_IsNumber(width_item)) {
            component->circle_width = width_item->valueint;
            if (component->circle_width < 1) {
                component->circle_width = 1;
            }
        }
    }
    
    // 如果没有设置方向，默认使用较长边作为进度方向
    if (!direction_specified) {
        if (layer && layer->rect.w >= layer->rect.h) {
            component->direction = PROGRESS_DIRECTION_HORIZONTAL;
        } else {
            component->direction = PROGRESS_DIRECTION_VERTICAL;
        }
    }
    
    // 如果没有设置形状，且是正方形，默认使用圆形
    if (!shape_specified && layer && layer->rect.w > 0 && layer->rect.h > 0 &&
        layer->rect.w == layer->rect.h) {
        component->shape = PROGRESS_SHAPE_CIRCLE;
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

// 设置进度条形状
void progress_component_set_shape(ProgressComponent* component, ProgressShape shape) {
    if (!component) {
        return;
    }
    
    component->shape = shape;
}

// 设置圆形进度条宽度
void progress_component_set_circle_width(ProgressComponent* component, int width) {
    if (!component) {
        return;
    }
    
    component->circle_width = width;
    if (component->circle_width < 1) {
        component->circle_width = 1;
    }
}

// 渲染进度条组件
void progress_component_render(Layer* layer) {
    if (!layer || !layer->component) {
        return;
    }
    
    ProgressComponent* component = (ProgressComponent*)layer->component;
    
    // 调试信息：打印当前状态
    printf("Progress: %.2f, Target: %.2f, Animating: %d, Shape: %d\n", 
           component->progress, component->target_progress, component->animating, component->shape);
    
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
    
    // 根据形状选择不同的渲染方式
    if (component->shape == PROGRESS_SHAPE_CIRCLE) {
        // 圆形进度条渲染
        
        // 计算圆心和半径
        int center_x = layer->rect.x + layer->rect.w / 2;
        int center_y = layer->rect.y + layer->rect.h / 2;
        int radius = (layer->rect.w < layer->rect.h ? layer->rect.w : layer->rect.h) / 2 - 5; // 留5px边距
        
        if (radius <= 0) {
            return; // 半径太小，无法绘制
        }
        
        // 绘制背景圆环（灰色）
        render_circle_arc(center_x, center_y, radius, 0, 360, layer->bg_color, component->circle_width);
        
        // 绘制进度圆环（填充色）
        int angle = (int)(component->progress * 360.0f); // 将进度转换为角度
        if (angle > 0) {
            render_circle_arc(center_x, center_y, radius, -90, angle - 90, component->fill_color, component->circle_width);
        }
        
    } else {
        // 长条形进度条渲染（原有的逻辑）
        
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
    }
    
    // 显示百分比文本（两种形状都支持）
    // 对于圆形进度条，如果自身没有字体，尝试从父层继承
    DFont* text_font = NULL;
    if (layer->font && layer->font->default_font) {
        text_font = layer->font->default_font;
    } else if (layer->parent && layer->parent->font && layer->parent->font->default_font) {
        text_font = layer->parent->font->default_font;
    }
    
    if (component->show_percentage && text_font) {
        char percentage_text[10];
        int percentage = (int)(component->progress * 100.0f);
        snprintf(percentage_text, sizeof(percentage_text), "%d%%", percentage);
        
        // 圆形进度条用layer->color或白色，长条形用黑色
        Color text_color;
        if (component->shape == PROGRESS_SHAPE_CIRCLE) {
            if (layer->color.a > 0) {
                text_color = layer->color;
            } else {
                text_color = (Color){255, 255, 255, 255}; // 默认白色
            }
        } else {
            text_color = (Color){0, 0, 0, 255};
        }
        
        Texture* text_texture = backend_render_texture(text_font, percentage_text, text_color);
        
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