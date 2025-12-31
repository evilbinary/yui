#include "../render.h"
#include "../backend.h"
#include <stdlib.h>
#include <string.h>
#include "checkbox_component.h"

// 创建复选框组件
CheckboxComponent* checkbox_component_create(Layer* layer, int default_checked) {
    if (!layer) {
        return NULL;
    }
    
    CheckboxComponent* component = (CheckboxComponent*)malloc(sizeof(CheckboxComponent));
    if (!component) {
        return NULL;
    }
    
    memset(component, 0, sizeof(CheckboxComponent));
    component->layer = layer;
    component->checked = default_checked;
    component->user_data = NULL;
    // 不再初始化component->label，因为我们将使用layer->label
    
    // 设置默认颜色到layer
    layer->bg_color = (Color){255, 255, 255, 255};         // 白色背景
    component->border_color = (Color){100, 149, 237, 255};     // 蓝色边框
    component->check_color = (Color){25, 25, 112, 255};        // 深蓝色勾选
    layer->color = (Color){0, 0, 0, 255};            // 黑色标签
        
    // 设置组件指针和自定义渲染函数
    layer->component = component;
    layer->render = checkbox_component_render;
    
    // 绑定事件处理函数
    layer->handle_mouse_event = checkbox_component_handle_mouse_event;
    
    // 设置组件为可聚焦（默认情况下未禁用时可聚焦）
    layer->focusable = !HAS_STATE(layer, LAYER_STATE_DISABLED);

    return component;
}

// 从JSON创建复选框组件
CheckboxComponent* checkbox_component_create_from_json(Layer* layer, cJSON* json_obj) {
    int default_checked = 0;
    if(cJSON_HasObjectItem(json_obj, "data")) {
        default_checked = cJSON_IsTrue(cJSON_GetObjectItem(json_obj, "data"));
    }
    cJSON* size = cJSON_GetObjectItem(json_obj, "size");
    if (size == NULL) {
      layer->rect.w = 20;
      layer->rect.h = layer->rect.w;
      layer->fixed_width = layer->rect.w;
      layer->fixed_height = layer->rect.h;
    }
    return checkbox_component_create(layer, default_checked);
}

