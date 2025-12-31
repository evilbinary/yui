#include "button_component.h"
#include "../render.h"
#include "../backend.h"
#include <stdlib.h>
#include <string.h>

// 创建按钮组件
ButtonComponent* button_component_create(Layer* layer) {
    if (!layer) {
        return NULL;
    }
    
    ButtonComponent* component = (ButtonComponent*)malloc(sizeof(ButtonComponent));
    if (!component) {
        return NULL;
    }
    
    memset(component, 0, sizeof(ButtonComponent));
    component->layer = layer;
    component->user_data = NULL;
    
    // 设置默认颜色
    component->colors[LAYER_STATE_NORMAL] = (Color){100, 149, 237, 255};    // 蓝色
    component->colors[LAYER_STATE_HOVER] = (Color){135, 206, 250, 255};     // 亮蓝色
    component->colors[LAYER_STATE_PRESSED] = (Color){70, 130, 180, 255};    // 深蓝色
    component->colors[LAYER_STATE_DISABLED] = (Color){200, 200, 200, 150};  // 灰色半透明
    // 设置组件指针和自定义渲染函数
    layer->component = component;
    layer->render = button_component_render;
    
    // 绑定事件处理函数
    layer->handle_mouse_event = button_component_handle_mouse_event;
    
    // 绑定键盘事件处理函数
    layer->handle_key_event = button_component_handle_key_event;
    
    // 设置组件为可聚焦
    layer->focusable = 1;
    
    return component;
}

// 销毁按钮组件
void button_component_destroy(ButtonComponent* component) {
    if (component) {
        free(component);
    }
}

// 设置按钮文本
void button_component_set_text(ButtonComponent* component, const char* text) {
    if (!component || !text) {
        return;
    }
    
    strncpy(component->layer->text, text, MAX_TEXT - 1);
    component->layer->text[MAX_TEXT - 1] = '\0';
}

// 设置按钮颜色
void button_component_set_color(ButtonComponent* component, LayerState state, Color color) {
    if (!component || state < 0 || state >= 4) {
        return;
    }
    
    component->colors[state] = color;
}

// 设置用户数据
void button_component_set_user_data(ButtonComponent* component, void* data) {
    if (!component) {
        return;
    }
    
    component->user_data = data;
}

// 处理键盘事件
void button_component_handle_key_event(Layer* layer, KeyEvent* event) {
    ButtonComponent* component = (ButtonComponent*)layer->component;
    if (!component || HAS_STATE(layer, LAYER_STATE_DISABLED)) {
        return;
    }
    
    // 处理回车键或空格键按下事件
    if (event->type == KEY_EVENT_DOWN && (event->data.key.key_code == 13 || event->data.key.key_code == 32)) {
        // 模拟按钮点击，设置Layer的PRESSED状态
        SET_STATE(layer, LAYER_STATE_PRESSED);
        // 触发点击事件（如果存在）
        if (layer->event && layer->event->click) {
            layer->event->click(layer);
        }
    }
    
    // 处理按键释放事件，恢复按钮状态
    if (event->type == KEY_EVENT_UP && (event->data.key.key_code == 13 || event->data.key.key_code == 32)) {
        // 同时设置Layer的状态，如果有焦点则保持焦点状态，否则设置为NORMAL
        if (HAS_STATE(layer, LAYER_STATE_PRESSED)) {
            CLEAR_STATE(layer, LAYER_STATE_PRESSED);
            // 保留焦点状态
        }
    }
}

// 处理鼠标事件
void button_component_handle_mouse_event(Layer* layer, MouseEvent* event) {
    ButtonComponent* component = (ButtonComponent*)layer->component;
    int is_click = (event->state == 1);
    if (!component || HAS_STATE(layer, LAYER_STATE_DISABLED)) {
        return;
    }

    // printf("button_component_handle_mouse_event: %d, %d, %d, %d state:%d\n", event->x, event->y, is_click , event->button,event->state);
    
    // 检查鼠标是否在按钮范围内
    int is_inside = (event->x >= layer->rect.x && 
                     event->x < layer->rect.x + layer->rect.w &&
                     event->y >= layer->rect.y && 
                     event->y < layer->rect.y + layer->rect.h);
    
    // 更新悬停状态
    if (is_inside) {
        SET_STATE(layer, LAYER_STATE_HOVER);
    } else {
        CLEAR_STATE(layer, LAYER_STATE_HOVER);
    }
    
    if (is_click) {
        // printf("button_component_handle_mouse_event click: %d, %d, %d, %d state:%d\n", event->x, event->y, is_click , event->button,event->state);
        if (is_inside) {
            // 鼠标点击在按钮上，设置Layer的PRESSED状态
            SET_STATE(layer, LAYER_STATE_PRESSED);
            // 触发点击事件（如果存在）
            if (layer->event && layer->event->click) {
                layer->event->click(layer);
            }
        } else {
            // 鼠标点击在按钮外，清除按下状态
            CLEAR_STATE(layer, LAYER_STATE_PRESSED);
        }
    } else {
        // 非点击事件，清除按下状态
        CLEAR_STATE(layer, LAYER_STATE_PRESSED);
    }
}

