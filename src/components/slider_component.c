#include "slider_component.h"
#include "../backend.h"
#include "../event.h"
#include "../util.h"
#include <stdlib.h>
#include <math.h>

// 创建滑块组件
SliderComponent* slider_component_create(Layer* layer) {
    SliderComponent* component = (SliderComponent*)malloc(sizeof(SliderComponent));
    if (!component) return NULL;
    
    component->layer = layer;
    component->min_value = 0.0f;
    component->max_value = 100.0f;
    component->value = 50.0f;
    component->step = 1.0f;
    component->orientation = SLIDER_ORIENTATION_HORIZONTAL;
    component->dragging = 0;
    
    // 修复color_from_hex函数调用，直接使用Color结构体
    component->track_color = (Color){140, 140, 140, 255}; // #E0E0E0的RGB值
    component->thumb_color = (Color){0, 120, 215, 255};  // #0078D7的RGB值
    component->active_thumb_color = (Color){0, 90, 158, 255}; // #005A9E的RGB值
    
    component->user_data = NULL;
    component->on_value_changed = NULL;
    
    // 设置组件和渲染函数
    layer->component = component;
    layer->render = slider_component_render;
    layer->handle_mouse_event = slider_component_handle_mouse_event;
    layer->handle_key_event = slider_component_handle_key_event;
    
    return component;
}

// 创建滑块组件，从JSON数据
SliderComponent* slider_component_create_from_json(Layer* layer, cJSON* json) {
    if (!layer || !json) return NULL;

    SliderComponent* sliderComponent = slider_component_create(layer);
    
    // 设置默认大小（如果没有指定size）
    if (layer->rect.w <= 0) {
        layer->rect.w = 200;  // 默认宽度
    }
    if (layer->rect.h <= 0) {
        layer->rect.h = 30;   // 默认高度
    }


    // 解析滑块特定属性 - 直接从JSON对象中读取，不是从sliderConfig
    cJSON* min = cJSON_GetObjectItem(json, "min");
    cJSON* max = cJSON_GetObjectItem(json, "max");
    cJSON* value = cJSON_GetObjectItem(json, "data");  // JSON中使用data字段
    cJSON* step = cJSON_GetObjectItem(json, "step");
    cJSON* orientation = cJSON_GetObjectItem(json, "orientation");

    
    // 设置范围
    if (min && max) {
        slider_component_set_range(sliderComponent, 
            (float)min->valuedouble, (float)max->valuedouble);
    }
    
    // 设置当前值
    if (value) {
        slider_component_set_value(sliderComponent, (float)value->valuedouble);
    }
    
    // 设置步长
    if (step) {
        slider_component_set_step(sliderComponent, (float)step->valuedouble);
    }
    
    // 设置方向 - 支持字符串和数字两种格式
    if (orientation) {
        if (orientation->valuestring != NULL && strcmp(orientation->valuestring, "vertical") == 0) {
            slider_component_set_orientation(sliderComponent, SLIDER_ORIENTATION_VERTICAL);
        } else if (orientation->valuedouble == 1.0) {
            slider_component_set_orientation(sliderComponent, SLIDER_ORIENTATION_VERTICAL);
        } else {
        }
    }
    
    // 解析样式
    cJSON* style = cJSON_GetObjectItem(json, "style");
    if (style) {
        Color trackColor = {224, 224, 224, 255};  // #E0E0E0
        Color thumbColor = {33, 150, 243, 255};   // #2196F3
        Color activeThumbColor = {25, 118, 210, 255}; // #1976D2
        
        if (cJSON_HasObjectItem(style, "trackColor")) {
            parse_color(cJSON_GetObjectItem(style, "trackColor")->valuestring, &trackColor);
        }
        if (cJSON_HasObjectItem(style, "thumbColor")) {
            parse_color(cJSON_GetObjectItem(style, "thumbColor")->valuestring, &thumbColor);
        }
        if (cJSON_HasObjectItem(style, "activeThumbColor")) {
            parse_color(cJSON_GetObjectItem(style, "activeThumbColor")->valuestring, &activeThumbColor);
        }
        
        slider_component_set_colors(sliderComponent, trackColor, thumbColor, activeThumbColor);
    }
    
    return sliderComponent;
}

