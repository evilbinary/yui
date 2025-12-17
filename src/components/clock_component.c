#include "clock_component.h"
#include "../backend.h"
#include "../event.h"
#include "../util.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

// 创建时钟组件
ClockComponent* clock_component_create(Layer* layer) {
    ClockComponent* component = (ClockComponent*)malloc(sizeof(ClockComponent));
    if (!component) return NULL;
    
    component->layer = layer;
    component->radius = 90;
    component->hour_hand_color = (Color){51, 51, 51, 255}; // #333
    component->minute_hand_color = (Color){102, 102, 102, 255}; // #666
    component->second_hand_color = (Color){255, 0, 0, 255}; // #FF0000
    component->center_color = (Color){255, 0, 0, 255}; // #FF0000
    component->border_color = (Color){51, 51, 51, 255}; // #333
    component->background_color = (Color){255, 255, 255, 255}; // #FFFFFF
    component->hour_hand_length = 60;
    component->minute_hand_length = 80;
    component->second_hand_length = 90;
    component->hand_width = 4;
    component->center_radius = 5;
    component->border_width = 2;
    component->show_numbers = true;
    component->number_color = (Color){51, 51, 51, 255}; // #333
    component->font_size = 14;
    component->smooth_second = false;
    component->smooth_second_angle = 0;
    
    // 获取当前时间
    clock_component_update_time(component);
    
    // 设置组件
    layer->component = component;
    layer->render = clock_component_render;
    
    return component;
}

// 销毁时钟组件
void clock_component_destroy(ClockComponent* component) {
    if (!component) return;
    free(component);
}

// 更新时间
void clock_component_update_time(ClockComponent* component) {
    if (!component) return;
    
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    
    component->hour = tm_info->tm_hour % 12;
    component->minute = tm_info->tm_min;
    component->second = tm_info->tm_sec;
    
    // 计算平滑秒针角度
    if (component->smooth_second) {
        component->smooth_second_angle = component->second * 6.0; // 每秒6度
    }
}

// 设置时间
void clock_component_set_time(ClockComponent* component, int hour, int minute, int second) {
    if (!component) return;
    
    component->hour = hour % 12;
    component->minute = minute;
    component->second = second;
    
    if (component->smooth_second) {
        component->smooth_second_angle = second * 6.0;
    }
}

// 设置半径
void clock_component_set_radius(ClockComponent* component, int radius) {
    if (!component) return;
    component->radius = radius;
}

// 设置颜色
void clock_component_set_colors(ClockComponent* component, 
                               Color hour_hand, Color minute_hand, Color second_hand,
                               Color center, Color border, Color background) {
    if (!component) return;
    
    component->hour_hand_color = hour_hand;
    component->minute_hand_color = minute_hand;
    component->second_hand_color = second_hand;
    component->center_color = center;
    component->border_color = border;
    component->background_color = background;
}

// 设置是否显示数字
void clock_component_set_show_numbers(ClockComponent* component, bool show) {
    if (!component) return;
    component->show_numbers = show;
}

// 设置数字颜色
void clock_component_set_number_color(ClockComponent* component, Color color) {
    if (!component) return;
    component->number_color = color;
}

// 设置字体大小
void clock_component_set_font_size(ClockComponent* component, int size) {
    if (!component) return;
    component->font_size = size;
}

// 设置是否平滑秒针
void clock_component_set_smooth_second(ClockComponent* component, bool smooth) {
    if (!component) return;
    component->smooth_second = smooth;
}

// 从JSON创建时钟组件
ClockComponent* clock_component_create_from_json(Layer* layer, cJSON* json_obj) {
    if (!layer || !json_obj) return NULL;
    
    ClockComponent* component = clock_component_create(layer);
    if (!component) return NULL;
    
    // 解析时钟特定属性
    cJSON* clockConfig = cJSON_GetObjectItem(json_obj, "clockConfig");
    if (clockConfig) {
        // 半径
        if (cJSON_HasObjectItem(clockConfig, "radius")) {
            component->radius = cJSON_GetObjectItem(clockConfig, "radius")->valueint;
        }
        
        // 边框宽度
        if (cJSON_HasObjectItem(clockConfig, "borderWidth")) {
            component->border_width = cJSON_GetObjectItem(clockConfig, "borderWidth")->valueint;
        }
        
        // 指针长度
        if (cJSON_HasObjectItem(clockConfig, "hourHandLength")) {
            component->hour_hand_length = cJSON_GetObjectItem(clockConfig, "hourHandLength")->valueint;
        }
        if (cJSON_HasObjectItem(clockConfig, "minuteHandLength")) {
            component->minute_hand_length = cJSON_GetObjectItem(clockConfig, "minuteHandLength")->valueint;
        }
        if (cJSON_HasObjectItem(clockConfig, "secondHandLength")) {
            component->second_hand_length = cJSON_GetObjectItem(clockConfig, "secondHandLength")->valueint;
        }
        
        // 指针宽度
        if (cJSON_HasObjectItem(clockConfig, "handWidth")) {
            component->hand_width = cJSON_GetObjectItem(clockConfig, "handWidth")->valueint;
        }
        
        // 中心圆半径
        if (cJSON_HasObjectItem(clockConfig, "centerRadius")) {
            component->center_radius = cJSON_GetObjectItem(clockConfig, "centerRadius")->valueint;
        }
        
        // 显示数字
        if (cJSON_HasObjectItem(clockConfig, "showNumbers")) {
            component->show_numbers = cJSON_GetObjectItem(clockConfig, "showNumbers")->valueint;
        }
        
        // 字体大小
        if (cJSON_HasObjectItem(clockConfig, "fontSize")) {
            component->font_size = cJSON_GetObjectItem(clockConfig, "fontSize")->valueint;
        }
        
        // 平滑秒针
        if (cJSON_HasObjectItem(clockConfig, "smoothSecond")) {
            component->smooth_second = cJSON_GetObjectItem(clockConfig, "smoothSecond")->valueint;
        }
        
        // 解析颜色
        if (cJSON_HasObjectItem(clockConfig, "hourHandColor")) {
            parse_color(cJSON_GetObjectItem(clockConfig, "hourHandColor")->valuestring, &component->hour_hand_color);
        }
        if (cJSON_HasObjectItem(clockConfig, "minuteHandColor")) {
            parse_color(cJSON_GetObjectItem(clockConfig, "minuteHandColor")->valuestring, &component->minute_hand_color);
        }
        if (cJSON_HasObjectItem(clockConfig, "secondHandColor")) {
            parse_color(cJSON_GetObjectItem(clockConfig, "secondHandColor")->valuestring, &component->second_hand_color);
        }
        if (cJSON_HasObjectItem(clockConfig, "centerColor")) {
            parse_color(cJSON_GetObjectItem(clockConfig, "centerColor")->valuestring, &component->center_color);
        }
        if (cJSON_HasObjectItem(clockConfig, "borderColor")) {
            parse_color(cJSON_GetObjectItem(clockConfig, "borderColor")->valuestring, &component->border_color);
        }
        if (cJSON_HasObjectItem(clockConfig, "backgroundColor")) {
            parse_color(cJSON_GetObjectItem(clockConfig, "backgroundColor")->valuestring, &component->background_color);
        }
        if (cJSON_HasObjectItem(clockConfig, "numberColor")) {
            parse_color(cJSON_GetObjectItem(clockConfig, "numberColor")->valuestring, &component->number_color);
        }
    }
    
    return component;
}