// 销毁复选框组件
void checkbox_component_destroy(CheckboxComponent* component) {
    if (component) {
        // 不再需要释放component->label，因为我们使用的是layer->label
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
        // 使用layer->bg_color代替component->bg_color
        if (component->layer) {
            component->layer->bg_color = bg_color;
        }
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
    
    // 直接使用layer->label
    if (label && component->layer) {
        strncpy(component->layer->label, label, MAX_TEXT - 1);
        component->layer->label[MAX_TEXT - 1] = '\0'; // 确保字符串结束
    } else if (component->layer) {
        component->layer->label[0] = '\0'; // 清空标签
    }
    
    // 使用layer->color代替component->label_color
    if (component->layer) {
        component->layer->color = color;
    }
}

// 设置禁用状态
void checkbox_component_set_disabled(CheckboxComponent* component, int disabled) {
    if (!component || !component->layer) {
        return;
    }
    
    if (disabled) {
        SET_STATE(component->layer, LAYER_STATE_DISABLED);
        // 禁用时不能获得焦点
        component->layer->focusable = 0;
    } else {
        CLEAR_STATE(component->layer, LAYER_STATE_DISABLED);
        // 启用时恢复焦点能力
        component->layer->focusable = 1;
    }
}

// 检查是否被禁用
int checkbox_component_is_disabled(CheckboxComponent* component) {
    if (!component || !component->layer) {
        return 0;
    }
    
    return HAS_STATE(component->layer, LAYER_STATE_DISABLED);
}

// 处理鼠标事件
void checkbox_component_handle_mouse_event(Layer* layer, MouseEvent* event) {
    if (!layer || !event || !layer->component) {
        return;
    }
    
    // 如果组件被禁用，不响应点击事件
    if (HAS_STATE(layer, LAYER_STATE_DISABLED)) {
        return;
    }
    
    CheckboxComponent* component = (CheckboxComponent*)layer->component;
    
    // 检查鼠标是否在复选框范围内
    int is_inside = (event->x >= layer->rect.x && 
                     event->x < layer->rect.x + layer->rect.w &&
                     event->y >= layer->rect.y && 
                     event->y < layer->rect.y + layer->rect.h);
    
    // 处理鼠标点击事件
    if (event->button == BUTTON_LEFT && event->state == BUTTON_PRESSED && is_inside) {
        // 切换选中状态
        component->checked = !component->checked;

        // 标记图层状态变化，确保重绘，但保留禁用状态
        if (component->checked) {
            layer->state |= LAYER_STATE_ACTIVE; // 只设置激活位，保留其他位
        } else {
            layer->state &= ~LAYER_STATE_ACTIVE; // 只清除激活位，保留其他位
        }
        
        // 如果有点击事件回调，调用它
        if (layer->event && layer->event->click) {
            layer->event->click(layer);
        }
    }
}

// 渲染复选框
void checkbox_component_render(Layer* layer) {
    if (!layer || !layer->component) {
        return;
    }
    
    CheckboxComponent* component = (CheckboxComponent*)layer->component;
    
    // 检查是否被禁用
    int is_disabled = HAS_STATE(layer, LAYER_STATE_DISABLED);
    
    // 绘制复选框背景，使用layer->bg_color
    // 禁用时降低透明度
    Color bg_color = layer->bg_color;
    if (is_disabled) {
        bg_color.a = bg_color.a * 0.6;
    }
    
    backend_render_fill_rect_color(
        &layer->rect,
        bg_color.r,
        bg_color.g,
        bg_color.b,
        bg_color.a
    );
    
    // 绘制复选框边框
    Color border_color = component->border_color;
    // 选中状态下使用不同的边框颜色
    if (component->checked) {
        border_color = component->check_color;
    }
    // 禁用时降低颜色饱和度
    if (is_disabled) {
        // 计算灰度值
        int gray = (border_color.r * 30 + border_color.g * 59 + border_color.b * 11) / 100;
        border_color.r = gray;
        border_color.g = gray;
        border_color.b = gray;
        border_color.a = border_color.a * 0.6;
    }
    
    backend_render_rect_color(
        &layer->rect,
        border_color.r,
        border_color.g,
        border_color.b,
        border_color.a
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
        
        Color check_color = component->check_color;
        // 禁用时降低颜色饱和度和透明度
        if (is_disabled) {
            int gray = (check_color.r * 30 + check_color.g * 59 + check_color.b * 11) / 100;
            check_color.r = gray;
            check_color.g = gray;
            check_color.b = gray;
            check_color.a = check_color.a * 0.6;
        }
        
        // 绘制勾选标记（两条线）
        backend_render_line(x1, y1, x2, y2, check_color);
        backend_render_line(x2, y2, x3, y3, check_color);
    }
    
    // 绘制标签文本，使用layer->label和layer->color
    if (layer->label[0] != '\0' && layer->font->default_font) {
        // 计算标签位置（在复选框右侧，垂直居中）
        int label_x = layer->rect.x + layer->rect.w + 5;  // 5像素间距
        
        Color text_color = layer->color;
        // 禁用时降低颜色饱和度和透明度
        if (is_disabled) {
            int gray = (text_color.r * 30 + text_color.g * 59 + text_color.b * 11) / 100;
            text_color.r = gray;
            text_color.g = gray;
            text_color.b = gray;
            text_color.a = text_color.a * 0.6;
        }
        
        // 使用backend_render_texture渲染文本到纹理
        Texture* text_texture = backend_render_texture(layer->font->default_font, layer->label, text_color);
        if (text_texture) {
            // 获取文本纹理的尺寸
            int text_width, text_height;
            backend_query_texture(text_texture, NULL, NULL, &text_width, &text_height);
            
            // 计算垂直居中的Y坐标
            int label_y = layer->rect.y + (layer->rect.h - text_height/scale) / 2;
            
            // 创建目标矩形
            Rect dst_rect = {
                .x = label_x,
                .y = label_y,
                .w = text_width/ scale,
                .h = text_height/ scale
            };
            
            // 渲染文本纹理
            backend_render_text_copy(text_texture, NULL, &dst_rect);
            
            // 释放纹理资源
            backend_render_text_destroy(text_texture);
        }
    }
}