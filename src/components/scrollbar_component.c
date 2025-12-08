#include "scrollbar_component.h"
#include "../render.h"
#include "../backend.h"
#include "../util.h"
#include <stdlib.h>
#include <string.h>

// 前向声明parse_color函数
extern void parse_color(char* color_string, Color* color);

// 创建滚动条组件
ScrollbarComponent* scrollbar_component_create(Layer* layer, Layer* target_layer, ScrollbarDirection direction) {
    if (!layer) {
        return NULL;
    }
    
    ScrollbarComponent* component = (ScrollbarComponent*)malloc(sizeof(ScrollbarComponent));
    if (!component) {
        return NULL;
    }
    
    memset(component, 0, sizeof(ScrollbarComponent));
    component->layer = layer;
    component->target_layer = target_layer;
    component->direction = direction;
    component->thickness = 8;  // 默认厚度
    component->track_color = (Color){200, 200, 200, 255};  // 默认轨道颜色
    component->thumb_color = (Color){100, 100, 100, 255};  // 默认滑块颜色
    component->is_dragging = 0;
    component->drag_offset = 0;
    component->visible = 1;
    
    // 组件内部创建滑块图层
    component->thumb_rect.x = 0;
    component->thumb_rect.y = 0;
    component->thumb_rect.w = 20; // 默认宽度
    component->thumb_rect.h = 20; // 默认高度
    
    // 设置组件指针和自定义渲染函数
    layer->component = component;
    layer->render = scrollbar_component_render;
    
    // 绑定事件处理函数
    layer->handle_mouse_event = scrollbar_component_handle_mouse_event;
    // 初始更新位置
    scrollbar_component_update_position(component);
    
    return component;
}

// 更新滚动条位置和大小
void scrollbar_component_update_position(ScrollbarComponent* component) {
    // 全面的参数检查
    if (!component || !component->target_layer || !component->layer) {
        return;
    }
    
    printf("scrollbar_component_update_position: updating position, visible=%d\n", component->visible);
    
    Layer* target = component->target_layer;
    Layer* scrollbar_layer = component->layer;
    
    // 确保target的children数组存在
    if (!target->children) {
        return;
    }
    
    if (component->direction == SCROLLBAR_DIRECTION_VERTICAL) {
        // 计算内容高度和可见高度
        int content_height;
        int visible_height = target->rect.h;
        
        // 优先使用 target->content_height（如果可用），这是组件设置的内容高度
        if (target->content_height > 0) {
            content_height = target->content_height;
        } else {
            // 回退到计算所有子图层的总高度
            content_height = 0;
            for (int i = 0; i < target->child_count; i++) {
                Layer* child = target->children[i];
                if (child == NULL) continue;
                if (child->rect.y + child->rect.h > content_height) {
                    content_height = child->rect.y + child->rect.h;
                }
            }
        }
        
        // 如果内容高度小于等于可见高度，隐藏滚动条
        if (content_height <= visible_height) {
            scrollbar_component_set_visible(component, 0);
            return;
        } else {
            scrollbar_component_set_visible(component, 1);
        }
        
        // 设置滚动条位置和大小
        scrollbar_layer->rect.x = target->rect.x + target->rect.w - component->thickness;
        scrollbar_layer->rect.y = target->rect.y;
        scrollbar_layer->rect.w = component->thickness;
        scrollbar_layer->rect.h = target->rect.h;
        
        // 计算滑块高度和位置
        float ratio = (float)visible_height / content_height;
        int thumb_height = (int)(visible_height * ratio);
        if (thumb_height < 20) thumb_height = 20;  // 最小高度
        
        int max_scroll_offset = content_height - visible_height;
        // 防止除零错误
        if (max_scroll_offset <= 0) {
            max_scroll_offset = 1;
        }
        
        int thumb_y = (int)((float)target->scroll_offset / max_scroll_offset * (visible_height - thumb_height));
        
        // 直接设置组件内部的滑块矩形
        component->thumb_rect.x = 0;
        component->thumb_rect.y = thumb_y;
        component->thumb_rect.w = component->thickness;
        component->thumb_rect.h = thumb_height;
    } else {
        // 水平滚动条
        // 计算内容宽度和可见宽度
        int content_width = 0;
        int visible_width = target->rect.w;
        
        // 计算所有子图层的总宽度
        for (int i = 0; i < target->child_count; i++) {
            Layer* child = target->children[i];
            if(child==NULL) continue;
            if (child->rect.x + child->rect.w > content_width) {
                content_width = child->rect.x + child->rect.w;
            }
        }
        
        // 如果内容宽度小于等于可见宽度，隐藏滚动条
        if (content_width <= visible_width) {
            scrollbar_component_set_visible(component, 0);
            return;
        } else {
            scrollbar_component_set_visible(component, 1);
        }
        
        // 设置滚动条位置和大小
        scrollbar_layer->rect.x = target->rect.x;
        scrollbar_layer->rect.y = target->rect.y + target->rect.h - component->thickness;
        scrollbar_layer->rect.w = target->rect.w;
        scrollbar_layer->rect.h = component->thickness;
        
        // 计算滑块宽度和位置
        float ratio = (float)visible_width / content_width;
        int thumb_width = (int)(visible_width * ratio);
        if (thumb_width < 20) thumb_width = 20;  // 最小宽度
        
        int max_scroll_offset = content_width - visible_width;
        // 防止除零错误
        if (max_scroll_offset <= 0) {
            max_scroll_offset = 1;
        }
        
        int thumb_x = (int)((float)target->scroll_offset_x / max_scroll_offset * (visible_width - thumb_width));
        
        // 直接设置组件内部的滑块矩形
        component->thumb_rect.x = thumb_x;
        component->thumb_rect.y = 0;
        component->thumb_rect.w = thumb_width;
        component->thumb_rect.h = component->thickness;
    }
}