// 渲染时钟
void clock_component_render(Layer* layer) {
    ClockComponent* component = (ClockComponent*)layer->component;
    if (!component) return;
    
    // 更新时间
    clock_component_update_time(component);
    
    int center_x = layer->rect.x + layer->rect.w / 2;
    int center_y = layer->rect.y + layer->rect.h / 2;
    
    // 绘制背景圆
    Rect bg_rect = {layer->rect.x, layer->rect.y, layer->rect.w, layer->rect.h};
    backend_render_rounded_rect_with_border(&bg_rect, component->background_color, 
                                           layer->rect.w / 2, component->border_width, component->border_color);
    
    // 绘制数字
    if (component->show_numbers) {
        DFont* font = backend_load_font("/System/Library/Fonts/Helvetica.ttc", component->font_size);
        if (!font && layer->font) {
            font = backend_load_font(layer->font->path, component->font_size);
        }
        
        if (font) {
            for (int i = 1; i <= 12; i++) {
                double angle = (i * 30 - 90) * M_PI / 180.0; // 12点位置开始，顺时针
                int number_x = center_x + (int)(component->radius * 0.8 * cos(angle)) - 5;
                int number_y = center_y + (int)(component->radius * 0.8 * sin(angle)) + 5;
                
                char number_str[3];
                snprintf(number_str, sizeof(number_str), "%d", i);
                
                Texture* text_texture = backend_render_texture(font, number_str, component->number_color);
                if (text_texture) {
                    Rect text_rect = {number_x, number_y, 0, 0};
                    backend_query_texture(text_texture, NULL, NULL, &text_rect.w, &text_rect.h);
                    text_rect.x -= text_rect.w / 2;
                    text_rect.y -= text_rect.h / 2;
                    backend_render_text_copy(text_texture, NULL, &text_rect);
                    backend_render_text_destroy(text_texture);
                }
            }
        }
    }
    
    // 计算角度
    double hour_angle = (component->hour * 30 + component->minute * 0.5 - 90) * M_PI / 180.0;
    double minute_angle = (component->minute * 6 + component->second * 0.1 - 90) * M_PI / 180.0;
    double second_angle;
    
    if (component->smooth_second) {
        second_angle = (component->smooth_second_angle - 90) * M_PI / 180.0;
    } else {
        second_angle = (component->second * 6 - 90) * M_PI / 180.0;
    }
    
    // 绘制时针
    int hour_end_x = center_x + (int)(component->hour_hand_length * cos(hour_angle));
    int hour_end_y = center_y + (int)(component->hour_hand_length * sin(hour_angle));
    backend_render_line(center_x, center_y, hour_end_x, hour_end_y, component->hour_hand_color);
    
    // 绘制分针
    int minute_end_x = center_x + (int)(component->minute_hand_length * cos(minute_angle));
    int minute_end_y = center_y + (int)(component->minute_hand_length * sin(minute_angle));
    backend_render_line(center_x, center_y, minute_end_x, minute_end_y, component->minute_hand_color);
    
    // 绘制秒针
    int second_end_x = center_x + (int)(component->second_hand_length * cos(second_angle));
    int second_end_y = center_y + (int)(component->second_hand_length * sin(second_angle));
    backend_render_line(center_x, center_y, second_end_x, second_end_y, component->second_hand_color);
    
    // 绘制中心圆
    Rect center_rect = {center_x - component->center_radius, center_y - component->center_radius,
                       component->center_radius * 2, component->center_radius * 2};
    backend_render_rounded_rect(&center_rect, component->center_color, component->center_radius);
}