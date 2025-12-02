#ifndef YUI_SLIDER_COMPONENT_H
#define YUI_SLIDER_COMPONENT_H

#include "../ytype.h"

// 滑块组件结构体
typedef struct {
    Layer* layer;             // 关联的图层
    float min_value;          // 最小值
    float max_value;          // 最大值
    float value;              // 当前值
    float step;               // 步长
    int orientation;          // 方向 (0: 水平, 1: 垂直)
    int dragging;             // 是否正在拖动
    Color track_color;        // 轨道颜色
    Color thumb_color;        // 滑块颜色
    Color active_thumb_color; // 激活状态滑块颜色
    void* user_data;          // 用户数据
    
    // 回调函数
    void (*on_value_changed)(float value, void* user_data);
} SliderComponent;

// 方向常量
#define SLIDER_ORIENTATION_HORIZONTAL 0
#define SLIDER_ORIENTATION_VERTICAL 1

// 函数声明
SliderComponent* slider_component_create(Layer* layer);

// 从JSON创建滑块组件
SliderComponent* slider_component_create_from_json(Layer* layer, cJSON* json);
void slider_component_destroy(SliderComponent* component);
void slider_component_set_range(SliderComponent* component, float min_value, float max_value);
void slider_component_set_value(SliderComponent* component, float value);
float slider_component_get_value(SliderComponent* component);
void slider_component_set_step(SliderComponent* component, float step);
void slider_component_set_orientation(SliderComponent* component, int orientation);
void slider_component_set_colors(SliderComponent* component, Color track, Color thumb, Color active_thumb);
void slider_component_set_user_data(SliderComponent* component, void* data);
void slider_component_set_value_changed_callback(SliderComponent* component, void (*callback)(float, void*));
void slider_component_handle_mouse_event(Layer* layer, MouseEvent* event);
void slider_component_handle_key_event(Layer* layer, KeyEvent* event);
void slider_component_render(Layer* layer);

#endif  // YUI_SLIDER_COMPONENT_H