// 处理鼠标事件
void scrollbar_component_handle_mouse_event(Layer* layer, MouseEvent* event) {
    ScrollbarComponent* component = (ScrollbarComponent*)layer->component;
    if (!component || !component->target_layer) {
        return;
    }
    
    Layer* target = component->target_layer;
    // 获取相对滑块位置
    Rect absolute_thumb_rect = component->thumb_rect;
    absolute_thumb_rect.x += layer->rect.x;
    absolute_thumb_rect.y += layer->rect.y;
    
    // 鼠标按下事件
    if (event->state == BUTTON_PRESSED && event->button == BUTTON_LEFT) {
        // 检查是否点击了滑块
        Point pt = {event->x, event->y};
        if (point_in_rect(pt, absolute_thumb_rect)) {
            component->is_dragging = 1;
            if (component->direction == SCROLLBAR_DIRECTION_VERTICAL) {
                component->drag_offset = event->y - absolute_thumb_rect.y;
            } else {
                component->drag_offset = event->x - absolute_thumb_rect.x;
            }
        }
    } else if (event->state == 0) {
        // 鼠标释放事件
        component->is_dragging = 0;
    }
    
    // 鼠标移动事件（无论是否按下）
    if (component->is_dragging) {
        if (component->direction == SCROLLBAR_DIRECTION_VERTICAL) {
            // 垂直滚动
            int new_y = event->y - component->drag_offset - layer->rect.y;
            int max_y = layer->rect.h - component->thumb_rect.h;
            
            if (new_y < 0) new_y = 0;
            if (new_y > max_y) new_y = max_y;
            
            component->thumb_rect.y = new_y;
            
            // 计算内容高度和可见高度
            int content_height;
            int visible_height = target->rect.h;
            
            // 优先使用 target->content_height（如果可用）
            if (target->content_height > 0) {
                content_height = target->content_height;
            } else {
                // 回退到计算所有子图层的总高度
                content_height = 0;
                for (int i = 0; i < target->child_count; i++) {
                    Layer* child = target->children[i];
                    if (child && child->rect.y + child->rect.h > content_height) {
                        content_height = child->rect.y + child->rect.h;
                    }
                }
            }
            
            int max_scroll_offset = content_height - visible_height;
            if (max_scroll_offset <= 0) max_scroll_offset = 0;
            float ratio = (float)new_y / max_y;
            target->scroll_offset = (int)(ratio * max_scroll_offset);
            
            // 确保滚动偏移在合理范围内
            if (target->scroll_offset < 0) target->scroll_offset = 0;
            if (target->scroll_offset > max_scroll_offset) target->scroll_offset = max_scroll_offset;
            
        } else {
            // 水平滚动
            int new_x = event->x - component->drag_offset - layer->rect.x;
            int max_x = layer->rect.w - component->thumb_rect.w;
            
            if (new_x < 0) new_x = 0;
            if (new_x > max_x) new_x = max_x;
            
            component->thumb_rect.x = new_x;
            
            // 计算内容宽度和可见宽度
            int content_width = 0;
            int visible_width = target->rect.w;
            
            for (int i = 0; i < target->child_count; i++) {
                Layer* child = target->children[i];
                if (child && child->rect.x + child->rect.w > content_width) {
                    content_width = child->rect.x + child->rect.w;
                }
            }
            
            int max_scroll_offset = content_width - visible_width;
            float ratio = (float)new_x / max_x;
            target->scroll_offset_x = (int)(ratio * max_scroll_offset);
        }
    }
}

