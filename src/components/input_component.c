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
    component->max_length = MAX_TEXT - 1;
    component->cursor_pos = 0;
    component->selection_start = 0;
    component->selection_end = 0;
    
    // 设置组件指针和自定义渲染函数
    layer->component = component;
    layer->render = input_component_render;
    layer->handle_mouse_event = input_component_handle_mouse_event;
    layer->handle_key_event = input_component_handle_key_event;
    
    // 设置组件为可聚焦
    layer->focusable = 1;
    
    component->cursor_color = (Color){255, 0, 0, 255};
    return component;
}

// 从JSON创建复选框组件
InputComponent* input_component_create_from_json(Layer* layer, cJSON* json_obj) {
    InputComponent* component = input_component_create(layer);
    
    // 解析placeholder属性
    if (cJSON_HasObjectItem(json_obj, "placeholder")) {
        input_component_set_placeholder(layer->component, cJSON_GetObjectItem(json_obj, "placeholder")->valuestring);
    }
    
    // 解析maxLength属性
    if (cJSON_HasObjectItem(json_obj, "maxLength")) {
        input_component_set_max_length(layer->component, cJSON_GetObjectItem(json_obj, "maxLength")->valueint);
    }

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



// 设置光标颜色
void input_component_set_cursor_color(InputComponent* component, Color cursor_color) {
    if (!component) {
        return;
    }
    
    component->cursor_color = cursor_color;
}

// 处理键盘事件
void input_component_handle_key_event(Layer* layer,  KeyEvent* event) {
    InputComponent* component = (InputComponent*)layer->component;
    if (!component || !event || HAS_STATE(layer, LAYER_STATE_DISABLED)) {
        return;
    }
    if(!HAS_STATE(layer, LAYER_STATE_FOCUSED)){
        return;
    }
    // printf("input_component_handle_key_event: %d, %s\n", event->type, event->data.text.text);

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

// 定义一个静态变量来跟踪上一次点击的组件
static InputComponent* last_clicked_component = NULL;
// 添加一个标记来跟踪是否应该强制保持焦点
static bool force_focus_debug = false;

// 添加焦点状态检查函数用于调试
void input_component_debug_focus_state() {
    printf("\n--- FOCUS STATE DEBUG INFO ---");
    printf("\nLast clicked component address: %p", last_clicked_component);
    printf("\n--- END FOCUS DEBUG INFO --\n");
}

// 处理鼠标事件
void input_component_handle_mouse_event(Layer* layer, MouseEvent* event) {
    if (!layer || !layer->component) {
        printf("Mouse event skipped: invalid layer or component\n");
        return;
    }
    InputComponent* component = (InputComponent*)layer->component;

    printf("input_component_handle_mouse_event: layer address=%p, component address=%p\n", layer, component);
    printf("event x=%d, y=%d, state=%d\n", event->x, event->y, event->state);
    printf("Component current state before processing: %d\n", layer->state);
    int is_click = (event->state == 1);

    // Calculate component boundaries for debugging
    printf("Component boundaries: x=%d, y=%d, w=%d, h=%d\n", 
           component->layer->rect.x, component->layer->rect.y, 
           component->layer->rect.w, component->layer->rect.h);
    
   int is_inside = (event->x >= component->layer->rect.x && 
                     event->x < component->layer->rect.x + component->layer->rect.w &&
                     event->y >= component->layer->rect.y && 
                     event->y < component->layer->rect.y + component->layer->rect.h);
    
    // 更新悬停状态
    if (is_inside) {
        SET_STATE(layer, LAYER_STATE_HOVER);
    } else {
        CLEAR_STATE(layer, LAYER_STATE_HOVER);
    }
    
    printf("Mouse position is inside component: %s\n", is_inside ? "YES" : "NO");
    printf("Is click event: %s\n", is_click ? "YES" : "NO");

    // 检查是否点击在输入框内
    if (is_inside) { 
        if (is_click) {
            // 点击时，设置为聚焦状态
            SET_STATE(layer, LAYER_STATE_FOCUSED);
            last_clicked_component = component;  // 记录最后点击的组件
            // 设置强制焦点标记用于调试
            force_focus_debug = true;
            printf("State set to FOCUSED (value: %d) on click inside\n", layer->state);
            printf("Last clicked component updated to: %p\n", last_clicked_component);
            printf("DEBUG: force_focus_debug flag set to TRUE\n");
            
            // TODO: 根据点击位置计算光标位置
            // 这里简化处理，暂时设置为文本末尾
            component->cursor_pos = strlen(component->text);
            component->selection_start = component->cursor_pos;
            component->selection_end = component->cursor_pos;
            
            // 调用调试函数检查焦点状态
            input_component_debug_focus_state();
        }
    } else if (is_click) {
        printf("Click outside component: current component=%p, last_clicked_component=%p\n", component, last_clicked_component);
        // 点击输入框外，只有当这个组件是最后点击的组件时才取消聚焦
        if (component == last_clicked_component) {
            // 清除聚焦状态
            CLEAR_STATE(layer, LAYER_STATE_FOCUSED);
            last_clicked_component = NULL;  // 清除最后点击的组件记录
            // 重置强制焦点标记
            force_focus_debug = false;
            printf("Focus state cleared (current value: %d) on click outside (last clicked component)\n", layer->state);
            printf("DEBUG: force_focus_debug flag set to FALSE\n");
            input_component_debug_focus_state();
        } else {
            printf("Not resetting state: current component is not the last clicked component\n");
        }
    }
}

// 渲染输入组件
void input_component_render(Layer* layer) {
    if (!layer || !layer->component) {
        printf("Render skipped: invalid layer or component\n");
        return;
    }
    
    InputComponent* component = (InputComponent*)layer->component;
    const char* label_text = layer_get_label(layer);
    
    printf("\n--- RENDER CYCLE START ---\n");
    printf("Rendering input component: layer address=%p, component address=%p\n", layer, component);
    
    // 调用调试函数检查焦点状态
    input_component_debug_focus_state();
    
    printf("Layer state during render: %d (NORMAL=%d, FOCUSED=%d, DISABLED=%d)\n", 
           layer->state, LAYER_STATE_NORMAL, LAYER_STATE_FOCUSED, LAYER_STATE_DISABLED);
    printf("State flags: FOCUSED=%d, HOVER=%d, PRESSED=%d, DISABLED=%d\n",
           HAS_STATE(layer, LAYER_STATE_FOCUSED), HAS_STATE(layer, LAYER_STATE_HOVER),
           HAS_STATE(layer, LAYER_STATE_PRESSED), HAS_STATE(layer, LAYER_STATE_DISABLED));

    // 绘制输入框背景和边框（一步完成）
    Color border_color = (Color){150, 150, 150, 255};
    if (HAS_STATE(layer, LAYER_STATE_FOCUSED)) {
        border_color = (Color){70, 130, 180, 255}; // 聚焦时使用蓝色边框
    }
    
    if (layer->bg_color.a > 0) {
        if (layer->radius > 0) {
            backend_render_rounded_rect_with_border(&layer->rect, layer->bg_color, layer->radius, 2, border_color);
        } else {
            backend_render_fill_rect(&layer->rect, layer->bg_color);
            backend_render_rect_color(&layer->rect, border_color.r, border_color.g, border_color.b, border_color.a);
        }
    }
    
    // 渲染输入框标签
    if (label_text[0] != '\0') {
        // 使用ttf渲染文本
        
        Color text_color = layer->color;
        Texture* text_texture = render_text(layer,label_text, text_color);
        
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
    
    // 全局调试选项：如果启用了强制焦点，则将组件状态设置为聚焦
    // 注意：在实际生产环境中应禁用此调试选项
    if (force_focus_debug) {
        SET_STATE(layer, LAYER_STATE_FOCUSED);
        printf("DEBUG: Forcing component to FOCUSED state due to global debug flag\n");
    }
    
    // 渲染文本
    Color text_color = layer->color;
    const char* display_text = component->text;
    
    // 如果没有输入文本且不是聚焦状态，显示占位文本
    if (strlen(component->text) == 0 && !HAS_STATE(layer, LAYER_STATE_FOCUSED) && 
        strlen(component->placeholder) > 0) {
        display_text = component->placeholder;
        text_color = (Color){150, 150, 150, 150};  // 占位文本使用灰色
    }
    
    // 注意：标签文本已经在前面渲染过了，这里不需要重复渲染
    
    Texture* text_texture = render_text(layer, display_text, text_color);
    
    if (text_texture) {
        int text_width, text_height;
        backend_query_texture(text_texture, NULL, NULL, &text_width, &text_height);
        
        int start_x = layer->rect.x + 5;  // 默认左侧留5像素边距
        
        // 如果图层有label文本，将text_rect放在label文字的右边
        if (label_text[0] != '\0' && layer->font && layer->font->default_font) {
            // 计算label文本的宽度
            Texture* label_texture = backend_render_texture(layer->font->default_font, label_text, layer->color);
            if (label_texture) {
                int label_width;
                backend_query_texture(label_texture, NULL, NULL, &label_width, NULL);
                backend_render_text_destroy(label_texture);
                
                // 将text_rect放在label文字的右边，留出5像素间距
                start_x = layer->rect.x + label_width / scale + 10;
            }
        }
        
        Rect text_rect = {
            start_x,
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
    
    // 调试信息：检查当前状态和组件实例
    printf("Rendering input component: state=%d, LAYER_STATE_FOCUSED=%d\n", 
           layer->state, LAYER_STATE_FOCUSED);
    
    // 比较当前组件和最后点击的组件是否为同一实例
    printf("Component comparison: render component=%p, last_clicked_component=%p\n", 
           component, last_clicked_component);
    if (component == last_clicked_component) {
        printf("This component is the last clicked component.\n");
    } else {
        printf("This component is NOT the last clicked component.\n");
    }
    
    // DEBUG OPTION: Uncomment the line below to force the component to be in focused state
    // layer->state = LAYER_STATE_FOCUSED;
    
    // 全局调试选项：如果启用了强制焦点，则将组件状态设置为聚焦
    if (force_focus_debug) {
        SET_STATE(layer, LAYER_STATE_FOCUSED);
        printf("DEBUG: Forcing component to FOCUSED state due to global debug flag\n");
    }
    
    // 聚焦状态下绘制光标
    if (HAS_STATE(layer, LAYER_STATE_FOCUSED)) {
        // 调试信息：检查焦点状态和光标属性
        printf("Cursor rendering: state=%d, color=(%d,%d,%d,%d)\n", 
               layer->state, 
               component->cursor_color.r, component->cursor_color.g, 
               component->cursor_color.b, component->cursor_color.a);
        
        // 计算光标位置
        int cursor_x = layer->rect.x + 5;  // 默认左侧边距
        int cursor_y = layer->rect.y + 5;
        int cursor_height = layer->rect.h - 10;
        
        // 如果有文本，计算光标前面的文本宽度，更新光标x坐标
        if (component->cursor_pos > 0 && strlen(component->text) > 0) {
            // 创建光标前的子串
            char cursor_text[component->cursor_pos + 1];
            strncpy(cursor_text, component->text, component->cursor_pos);
            cursor_text[component->cursor_pos] = '\0';
            
            // 如果图层有标签文本，需要调整起始位置
            if (label_text[0] != '\0' && layer->font && layer->font->default_font) {
                Texture* label_texture = backend_render_texture(layer->font->default_font, label_text, layer->color);
                if (label_texture) {
                    int label_width;
                    backend_query_texture(label_texture, NULL, NULL, &label_width, NULL);
                    backend_render_text_destroy(label_texture);
                    
                    // 将光标起始位置放在label文字的右边，留出5像素间距
                    cursor_x = layer->rect.x + label_width / scale + 10;
                }
            }
            
            // 计算光标前文本的宽度
            if (layer->font && layer->font->default_font) {
                Texture* cursor_text_texture = backend_render_texture(layer->font->default_font, cursor_text, text_color);
                if (cursor_text_texture) {
                    int text_width;
                    backend_query_texture(cursor_text_texture, NULL, NULL, &text_width, NULL);
                    backend_render_text_destroy(cursor_text_texture);
                    
                    // 更新光标x坐标
                    cursor_x += text_width / scale;
                }
            }
        } else if (label_text[0] != '\0' && layer->font && layer->font->default_font) {
            // 没有文本但有标签，将光标放在标签右侧
            Texture* label_texture = backend_render_texture(layer->font->default_font, label_text, layer->color);
            if (label_texture) {
                int label_width;
                backend_query_texture(label_texture, NULL, NULL, &label_width, NULL);
                backend_render_text_destroy(label_texture);
                
                cursor_x = layer->rect.x + label_width / scale + 10;
            }
        }
        
        printf("Cursor position: x=%d, y=%d, height=%d\n", cursor_x, cursor_y, cursor_height);
        
        // 确保光标不超出输入框的右边界
        int input_right_boundary = layer->rect.x + layer->rect.w;
        if (cursor_x > input_right_boundary) {
            cursor_x = input_right_boundary - 2; // 留出2像素边距
            printf("DEBUG: Cursor position adjusted to stay within input bounds. New x=%d\n", cursor_x);
        }
        
        // 使用自定义光标颜色绘制垂直线作为光标
        backend_render_line(cursor_x, cursor_y, cursor_x, cursor_y + cursor_height, component->cursor_color);
    } else {
        printf("Not rendering cursor: state=%d (expected FOCUSED flag to be set)\n", 
               layer->state);
    }
    
    printf("--- RENDER CYCLE END ---\n\n");
}