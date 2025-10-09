#include "../render.h"
#include "../backend.h"
#include <stdlib.h>
#include <string.h>
#include "checkbox_component.h"

// 创建复选框组件
CheckboxComponent* checkbox_component_create(Layer* layer) {
    if (!layer) {
        return NULL;
    }
    
    CheckboxComponent* component = (CheckboxComponent*)malloc(sizeof(CheckboxComponent));
    if (!component) {
        return NULL;
    }
    
    memset(component, 0, sizeof(CheckboxComponent));
    component->layer = layer;
    component->checked = 0;
    component->user_data = NULL;
    component->label = NULL;
    
    // 设置默认颜色
    component->bg_color = (Color){255, 255, 255, 255};         // 白色背景
    component->border_color = (Color){100, 149, 237, 255};     // 蓝色边框
    component->check_color = (Color){25, 25, 112, 255};        // 深蓝色勾选
    component->label_color = (Color){0, 0, 0, 255};            // 黑色标签
    
    // 设置组件指针和自定义渲染函数
    layer->component = component;
    layer->render = checkbox_component_render;
    
    // 绑定事件处理函数
    layer->handle_mouse_event = checkbox_component_handle_mouse_event;
    
    // 设置组件为可聚焦
    layer->focusable = 1;
    
    return component;
}

// 销毁复选框组件
void checkbox_component_destroy(CheckboxComponent* component) {
    if (component) {
        if (component->label) {
            free(component->label);
        }
        free(component);
    }
}

// 设置复选框选中状态
void checkbox_component_set_checked(CheckboxComponent* component, int checked) {
    if (component) {
        component->checked = checked ? 1 : 0;
        // 同步状态到图层
        if (component->checked) {
            SET_STATE(component->layer, LAYER_STATE_ACTIVE);
        } else {
            CLEAR_STATE(component->layer, LAYER_STATE_ACTIVE);
        }
    }
}

// 获取复选框选中状态
int checkbox_component_is_checked(CheckboxComponent* component) {
    if (component) {
        return component->checked;
    }
    return 0;
}

// 设置复选框颜色
void checkbox_component_set_colors(CheckboxComponent* component, Color bg_color, Color border_color, Color check_color) {
    if (component) {
        component->bg_color = bg_color;
        component->border_color = border_color;
        component->check_color = check_color;
    }
}

// 设置用户数据
void checkbox_component_set_user_data(CheckboxComponent* component, void* data) {
    if (component) {
        component->user_data = data;
    }
}

// 设置标签文本和颜色
void checkbox_component_set_label(CheckboxComponent* component, const char* label, Color color) {
    if (!component) {
        return;
    }
    
    // 释放之前的标签文本
    if (component->label) {
        free(component->label);
        component->label = NULL;
    }
    
    // 设置新的标签文本
    if (label) {
        component->label = strdup(label);
    }
    
    // 设置标签颜色
    component->label_color = color;
}

// 处理鼠标事件
// 处理鼠标点击事件
void checkbox_component_handle_mouse_event(Layer* layer, MouseEvent* event) {
    if (!layer || !event || !layer->component) {
        return;
    }
    
    CheckboxComponent* component = (CheckboxComponent*)layer->component;
    
    // 检查鼠标是否在复选框范围内
    int is_inside = (event->x >= layer->rect.x && 
                     event->x < layer->rect.x + layer->rect.w &&
                     event->y >= layer->rect.y && 
                     event->y < layer->rect.y + layer->rect.h);
    
    // 处理鼠标点击事件
    if (event->button == SDL_BUTTON_LEFT && event->state == SDL_PRESSED && is_inside) {
        // 切换选中状态
        component->checked = !component->checked;

        // 标记图层状态变化，确保重绘
        if (component->checked) {
            SET_STATE(layer, LAYER_STATE_ACTIVE);
        } else {
            CLEAR_STATE(layer, LAYER_STATE_ACTIVE);
        }
        
        // 如果有点击事件回调，调用它
        if (layer->event && layer->event->click) {
            layer->event->click();
        }
    }
}

// 渲染复选框
void checkbox_component_render(Layer* layer) {
    if (!layer || !layer->component) {
        return;
    }
    
    CheckboxComponent* component = (CheckboxComponent*)layer->component;
    
    // 绘制复选框背景
    backend_render_fill_rect_color(
        &layer->rect,
        component->bg_color.r,
        component->bg_color.g,
        component->bg_color.b,
        component->bg_color.a
    );
    
    // 绘制复选框边框
    Color border_color = component->border_color;
    // 选中状态下使用不同的边框颜色
    if (component->checked) {
        border_color = component->check_color;
    }
    
    backend_render_rect_color(
        &layer->rect,
        border_color.r,
        border_color.g,
        border_color.b,
        border_color.a
    );
    
    // printf("checkbox checked: %d\n", component->checked);
    // 如果选中，绘制勾选标记
    if (component->checked) {
        // 计算勾选标记的位置和大小
        int padding = layer->rect.w / 4;
        int x1 = layer->rect.x + padding;
        int y1 = layer->rect.y + layer->rect.h / 2;
        int x2 = layer->rect.x + layer->rect.w / 2;
        int y2 = layer->rect.y + layer->rect.h - padding;
        int x3 = layer->rect.x + layer->rect.w - padding;
        int y3 = layer->rect.y + padding;
        
        // 绘制勾选标记（两条线）
        backend_render_line(x1, y1, x2, y2, component->check_color);
        backend_render_line(x2, y2, x3, y3, component->check_color);
    }
    
    // 绘制标签文本
    if (component->label && layer->font->default_font) {
        // 计算标签位置（在复选框右侧，垂直居中）
        int label_x = layer->rect.x + layer->rect.w + 5;  // 5像素间距
        
        // 使用backend_render_texture渲染文本到纹理
        Texture* text_texture = backend_render_texture(layer->font->default_font, component->label, component->label_color);
        if (text_texture) {
            // 获取文本纹理的尺寸
            int text_width, text_height;
            backend_query_texture(text_texture, NULL, NULL, &text_width, &text_height);
            
            // 计算垂直居中的Y坐标
            int label_y = layer->rect.y + (layer->rect.h - text_height) / 2;
            
            // 创建目标矩形
            Rect dst_rect = {
                .x = label_x,
                .y = label_y,
                .w = text_width,
                .h = text_height
            };
            
            // 渲染文本纹理
            backend_render_text_copy(text_texture, NULL, &dst_rect);
            
            // 释放纹理资源
            backend_render_text_destroy(text_texture);
        }
    }
}