// 销毁滑块组件
void slider_component_destroy(SliderComponent* component) {
    if (!component) return;
    free(component);
}

// 设置范围
void slider_component_set_range(SliderComponent* component, float min_value, float max_value) {
    if (!component || min_value >= max_value) return;
    
    component->min_value = min_value;
    component->max_value = max_value;
    
    // 调整当前值，确保在范围内
    if (component->value < min_value) {
        component->value = min_value;
    } else if (component->value > max_value) {
        component->value = max_value;
    }
}

// 设置值
void slider_component_set_value(SliderComponent* component, float value) {
    if (!component) return;
    
    float old_value = component->value;
    
    // 限制值在范围内
    if (value < component->min_value) {
        value = component->min_value;
    } else if (value > component->max_value) {
        value = component->max_value;
    }
    
    // 应用步长
    if (component->step > 0) {
        float steps = roundf((value - component->min_value) / component->step);
        value = component->min_value + steps * component->step;
    }
    
    component->value = value;
    
    // 调用回调函数
    if (old_value != value && component->on_value_changed) {
        component->on_value_changed(value, component->user_data);
    }
}

// 获取值
float slider_component_get_value(SliderComponent* component) {
    if (!component) return 0.0f;
    return component->value;
}

// 设置步长
void slider_component_set_step(SliderComponent* component, float step) {
    if (!component || step < 0) return;
    component->step = step;
}

// 设置方向
void slider_component_set_orientation(SliderComponent* component, int orientation) {
    if (!component) return;
    component->orientation = orientation;
}

// 设置颜色
void slider_component_set_colors(SliderComponent* component, Color track, Color thumb, Color active_thumb) {
    if (!component) return;
    component->track_color = track;
    component->thumb_color = thumb;
    component->active_thumb_color = active_thumb;
}

// 设置用户数据
void slider_component_set_user_data(SliderComponent* component, void* data) {
    if (!component) return;
    component->user_data = data;
}

// 设置值变化回调
void slider_component_set_value_changed_callback(SliderComponent* component, void (*callback)(float, void*)) {
    if (!component) return;
    component->on_value_changed = callback;
}

// 计算滑块位置
int slider_calculate_thumb_position(SliderComponent* component) {
    if (!component) return 0;
    
    float range = component->max_value - component->min_value;
    float normalized_value = 0.5f;  // 默认在中间位置
    
    // 防止除零错误
    if (range > 0) {
        normalized_value = (component->value - component->min_value) / range;
        // 限制在0-1范围内
        if (normalized_value < 0.0f) normalized_value = 0.0f;
        if (normalized_value > 1.0f) normalized_value = 1.0f;
    }
    
    if (component->orientation == SLIDER_ORIENTATION_HORIZONTAL) {
        int track_width = component->layer->rect.w - 20;  // 留出边距
        return component->layer->rect.x + 10 + (int)(normalized_value * track_width);
    } else {
        int track_height = component->layer->rect.h - 20;  // 留出边距
        return component->layer->rect.y + 10 + (int)((1.0f - normalized_value) * track_height);
    }
}

