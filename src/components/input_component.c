#include "input_component.h"
#include "../render.h"
#include "../backend.h"
#include <stdlib.h>
#include <string.h>

// 创建输入组件
InputComponent* input_component_create(Layer* layer) {
    if (!layer) {
        return NULL;
    }
    
    InputComponent* component = (InputComponent*)malloc(sizeof(InputComponent));
    if (!component) {
        return NULL;
    }
    
    memset(component, 0, sizeof(InputComponent));
    component->layer = layer;
    component->state = INPUT_STATE_NORMAL;
    component->max_length = MAX_TEXT - 1;
    component->cursor_pos = 0;
    component->selection_start = 0;
    component->selection_end = 0;
    
    // 设置组件指针和自定义渲染函数
    layer->component = component;
    layer->render = input_component_render;
    layer->handle_mouse_event = input_component_handle_mouse_event;
    layer->handle_key_event = input_component_handle_key_event;
    
    return component;
}

// 销毁输入组件
void input_component_destroy(InputComponent* component) {
    if (component) {
        free(component);
    }
}

// 设置输入文本
void input_component_set_text(InputComponent* component, const char* text) {
    if (!component || !text) {
        return;
    }
    
    strncpy(component->text, text, component->max_length);
    component->text[component->max_length] = '\0';
    component->cursor_pos = strlen(component->text);
    component->selection_start = component->cursor_pos;
    component->selection_end = component->cursor_pos;
}

// 设置占位文本
void input_component_set_placeholder(InputComponent* component, const char* placeholder) {
    if (!component || !placeholder) {
        return;
    }
    
    strncpy(component->placeholder, placeholder, MAX_TEXT - 1);
    component->placeholder[MAX_TEXT - 1] = '\0';
}

// 设置最大输入长度
void input_component_set_max_length(InputComponent* component, int max_length) {
    if (!component) {
        return;
    }
    
    if (max_length > 0 && max_length < MAX_TEXT) {
        component->max_length = max_length;
        
        // 如果当前文本超过新的最大长度，截断它
        if (strlen(component->text) > max_length) {
            component->text[max_length] = '\0';
            component->cursor_pos = max_length;
            component->selection_start = max_length;
            component->selection_end = max_length;
        }
    }
}

// 设置输入状态
void input_component_set_state(InputComponent* component, InputState state) {
    if (!component) {
        return;
    }
    
    component->state = state;
}

