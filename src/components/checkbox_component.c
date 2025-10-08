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
    
    // 设置默认颜色
    component->bg_color = (Color){255, 255, 255, 255};         // 白色背景
    component->border_color = (Color){100, 149, 237, 255};     // 蓝色边框
    component->check_color = (Color){25, 25, 112, 255};        // 深蓝色勾选
    
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
        free(component);
    }
}

// 设置复选框选中状态
void checkbox_component_set_checked(CheckboxComponent* component, int checked) {
    if (component) {
        component->checked = checked ? 1 : 0;
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

// 处理鼠标事件
void checkbox_component_handle_mouse_event(Layer* layer, MouseEvent* event) {
    if (!layer || !event || !layer->component) {
        return;
    }
    
    CheckboxComponent* component = (CheckboxComponent*)layer->component;
    
    // 处理鼠标点击事件
    if (event->button == SDL_BUTTON_LEFT && event->state == SDL_PRESSED) {
        // 切换选中状态
        component->checked = !component->checked;
        
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
    backend_render_rect_color(
        &layer->rect,
        component->border_color.r,
        component->border_color.g,
        component->border_color.b,
        component->border_color.a
    );
    
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
}