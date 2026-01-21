#ifndef YUI_PROGRESS_COMPONENT_H
#define YUI_PROGRESS_COMPONENT_H

#include "../ytype.h"

// 进度条形状枚举
typedef enum {
    PROGRESS_SHAPE_RECTANGLE,  // 长条形
    PROGRESS_SHAPE_CIRCLE      // 圆形
} ProgressShape;

// 进度条方向枚举
typedef enum {
    PROGRESS_DIRECTION_HORIZONTAL,
    PROGRESS_DIRECTION_VERTICAL
} ProgressDirection;

// 进度条组件结构体
typedef struct {
    Layer* layer;          // 关联的图层
    float progress;        // 当前进度 (0.0 到 1.0)
    float target_progress; // 目标进度 (用于动画)
    float animation_speed; // 动画速度 (0.0-1.0)
    int animating;         // 是否正在进行动画
    ProgressShape shape;   // 进度条形状
    ProgressDirection direction; // 进度条方向（用于长条形）
    Color fill_color;      // 填充颜色
    int show_percentage;   // 是否显示百分比文本
    int circle_width;      // 圆形进度条宽度（圆环厚度）
} ProgressComponent;

// 函数声明
ProgressComponent* progress_component_create(Layer* layer);
ProgressComponent* progress_component_create_from_json(Layer* layer, cJSON* json_obj);
void progress_component_destroy(ProgressComponent* component);
void progress_component_set_progress(ProgressComponent* component, float progress);
void progress_component_set_direction(ProgressComponent* component, ProgressDirection direction);
void progress_component_set_fill_color(ProgressComponent* component, Color color);
void progress_component_set_background_color(ProgressComponent* component, Color color);
void progress_component_set_show_percentage(ProgressComponent* component, int show);
void progress_component_set_animation_speed(ProgressComponent* component, float speed);
void progress_component_set_shape(ProgressComponent* component, ProgressShape shape);
void progress_component_set_circle_width(ProgressComponent* component, int width);
void progress_component_render(Layer* layer);

#endif  // YUI_PROGRESS_COMPONENT_H