// 处理键盘事件
void input_component_handle_key_event(Layer* layer,  KeyEvent* event) {
    InputComponent* component = (InputComponent*)layer->component;
    if (!component || !event || component->state == INPUT_STATE_DISABLED) {
        return;
    }
    printf("input_component_handle_key_event: %d, %s\n", event->type, event->data.text.text);

    int current_length = strlen(component->text);
    
    switch (event->type) {
        case KEY_EVENT_TEXT_INPUT:
            // 处理文本输入
            if (current_length < component->max_length) {
                // 如果有选中文本，先删除选中的文本
                if (component->selection_start != component->selection_end) {
                    int start = component->selection_start < component->selection_end ? 
                                component->selection_start : component->selection_end;
                    int end = component->selection_start > component->selection_end ? 
                              component->selection_start : component->selection_end;
                    
                    // 删除选中的文本
                    memmove(component->text + start, component->text + end, 
                           current_length - end + 1);
                    component->cursor_pos = start;
                    current_length -= (end - start);
                }
                
                // 插入新文本
                if (strlen(event->data.text.text) > 0) {
                    int text_len = strlen(event->data.text.text);
                    int available_space = component->max_length - current_length;
                    int copy_len = text_len < available_space ? text_len : available_space;
                    
                    // 移动现有文本，为新文本腾出空间
                    memmove(component->text + component->cursor_pos + copy_len, 
                           component->text + component->cursor_pos, 
                           current_length - component->cursor_pos + 1);
                    
                    // 复制新文本
                    memcpy(component->text + component->cursor_pos, event->data.text.text, copy_len);
                    
                    // 更新光标位置
                    component->cursor_pos += copy_len;
                }
            }
            break;
            
        case KEY_EVENT_DOWN:
            // 处理特殊键
            switch (event->data.key.key_code) {
                case 8:  // Backspace
                    if (current_length > 0 && component->cursor_pos > 0) {
                        if (component->selection_start != component->selection_end) {
                            // 有选中文本，删除选中的文本
                            int start = component->selection_start < component->selection_end ? 
                                        component->selection_start : component->selection_end;
                            int end = component->selection_start > component->selection_end ? 
                                      component->selection_start : component->selection_end;
                            
                            memmove(component->text + start, component->text + end, 
                                   current_length - end + 1);
                            component->cursor_pos = start;
                        } else {
                            // 没有选中文本，删除光标前的字符
                            memmove(component->text + component->cursor_pos - 1, 
                                   component->text + component->cursor_pos, 
                                   current_length - component->cursor_pos + 1);
                            component->cursor_pos--;
                        }
                    }
                    break;
                    
                case 127:  // Delete
                    if (current_length > 0 && component->cursor_pos < current_length) {
                        if (component->selection_start != component->selection_end) {
                            // 有选中文本，删除选中的文本
                            int start = component->selection_start < component->selection_end ? 
                                        component->selection_start : component->selection_end;
                            int end = component->selection_start > component->selection_end ? 
                                      component->selection_start : component->selection_end;
                            
                            memmove(component->text + start, component->text + end, 
                                   current_length - end + 1);
                            component->cursor_pos = start;
                        } else {
                            // 没有选中文本，删除光标后的字符
                            memmove(component->text + component->cursor_pos, 
                                   component->text + component->cursor_pos + 1, 
                                   current_length - component->cursor_pos);
                        }
                    }
                    break;
                    
                case 37:  // Left arrow
                    if (component->cursor_pos > 0) {
                        component->cursor_pos--;
                    }
                    break;
                    
                case 39:  // Right arrow
                    if (component->cursor_pos < current_length) {
                        component->cursor_pos++;
                    }
                    break;
                    
                case 36:  // Home
                    component->cursor_pos = 0;
                    break;
                    
                case 35:  // End
                    component->cursor_pos = current_length;
                    break;
            }
            
            // 重置选择范围
            component->selection_start = component->cursor_pos;
            component->selection_end = component->cursor_pos;
            break;
    }
}

// 处理鼠标事件
void input_component_handle_mouse_event(Layer* layer, MouseEvent* event) {
    if (!layer || layer->component) {
        return;
    }
    InputComponent* component = (InputComponent*)layer->component;

    printf("input_component_handle_mouse_event: %d, %d, %d\n", event->x, event->y, event->state);
    int is_click = (event->state == 1);

   int is_inside = (event->x >= component->layer->rect.x && 
                     event->x < component->layer->rect.x + component->layer->rect.w &&
                     event->y >= component->layer->rect.y && 
                     event->y < component->layer->rect.y + component->layer->rect.h);

    // 检查是否点击在输入框内
    if (is_inside) { 
        if (is_click) {
            // 点击时，设置为聚焦状态
            component->state = INPUT_STATE_FOCUSED;
            
            // TODO: 根据点击位置计算光标位置
            // 这里简化处理，暂时设置为文本末尾
            component->cursor_pos = strlen(component->text);
            component->selection_start = component->cursor_pos;
            component->selection_end = component->cursor_pos;
        }
    } else if (is_click) {
        // 点击输入框外，取消聚焦
        component->state = INPUT_STATE_NORMAL;
    }
}