// 渲染滚动条
void scrollbar_component_render(Layer* layer) {
    ScrollbarComponent* component = (ScrollbarComponent*)layer->component;
    if (!component || !component->visible) {
        return;
    }
    // 绘制轨道
    Rect track_rect = layer->rect;
    backend_render_fill_rect(&track_rect, component->track_color);
    
    // 绘制滑块（使用组件内部的矩形）
    Rect thumb_rect = component->thumb_rect;
    thumb_rect.x += layer->rect.x;
    thumb_rect.y += layer->rect.y;
    backend_render_fill_rect(&thumb_rect, component->thumb_color);
}


// 从JSON创建滚动条组件
ScrollbarComponent* scrollbar_component_create_from_json(Layer* layer,Layer* target_layer, cJSON* json_obj) {
    if (!layer || !json_obj) {
        return NULL;
    }

    printf("scrollbar_component_create_from_json: creating scrollbar component\n");

    // 获取滚动方向
    ScrollbarDirection direction = SCROLLBAR_DIRECTION_VERTICAL;
    cJSON* direction_json = cJSON_GetObjectItem(json_obj, "direction");
    if (direction_json && cJSON_IsString(direction_json)) {
        if (strcmp(direction_json->valuestring, "horizontal") == 0) {
            direction = SCROLLBAR_DIRECTION_HORIZONTAL;
        }
    }

    // 创建滚动条组件
    ScrollbarComponent* component = scrollbar_component_create(layer, target_layer, direction);
    if (!component) {
        return NULL;
    }

    // 设置厚度
    cJSON* thickness_json = cJSON_GetObjectItem(json_obj, "thickness");
    if (thickness_json && cJSON_IsNumber(thickness_json)) {
        scrollbar_component_set_thickness(component, thickness_json->valueint);
    }
    
    // 兼容旧的 scrollbarWidth 属性
    cJSON* scrollbar_width_json = cJSON_GetObjectItem(json_obj, "scrollbarWidth");
    if (scrollbar_width_json && cJSON_IsNumber(scrollbar_width_json)) {
        scrollbar_component_set_thickness(component, scrollbar_width_json->valueint);
    }

    // 设置轨道颜色
    cJSON* track_color_json = cJSON_GetObjectItem(json_obj, "trackColor");
    if (track_color_json && cJSON_IsString(track_color_json)) {
        Color track_color;
        parse_color(track_color_json->valuestring, &track_color);
        scrollbar_component_set_colors(component, track_color, component->thumb_color);
    }

    // 设置滑块颜色
    cJSON* thumb_color_json = cJSON_GetObjectItem(json_obj, "thumbColor");
    if (thumb_color_json && cJSON_IsString(thumb_color_json)) {
        Color thumb_color;
        parse_color(thumb_color_json->valuestring, &thumb_color);
        scrollbar_component_set_colors(component, component->track_color, thumb_color);
    }

    // 设置可见性
    cJSON* visible_json = cJSON_GetObjectItem(json_obj, "visible");
    if (visible_json && cJSON_IsBool(visible_json)) {
        scrollbar_component_set_visible(component, cJSON_IsTrue(visible_json) ? 1 : 0);
    }

    return component;
}

// 设置滚动条厚度
void scrollbar_component_set_thickness(ScrollbarComponent* component, int thickness) {
    if (!component) {
        return;
    }
    component->thickness = thickness;
    scrollbar_component_update_position(component);
}

// 设置滚动条颜色
void scrollbar_component_set_colors(ScrollbarComponent* component, Color track_color, Color thumb_color) {
    if (!component) {
        return;
    }
    component->track_color = track_color;
    component->thumb_color = thumb_color;
}

// 设置滚动条可见性
void scrollbar_component_set_visible(ScrollbarComponent* component, int visible) {
    if (!component) {
        return;
    }
    component->visible = visible;
}

// 销毁滚动条组件
void scrollbar_component_destroy(ScrollbarComponent* component) {
    if (!component) return;
    
    // 清理指针
    component->layer = NULL;
    component->target_layer = NULL;
    
    free(component);
}