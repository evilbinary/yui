#include "animate.h"
#include "render.h"
#include "layer_update.h"

#include <math.h>

// 线性插值函数
float lerp(float start, float end, float t) {
    // 参数t通常取值范围为[0.0, 1.0]
    return start + t * (end - start);
}

// 1. 二次缓入 (Ease-In Quad): 开始时慢，然后加速
float ease_in_quad(float t) {
    return t * t;
}

// 2. 二次缓出 (Ease-Out Quad): 开始时快，然后减速
float ease_out_quad(float t) {
    return 1 - (1 - t) * (1 - t);
}

// 3. 二次缓入缓出 (Ease-In-Out Quad): 开始和结束慢，中间快
float ease_in_out_quad(float t) {
    return t < 0.5 ? 2 * t * t : 1 - pow(-2 * t + 2, 2) / 2;
}

// 4. 弹性缓动 (Ease-Out Elastic): 带有弹性振荡效果
float ease_out_elastic(float t) {
    if (t == 0.0f) return 0.0f;
    if (t == 1.0f) return 1.0f;
    float c4 = (2 * M_PI) / 3;
    return pow(2, -10 * t) * sin((t * 10 - 0.75) * c4) + 1;
}

// 使用缓动函数进行插值
float interpolate_with_easing(float start, float end, float t, float (*easing_func)(float)) {
    float eased_t = easing_func(t);
    return lerp(start, end, eased_t);
}

// 拉格朗日插值 (适用于多个数据点)
float lagrange_interpolate(float x[], float y[], int n, float xi) {
    float result = 0.0;
    for (int i = 0; i < n; i++) {
        float term = y[i];
        for (int j = 0; j < n; j++) {
            if (j != i)
                term = term * (xi - x[j]) / (x[i] - x[j]);
        }
        result += term;
    }
    return result;
}

// 创建动画对象
Animation* animation_create(float duration, float (*easing_func)(float)) {
    Animation* animation = (Animation*)malloc(sizeof(Animation));
    if (!animation) {
        return NULL;
    }
    
    // 初始化所有目标值和起始值为默认值
    animation->target_x = 0.0f;
    animation->target_y = 0.0f;
    animation->target_width = 0.0f;
    animation->target_height = 0.0f;
    animation->target_opacity = 1.0f;
    animation->target_rotation = 0.0f;
    animation->target_scale_x = 1.0f;
    animation->target_scale_y = 1.0f;
    
    animation->start_x = 0.0f;
    animation->start_y = 0.0f;
    animation->start_width = 0.0f;
    animation->start_height = 0.0f;
    animation->start_opacity = 1.0f;
    animation->start_rotation = 0.0f;
    animation->start_scale_x = 1.0f;
    animation->start_scale_y = 1.0f;
    
    animation->duration = duration;
    animation->progress = 0.0f;
    animation->easing_func = easing_func ? easing_func : ease_in_out_quad; // 默认使用缓入缓出
    
    animation->fill_mode = ANIMATION_FILL_FORWARDS;
    animation->state = ANIMATION_STATE_IDLE;
    animation->on_complete = NULL;
    
    // 初始化重复动画相关字段
    animation->repeat_type = ANIMATION_REPEAT_NONE;
    animation->repeat_count = 0;
    animation->current_repeats = 0;
    animation->reverse_on_repeat = false;
    
    return animation;
}

// 启动动画
void animation_start(Layer* layer, Animation* animation) {
    if (!layer || !animation) {
        return;
    }
    
    // 替换同层已有动画：先回退计数并释放，保证"一层一动画"计数一致
    if (layer->animation && layer->animation != animation) {
        render_animation_released(layer);
        free(layer->animation);
    }
    
    // 保存图层的当前状态作为动画的起始值
    animation->start_x = layer->rect.x;
    animation->start_y = layer->rect.y;
    animation->start_width = layer->rect.w;
    animation->start_height = layer->rect.h;
    animation->start_opacity = layer->color.a / 255.0f; // 假设alpha通道是0-255
    animation->start_rotation = layer->rotation;
    animation->start_scale_x = 1.0f; // 假设图层默认缩放为1
    animation->start_scale_y = 1.0f;
    
    // 设置动画状态
    animation->progress = 0.0f;
    animation->state = ANIMATION_STATE_RUNNING;
    
    // 如果填充模式是BACKWARDS或BOTH，则立即应用起始值
    if (animation->fill_mode == ANIMATION_FILL_BACKWARDS || animation->fill_mode == ANIMATION_FILL_BOTH) {
        // 这里可以根据需要设置起始值
    }
    
    // 将动画附加到图层
    layer->animation = animation;

    // DIRTY 模式：进入运行态，所在树 ctx 计数 +1，渲染时 root 放行遍历
    render_animation_started(layer);
}

