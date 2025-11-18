#ifndef YUI_SCROLLBAR_COMPONENT_H
#define YUI_SCROLLBAR_COMPONENT_H

#include "../ytype.h"

// 滚动条组件结构体
typedef struct {
    Layer* layer;          // 关联的图层
    Layer* target_layer;   // 目标滚动图层
    ScrollbarDirection direction;  // 滚动条方向
    int thickness;         // 滚动条厚度
    Color track_color;     // 轨道颜色
    Color thumb_color;     // 滑块颜色
    int is_dragging;       // 是否正在拖动
    int drag_offset;       // 拖动时的偏移量
    int visible;           // 是否可见
    Rect thumb_rect;       // 滑块矩形（相对于滚动条图层的位置）
} ScrollbarComponent;

// 函数声明
ScrollbarComponent* scrollbar_component_create(Layer* layer, Layer* target_layer, ScrollbarDirection direction);
void scrollbar_component_destroy(ScrollbarComponent* component);
void scrollbar_component_set_thickness(ScrollbarComponent* component, int thickness);
void scrollbar_component_set_colors(ScrollbarComponent* component, Color track_color, Color thumb_color);
void scrollbar_component_set_visible(ScrollbarComponent* component, int visible);
void scrollbar_component_update_position(ScrollbarComponent* component);
void scrollbar_component_handle_mouse_event(Layer* layer, MouseEvent* event);
void scrollbar_component_render(Layer* layer);
ScrollbarComponent* scrollbar_component_create_from_json(Layer* layer,Layer* target_layer, cJSON* json_obj);

#endif  // YUI_SCROLLBAR_COMPONENT_H