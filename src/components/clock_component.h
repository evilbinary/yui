#ifndef YUI_CLOCK_COMPONENT_H
#define YUI_CLOCK_COMPONENT_H

#include "../layer.h"
#include "../ytype.h"

#ifdef __cplusplus
extern "C" {
#endif

// 时钟组件
typedef struct ClockComponent {
    Layer* layer;
    int hour;
    int minute;
    int second;
    int radius;
    Color hour_hand_color;
    Color minute_hand_color;
    Color second_hand_color;
    Color center_color;
    Color border_color;
    Color background_color;
    int hour_hand_length;
    int minute_hand_length;
    int second_hand_length;
    int hand_width;
    int center_radius;
    int border_width;
    bool show_numbers;
    Color number_color;
    int font_size;
    bool smooth_second; // 平滑秒针移动
    double smooth_second_angle; // 平滑秒针角度
} ClockComponent;

// 创建时钟组件
ClockComponent* clock_component_create(Layer* layer);

// 销毁时钟组件
void clock_component_destroy(ClockComponent* component);

// 更新时间
void clock_component_update_time(ClockComponent* component);

// 设置时间
void clock_component_set_time(ClockComponent* component, int hour, int minute, int second);

// 设置半径
void clock_component_set_radius(ClockComponent* component, int radius);

// 设置颜色
void clock_component_set_colors(ClockComponent* component, 
                               Color hour_hand, Color minute_hand, Color second_hand,
                               Color center, Color border, Color background);

// 设置是否显示数字
void clock_component_set_show_numbers(ClockComponent* component, bool show);

// 设置数字颜色
void clock_component_set_number_color(ClockComponent* component, Color color);

// 设置字体大小
void clock_component_set_font_size(ClockComponent* component, int size);

// 设置是否平滑秒针
void clock_component_set_smooth_second(ClockComponent* component, bool smooth);

// 从JSON创建时钟组件
ClockComponent* clock_component_create_from_json(Layer* layer, cJSON* json_obj);

// 处理渲染
void clock_component_render(Layer* layer);

#ifdef __cplusplus
}
#endif

#endif // CLOCK_COMPONENT_H