// 根据鼠标位置计算值
float slider_calculate_value_from_position(SliderComponent* component, int mouse_x, int mouse_y) {
    if (!component) return 0.0f;
    
    float normalized_pos;
    
    if (component->orientation == SLIDER_ORIENTATION_HORIZONTAL) {
        int track_width = component->layer->rect.w - 20;
        if (track_width <= 0) return component->value;  // 防止除零
        int track_x = component->layer->rect.x + 10;
        normalized_pos = (float)(mouse_x - track_x) / track_width;
    } else {
        int track_height = component->layer->rect.h - 20;
        if (track_height <= 0) return component->value;  // 防止除零
        int track_y = component->layer->rect.y + 10;
        normalized_pos = 1.0f - (float)(mouse_y - track_y) / track_height;
    }
    
    // 限制在0-1范围内
    if (normalized_pos < 0.0f) normalized_pos = 0.0f;
    if (normalized_pos > 1.0f) normalized_pos = 1.0f;
    
    // 转换为实际值
    float range = component->max_value - component->min_value;
    float value;
    
    if (range > 0) {
        value = component->min_value + normalized_pos * range;
        // 应用步长
        if (component->step > 0) {
            float steps = roundf((value - component->min_value) / component->step);
            value = component->min_value + steps * component->step;
        }
    } else {
        value = component->value;  // 如果范围为0，返回当前值
    }
    
    return value;
}

// 处理鼠标事件
void slider_component_handle_mouse_event(Layer* layer, MouseEvent* event) {
    if (!layer || !event || !layer->component) return;
    
    SliderComponent* component = (SliderComponent*)layer->component;
    // 只在按下事件时输出调试信息
    if (event->state == SDL_PRESSED && event->button == SDL_BUTTON_LEFT) {
        printf("DEBUG: Mouse CLICK on slider at (%d,%d)\n", layer->rect.x, layer->rect.y);
    }
    
    // 首先检查鼠标是否在滑块区域内
    if (event->x < layer->rect.x || event->x >= layer->rect.x + layer->rect.w ||
        event->y < layer->rect.y || event->y >= layer->rect.y + layer->rect.h) {
        // 鼠标不在滑块区域内，只处理拖动释放事件
        if (event->state == SDL_RELEASED && event->button == SDL_BUTTON_LEFT) {
            component->dragging = 0;
        }
        return;
    }
    
    if (event->state == SDL_PRESSED && event->button == SDL_BUTTON_LEFT) {
        // 检查是否点击在滑块上
        int thumb_pos = slider_calculate_thumb_position(component);
        int thumb_size = 16;
        int thumb_x, thumb_y;
        
        if (component->orientation == SLIDER_ORIENTATION_HORIZONTAL) {
            thumb_x = thumb_pos - thumb_size / 2;
            thumb_y = layer->rect.y + layer->rect.h / 2 - thumb_size / 2;
        } else {
            thumb_x = layer->rect.x + layer->rect.w / 2 - thumb_size / 2;
            thumb_y = thumb_pos - thumb_size / 2;
        }
        
        if (event->x >= thumb_x && event->x < thumb_x + thumb_size &&
            event->y >= thumb_y && event->y < thumb_y + thumb_size) {
            component->dragging = 1;
        } else {
            // 如果点击在轨道上，直接跳转到该位置
            float new_value = slider_calculate_value_from_position(component, event->x, event->y);
            slider_component_set_value(component, new_value);
        }
    } else if (event->state == SDL_RELEASED && event->button == SDL_BUTTON_LEFT) {
        component->dragging = 0;
    } else if (event->button == SDL_BUTTON_LEFT && component->dragging) {
        // 拖动滑块（鼠标移动时）
        float new_value = slider_calculate_value_from_position(component, event->x, event->y);
        slider_component_set_value(component, new_value);
    }
}

// 处理键盘事件
void slider_component_handle_key_event(Layer* layer, KeyEvent* event) {
    if (!layer || !event || !layer->component) return;
    
    SliderComponent* component = (SliderComponent*)layer->component;
    
    if (event->type == KEY_EVENT_DOWN) {
        float new_value = component->value;
        
        switch (event->data.key.key_code) {
            case SDLK_LEFT:
            case SDLK_DOWN:
                if (component->orientation == SLIDER_ORIENTATION_HORIZONTAL && event->data.key.key_code == SDLK_LEFT) {
                    new_value -= component->step;
                } else if (component->orientation == SLIDER_ORIENTATION_VERTICAL && event->data.key.key_code == SDLK_DOWN) {
                    new_value -= component->step;
                }
                break;
                
            case SDLK_RIGHT:
            case SDLK_UP:
                if (component->orientation == SLIDER_ORIENTATION_HORIZONTAL && event->data.key.key_code == SDLK_RIGHT) {
                    new_value += component->step;
                } else if (component->orientation == SLIDER_ORIENTATION_VERTICAL && event->data.key.key_code == SDLK_UP) {
                    new_value += component->step;
                }
                break;
                
            case SDLK_HOME:
                new_value = component->min_value;
                break;
                
            case SDLK_END:
                new_value = component->max_value;
                break;
        }
        
        slider_component_set_value(component, new_value);
    }
}