// 停止动画
void animation_stop(Layer* layer) {
    if (!layer || !layer->animation) {
        return;
    }
    
    // 如果填充模式不是FORWARDS或BOTH，则将图层恢复到初始状态
    if (layer->animation->fill_mode != ANIMATION_FILL_FORWARDS && 
        layer->animation->fill_mode != ANIMATION_FILL_BOTH) {
        Rect old_rect = layer->rect;
        layer->rect.x = layer->animation->start_x;
        layer->rect.y = layer->animation->start_y;
        layer->rect.w = layer->animation->start_width;
        layer->rect.h = layer->animation->start_height;
        layer->color.a = layer->animation->start_opacity * 255.0f;
        layer->rotation = layer->animation->start_rotation;
        // 缩放值可以根据实际实现进行恢复
        if (old_rect.x != layer->rect.x || old_rect.y != layer->rect.y ||
            old_rect.w != layer->rect.w || old_rect.h != layer->rect.h) {
            /* DIRTY 模式：恢复初值导致位置/尺寸变化，须擦除动画末位置残留并重绘 */
            Rect r = layer->rect;
            int left = old_rect.x < r.x ? old_rect.x : r.x;
            int top = old_rect.y < r.y ? old_rect.y : r.y;
            int right = (old_rect.x + old_rect.w) > (r.x + r.w) ? (old_rect.x + old_rect.w) : (r.x + r.w);
            int bottom = (old_rect.y + old_rect.h) > (r.y + r.h) ? (old_rect.y + old_rect.h) : (r.y + r.h);
            r.x = left;
            r.y = top;
            r.w = right - left;
            r.h = bottom - top;
            render_request_redraw_rect(layer, r);
            mark_layer_dirty(layer, DIRTY_LAYOUT_RECT);
        }
    }
    
    // 释放动画资源（离开运行态：计数回退）
    render_animation_released(layer);
    free(layer->animation);
    layer->animation = NULL;
}

// 暂停动画
void animation_pause(Layer* layer) {
    if (!layer || !layer->animation) {
        return;
    }
    
    if (layer->animation->state == ANIMATION_STATE_RUNNING) {
        render_animation_released(layer);
        layer->animation->state = ANIMATION_STATE_PAUSED;
    }
}

// 恢复动画
void animation_resume(Layer* layer) {
    if (!layer || !layer->animation) {
        return;
    }
    
    if (layer->animation->state == ANIMATION_STATE_PAUSED) {
        layer->animation->state = ANIMATION_STATE_RUNNING;
        render_animation_started(layer);
    }
}

// 更新动画
void animation_update(Layer* layer, float delta_time) {
    if (!layer || !layer->animation) {
        return;
    }
    
    Animation* animation = layer->animation;
    
    // 只更新运行中的动画
    if (animation->state != ANIMATION_STATE_RUNNING) {
        return;
    }
    
    // 更新动画进度
    animation->progress += delta_time / animation->duration;
    
    // 检查动画是否完成
    if (animation->progress >= 1.0f) {
        // 处理重复动画
        bool should_repeat = false;
        
        if (animation->repeat_type == ANIMATION_REPEAT_INFINITE) {
            should_repeat = true;
        } else if (animation->repeat_type == ANIMATION_REPEAT_COUNT && 
                  animation->current_repeats < animation->repeat_count) {
            should_repeat = true;
        }
        
        if (should_repeat) {
            // 增加重复计数
            animation->current_repeats++;
            
            if (animation->reverse_on_repeat) {
                // 交换起始值和目标值以实现反向动画
                float temp;
                
                // X坐标
                temp = animation->start_x;
                animation->start_x = animation->target_x;
                animation->target_x = temp;
                
                // Y坐标
                temp = animation->start_y;
                animation->start_y = animation->target_y;
                animation->target_y = temp;
                
                // 宽度
                temp = animation->start_width;
                animation->start_width = animation->target_width;
                animation->target_width = temp;
                
                // 高度
                temp = animation->start_height;
                animation->start_height = animation->target_height;
                animation->target_height = temp;
                
                // 透明度
                temp = animation->start_opacity;
                animation->start_opacity = animation->target_opacity;
                animation->target_opacity = temp;
                
                // 旋转角度
                temp = animation->start_rotation;
                animation->start_rotation = animation->target_rotation;
                animation->target_rotation = temp;
                
                // 缩放值
                temp = animation->start_scale_x;
                animation->start_scale_x = animation->target_scale_x;
                animation->target_scale_x = temp;
                
                temp = animation->start_scale_y;
                animation->start_scale_y = animation->target_scale_y;
                animation->target_scale_y = temp;
            }
            
            // 重置动画进度
            animation->progress = 0.0f;
            animation->state = ANIMATION_STATE_RUNNING;
        } else {
            animation->progress = 1.0f;
            render_animation_released(layer);
            animation->state = ANIMATION_STATE_COMPLETED;
            
            // 如果有完成回调，则调用它
            if (animation->on_complete) {
                animation->on_complete(layer);
            }
            
            // 如果填充模式是NONE，则停止动画并恢复初始状态
            if (animation->fill_mode == ANIMATION_FILL_NONE) {
                animation_stop(layer);
                return;
            }
        }
    }
    
    // 计算缓动后的进度
    float eased_progress = animation->easing_func(animation->progress);

    // 应用所有属性的动画，并跟踪哪些属性实际发生了变化（DIRTY 渲染下需要触发重绘）
    Rect old_rect = layer->rect;
    int rect_changed = 0;

    // X坐标
    if (animation->target_x != animation->start_x) {
        layer->rect.x = interpolate_with_easing(animation->start_x, animation->target_x, eased_progress, animation->easing_func);
        rect_changed = 1;
    }
    
    // Y坐标
    if (animation->target_y != animation->start_y) {
        layer->rect.y = interpolate_with_easing(animation->start_y, animation->target_y, eased_progress, animation->easing_func);
        rect_changed = 1;
    }
    
    // 宽度
    if (animation->target_width != animation->start_width) {
        layer->rect.w = interpolate_with_easing(animation->start_width, animation->target_width, eased_progress, animation->easing_func);
        rect_changed = 1;
    }
    
    // 高度
    if (animation->target_height != animation->start_height) {
        layer->rect.h = interpolate_with_easing(animation->start_height, animation->target_height, eased_progress, animation->easing_func);
        rect_changed = 1;
    }
    
    // 透明度
    if (animation->target_opacity != animation->start_opacity) {
        float opacity = interpolate_with_easing(animation->start_opacity, animation->target_opacity, eased_progress, animation->easing_func);
        layer->color.a = opacity * 255.0f; // 转换回0-255范围
    }
    
    // 旋转角度
    if (animation->target_rotation != animation->start_rotation) {
        layer->rotation = interpolate_with_easing(animation->start_rotation, animation->target_rotation, eased_progress, animation->easing_func);
    }
    
    // 缩放值可以根据实际实现进行处理
    // ...

    /* DIRTY 模式：推进后标记重绘（传播到 root 使整树可遍历），
     * 位置/尺寸变化时请求局部重绘旧位置与新位置合并区域（擦除残留 + 画新位置）。 */
    mark_layer_dirty(layer, DIRTY_LAYOUT_RECT);
    if (rect_changed && (layer->rect.x != old_rect.x || layer->rect.y != old_rect.y ||
                         layer->rect.w != old_rect.w || layer->rect.h != old_rect.h)) {
        Rect r = layer->rect;
        int left = old_rect.x < r.x ? old_rect.x : r.x;
        int top = old_rect.y < r.y ? old_rect.y : r.y;
        int right = (old_rect.x + old_rect.w) > (r.x + r.w) ? (old_rect.x + old_rect.w) : (r.x + r.w);
        int bottom = (old_rect.y + old_rect.h) > (r.y + r.h) ? (old_rect.y + old_rect.h) : (r.y + r.h);
        r.x = left;
        r.y = top;
        r.w = right - left;
        r.h = bottom - top;
        render_request_redraw_rect(layer, r);
    }
}

