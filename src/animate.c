#include "animate.h"

#include <math.h>

// 线性插值函数
float lerp(float start, float end, float t) {
    // 参数t通常取值范围为[0.0, 1.0]
    return start + t * (end - start);
}

// 1. 二次缓入 (Ease-In Quad): 开始时慢，然后加速[7](@ref)
float ease_in_quad(float t) {
    return t * t;
}

// 2. 二次缓出 (Ease-Out Quad): 开始时快，然后减速[7](@ref)
float ease_out_quad(float t) {
    return 1 - (1 - t) * (1 - t);
}

// 3. 二次缓入缓出 (Ease-In-Out Quad): 开始和结束慢，中间快[7](@ref)
float ease_in_out_quad(float t) {
    return t < 0.5 ? 2 * t * t : 1 - pow(-2 * t + 2, 2) / 2;
}

// 4. 弹性缓动 (Ease-Out Elastic): 带有弹性振荡效果[6](@ref)
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


void layer_update_animation(Layer* layer) {
    if(layer->animation==NULL){
        return;
    }
    
    float start_x = layer->rect.x; // 假设这是起始值
    float current_x = interpolate_with_easing(start_x, layer->animation->target_x, layer->animation->progress, layer->animation->easing_func);
    layer->rect.x = current_x;
}