// 渲染输入组件
void input_component_render(Layer* layer) {
    if (!layer || !layer->component) {
        return;
    }
    
    InputComponent* component = (InputComponent*)layer->component;
    

    // 绘制背景
    if (layer->bgColor.a > 0) {
        if (layer->radius > 0) {
            backend_render_rounded_rect(&layer->rect, layer->bgColor, layer->radius);
        } else {
            backend_render_fill_rect(&layer->rect, layer->bgColor);
        }
    }

    // 绘制输入框边框，考虑圆角
    if (layer->radius > 0) {
        backend_render_rounded_rect_with_border(&layer->rect, layer->bgColor, layer->radius, 2, (Color){150, 150, 150, 255});
    } else {
        backend_render_rect_color(&layer->rect,150, 150, 150, 255);
    }
    
    // 渲染输入框标签
    if (strlen(layer->label) > 0) {
        // 使用ttf渲染文本
        
        Color text_color = layer->color;
        Texture* text_texture = render_text(layer,layer->label, text_color);
        
        if (text_texture) {
            int text_width, text_height;
            backend_query_texture(text_texture, NULL, NULL, &text_width, &text_height);
            
            Rect text_rect = {
                layer->rect.x + 5,  // 左侧留5像素边距
                layer->rect.y + (layer->rect.h - text_height/ scale) / 2,
                text_width / scale,
                text_height / scale
            };
            
            // 确保文本不会超出输入框边界
            if (text_rect.x + text_rect.w > layer->rect.x + layer->rect.w - 5) {
                text_rect.w = layer->rect.x + layer->rect.w - 5 - text_rect.x;
            }
            
            if (text_rect.y + text_rect.h > layer->rect.y + layer->rect.h) {
                text_rect.h = layer->rect.y + layer->rect.h - text_rect.y;
            }
            
            backend_render_text_copy(text_texture,NULL,&text_rect);
            backend_render_text_destroy(text_texture);

        }
    }
    
    // 绘制边框
    Color border_color = {150, 150, 150, 255};
    if (component->state == INPUT_STATE_FOCUSED) {
        // 聚焦状态下使用高亮边框
        border_color = (Color){50, 150, 255, 255};
    }
    
    if (layer->radius > 0) {
        backend_render_rounded_rect_with_border(&layer->rect, layer->bgColor, layer->radius, 2, border_color);
    } else {
        backend_render_rect_color(&layer->rect, border_color.r, border_color.g, border_color.b, border_color.a);
    }
    
    // 渲染文本
    Color text_color = layer->color;
    const char* display_text = component->text;
    
    // 如果没有输入文本且不是聚焦状态，显示占位文本
    if (strlen(component->text) == 0 && component->state != INPUT_STATE_FOCUSED && 
        strlen(component->placeholder) > 0) {
        display_text = component->placeholder;
        text_color = (Color){150, 150, 150, 150};  // 占位文本使用灰色
    }
    
    Texture* text_texture = render_text(layer, display_text, text_color);
    
    if (text_texture) {
        int text_width, text_height;
        backend_query_texture(text_texture, NULL, NULL, &text_width, &text_height);
        
        Rect text_rect = {
            layer->rect.x + 5,  // 左侧留5像素边距
            layer->rect.y + (layer->rect.h - text_height / scale) / 2,
            text_width / scale,
            text_height / scale
        };
        
        // 确保文本不会超出输入框边界
        if (text_rect.x + text_rect.w > layer->rect.x + layer->rect.w - 5) {
            text_rect.w = layer->rect.x + layer->rect.w - 5 - text_rect.x;
        }
        
        if (text_rect.y + text_rect.h > layer->rect.y + layer->rect.h) {
            text_rect.h = layer->rect.y + layer->rect.h - text_rect.y;
        }
        
        backend_render_text_copy(text_texture, NULL, &text_rect);
        backend_render_text_destroy(text_texture);
    }
    
    // 聚焦状态下绘制光标
    if (component->state == INPUT_STATE_FOCUSED) {
        // 计算光标位置
        // TODO: 更精确地计算光标位置
        int cursor_x = layer->rect.x + 5;
        int cursor_y = layer->rect.y + 5;
        int cursor_height = layer->rect.h - 10;
        
        // 简单处理：绘制垂直线作为光标
        backend_render_line(cursor_x, cursor_y, cursor_x, cursor_y + cursor_height, (Color){0, 0, 0, 255});
    }
}