// 设置动画目标属性
void animation_set_target(Animation* animation, AnimationProperty property, float value) {
    if (!animation) {
        return;
    }
    
    switch (property) {
        case ANIMATION_PROPERTY_X:
            animation->target_x = value;
            break;
        case ANIMATION_PROPERTY_Y:
            animation->target_y = value;
            break;
        case ANIMATION_PROPERTY_WIDTH:
            animation->target_width = value;
            break;
        case ANIMATION_PROPERTY_HEIGHT:
            animation->target_height = value;
            break;
        case ANIMATION_PROPERTY_OPACITY:
            // 限制在0.0-1.0范围内
            animation->target_opacity = (value < 0.0f) ? 0.0f : (value > 1.0f) ? 1.0f : value;
            break;
        case ANIMATION_PROPERTY_ROTATION:
            animation->target_rotation = value;
            break;
        case ANIMATION_PROPERTY_SCALE_X:
            animation->target_scale_x = value;
            break;
        case ANIMATION_PROPERTY_SCALE_Y:
            animation->target_scale_y = value;
            break;
        default:
            break;
    }
}

// 设置动画填充模式
void animation_set_fill_mode(Animation* animation, AnimationFillMode fill_mode) {
    if (!animation) {
        return;
    }
    
    animation->fill_mode = fill_mode;
}

// 设置动画完成回调函数
void animation_set_complete_callback(Animation* animation, void (*on_complete)(Layer*)) {
    if (!animation) {
        return;
    }
    
    animation->on_complete = on_complete;
}

// 图层动画更新函数，现在作为update_animation的简化版本
void layer_update_animation(Layer* layer) {
    // 使用默认的delta_time值，例如16ms (约60FPS)
    animation_update(layer, DEFAULT_DELTA_TIME);
}

// 设置动画重复类型
void animation_set_repeat_type(Animation* animation, AnimationRepeatType repeat_type) {
    if (!animation) {
        return;
    }
    animation->repeat_type = repeat_type;
}

// 设置动画重复次数
void animation_set_repeat_count(Animation* animation, int repeat_count) {
    if (!animation) {
        return;
    }
    animation->repeat_count = (repeat_count < 0) ? 0 : repeat_count;
}

// 设置动画重复时是否反向播放
void animation_set_reverse_on_repeat(Animation* animation, bool reverse_on_repeat) {
    if (!animation) {
        return;
    }
    animation->reverse_on_repeat = reverse_on_repeat;
}