// 渲染滑块
void slider_component_render(Layer* layer) {
    if (!layer || !layer->component) return;
    
    SliderComponent* component = (SliderComponent*)layer->component;
    
    // 检查layer的尺寸是否有效
    if (layer->rect.w <= 0 || layer->rect.h <= 0) {
        printf("DEBUG: Invalid layer dimensions, skipping render\n");
        return;
    }
    
    // 绘制轨道
    if (component->orientation == SLIDER_ORIENTATION_HORIZONTAL) {
        // 水平轨道
        int track_y = layer->rect.y + layer->rect.h / 2 - 3;  // 增加轨道高度到6像素
        int track_height = 6;  // 增加轨道高度，使其更明显
        int track_x = layer->rect.x + 10;
        int track_width = layer->rect.w - 20;
        

        
        // 修复调用：创建Rect结构体
        Rect track_rect = {track_x, track_y, track_width, track_height};
        backend_render_rounded_rect(&track_rect, component->track_color, 3);
        
        // 绘制进度
        int thumb_pos = slider_calculate_thumb_position(component);
        int progress_width = thumb_pos - track_x;
        if (progress_width > 0) {

            // 修复调用：创建进度条的Rect结构体
            Rect progress_rect = {track_x, track_y, progress_width, track_height};
            backend_render_rounded_rect(&progress_rect, component->thumb_color, 3);
        }
    } else {
        // 垂直轨道
        int track_x = layer->rect.x + layer->rect.w / 2 - 4;  // 增加轨道宽度到8像素
        int track_width = 8;  // 增加轨道宽度，使其更明显
        int track_y = layer->rect.y + 10;
        int track_height = layer->rect.h - 20;
        

        
        // 使用更深的轨道颜色使其更明显
        Color dark_track_color = {100, 100, 100, 255};  // 更深的灰色
        Rect track_rect = {track_x, track_y, track_width, track_height};
        backend_render_rounded_rect(&track_rect, dark_track_color, 4);
        
        // 绘制进度
        int thumb_pos = slider_calculate_thumb_position(component);
        int progress_height = track_y + track_height - thumb_pos;
        if (progress_height > 0) {

            // 修复调用：创建进度条的Rect结构体
            Rect progress_rect = {track_x, thumb_pos, track_width, progress_height};
            backend_render_rounded_rect(&progress_rect, component->thumb_color, 4);
        }
    }
    
    // 绘制滑块
    int thumb_size = 16;
    int thumb_pos = slider_calculate_thumb_position(component);
    int thumb_x, thumb_y;
    
    if (component->orientation == SLIDER_ORIENTATION_HORIZONTAL) {
        thumb_x = thumb_pos - thumb_size / 2;
        thumb_y = layer->rect.y + layer->rect.h / 2 - thumb_size / 2;
    } else {
        thumb_x = layer->rect.x + layer->rect.w / 2 - thumb_size / 2;
        thumb_y = thumb_pos - thumb_size / 2;
    }
    
    Color thumb_color = component->dragging ? component->active_thumb_color : component->thumb_color;
    // 修复调用：创建滑块的Rect结构体
    Rect thumb_rect = {thumb_x, thumb_y, thumb_size, thumb_size};
    backend_render_rounded_rect(&thumb_rect, thumb_color, thumb_size / 2);
}