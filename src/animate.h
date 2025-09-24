#ifndef YUI_ANIMATE_H
#define YUI_ANIMATE_H

#include "layer.h"
#include "ytype.h"

// 动画属性类型枚举
typedef enum {
    ANIMATION_PROPERTY_X,        // X坐标
    ANIMATION_PROPERTY_Y,        // Y坐标
    ANIMATION_PROPERTY_WIDTH,    // 宽度
    ANIMATION_PROPERTY_HEIGHT,   // 高度
    ANIMATION_PROPERTY_OPACITY,  // 透明度
    ANIMATION_PROPERTY_ROTATION, // 旋转角度
    ANIMATION_PROPERTY_SCALE_X,  // X轴缩放
    ANIMATION_PROPERTY_SCALE_Y   // Y轴缩放
} AnimationProperty;

// 动画填充模式枚举
typedef enum {
    ANIMATION_FILL_NONE,      // 动画结束后回到初始状态
    ANIMATION_FILL_FORWARDS,  // 动画结束后保持最终状态
    ANIMATION_FILL_BACKWARDS, // 动画开始前应用初始状态
    ANIMATION_FILL_BOTH       // 同时应用FORWARDS和BACKWARDS
} AnimationFillMode;

// 动画状态枚举
typedef enum {
    ANIMATION_STATE_IDLE,     // 空闲状态
    ANIMATION_STATE_RUNNING,  // 运行中
    ANIMATION_STATE_PAUSED,   // 暂停
    ANIMATION_STATE_COMPLETED // 完成
} AnimationState;

// 扩展动画结构体
typedef struct Animation {
    float target_x;         // 目标X坐标
    float target_y;         // 目标Y坐标
    float target_width;     // 目标宽度
    float target_height;    // 目标高度
    float target_opacity;   // 目标透明度
    float target_rotation;  // 目标旋转角度
    float target_scale_x;   // 目标X轴缩放
    float target_scale_y;   // 目标Y轴缩放
    
    float start_x;          // 起始X坐标
    float start_y;          // 起始Y坐标
    float start_width;      // 起始宽度
    float start_height;     // 起始高度
    float start_opacity;    // 起始透明度
    float start_rotation;   // 起始旋转角度
    float start_scale_x;    // 起始X轴缩放
    float start_scale_y;    // 起始Y轴缩放
    
    float duration;         // 动画持续时间（秒）
    float progress;         // 动画进度 [0.0, 1.0]
    float (*easing_func)(float); // 缓动函数
    
    AnimationFillMode fill_mode; // 填充模式
    AnimationState state;       // 动画状态
    
    // 动画完成回调函数
    void (*on_complete)(Layer* layer);
} Animation;

// 缓动函数
extern float ease_in_quad(float t);
extern float ease_out_quad(float t);
extern float ease_in_out_quad(float t);
extern float ease_out_elastic(float t);

// 插值函数
extern float lerp(float start, float end, float t);
extern float interpolate_with_easing(float start, float end, float t, float (*easing_func)(float));
extern float lagrange_interpolate(float x[], float y[], int n, float xi);

// 动画管理函数
Animation* create_animation(float duration, float (*easing_func)(float));
void start_animation(Layer* layer, Animation* animation);
void stop_animation(Layer* layer);
void pause_animation(Layer* layer);
void resume_animation(Layer* layer);
void update_animation(Layer* layer, float delta_time);

// 设置动画目标属性
void set_animation_target(Animation* animation, AnimationProperty property, float value);
void set_animation_fill_mode(Animation* animation, AnimationFillMode fill_mode);
void set_animation_complete_callback(Animation* animation, void (*on_complete)(Layer*));

#endif