// 渲染按钮组件
void button_component_render(Layer* layer) {
    if (!layer || !layer->component) {
        return;
    }
    
    ButtonComponent* component = (ButtonComponent*)layer->component;
    
    // 根据按钮状态选择颜色，如果layer有背景色则优先使用layer的背景色
    Color bg_color;
    if (layer->bg_color.a > 0) {
        bg_color = layer->bg_color;
    } else {
        // 根据当前状态组合选择合适的颜色
        if (HAS_STATE(layer, LAYER_STATE_PRESSED)) {
            bg_color = component->colors[LAYER_STATE_PRESSED];
        } else if (HAS_STATE(layer, LAYER_STATE_HOVER)) {
            bg_color = component->colors[LAYER_STATE_HOVER];
        } else if (HAS_STATE(layer, LAYER_STATE_FOCUSED)) {
            bg_color = component->colors[LAYER_STATE_FOCUSED];
        } else if (HAS_STATE(layer, LAYER_STATE_DISABLED)) {
            bg_color = component->colors[LAYER_STATE_DISABLED];
        } else {
            bg_color = component->colors[LAYER_STATE_NORMAL];
        }
    }
    
    // 绘制背景
    if (bg_color.a > 0) {
        // 如果启用了毛玻璃效果，先渲染毛玻璃效果
        if (layer->backdrop_filter) {
            backend_render_backdrop_filter(&layer->rect, layer->blur_radius, layer->saturation, layer->brightness);
        }
        
        if (layer->radius > 0) {
            backend_render_rounded_rect(&layer->rect, bg_color, layer->radius);
        } else {
            backend_render_fill_rect(&layer->rect, bg_color);
        }
    }
    
    // 绘制边框
    Color border_color = (Color){200, 200, 200, 255};
    if (HAS_STATE(layer, LAYER_STATE_PRESSED)) {
        border_color = (Color){100, 100, 100, 255};
    } else if (HAS_STATE(layer, LAYER_STATE_HOVER)) {
        border_color = (Color){150, 150, 150, 255};
    } else if (HAS_STATE(layer, LAYER_STATE_FOCUSED)) {
        border_color = (Color){50, 150, 255, 255}; // 聚焦状态使用高亮边框
    }
    
    if (layer->radius > 0) {
        backend_render_rounded_rect_with_border(&layer->rect, bg_color, layer->radius, 2, border_color);
    } else {
        backend_render_rect_color(&layer->rect, border_color.r, border_color.g, border_color.b, border_color.a);
    }
    
    // 渲染按钮文本
    if (strlen(layer->text) > 0) {
        Color text_color = layer->color;
        if (HAS_STATE(layer, LAYER_STATE_DISABLED)) {
            text_color = (Color){255, 255, 255, 150};
        }
        
        Texture* text_texture = render_text(layer, layer->text, text_color);
        
        if (text_texture) {
            int text_width, text_height;
            backend_query_texture(text_texture, NULL, NULL, &text_width, &text_height);
            
            Rect text_rect = {
                layer->rect.x + (layer->rect.w - text_width / scale) / 2,  // 居中
                layer->rect.y + (layer->rect.h - text_height / scale) / 2,
                text_width / scale,
                text_height / scale
            };
            
            // 确保文本不会超出按钮边界
            if (text_rect.w > layer->rect.w - 20) {
                text_rect.w = layer->rect.w - 20;
                text_rect.x = layer->rect.x + 10;
            }
            
            if (text_rect.h > layer->rect.h) {
                text_rect.h = layer->rect.h;
                text_rect.y = layer->rect.y;
            }
            
            backend_render_text_copy(text_texture, NULL, &text_rect);
            backend_render_text_destroy(text_texture);
        }
    }
}