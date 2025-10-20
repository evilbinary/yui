#include "text_component.h"
#include "../ytype.h"
#include "../layer.h"
#include "../event.h"
#include "../backend.h"
#include "../render.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

// 创建文本组件
TextComponent* text_component_create(Layer* layer) {
    TextComponent* component = (TextComponent*)malloc(sizeof(TextComponent));
    if (!component) {
        return NULL;
    }
    
    component->layer = layer;
    component->text[0] = '\0';
    component->placeholder[0] = '\0';
    component->cursor_pos = 0;
    component->selection_start = -1;
    component->selection_end = -1;
    component->max_length = MAX_TEXT * 4;
    component->cursor_color = (Color){0, 0, 0, 255};
    component->scroll_x = 0;
    component->scroll_y = 0;
    component->line_height = 20;
    component->multiline = 0;
    component->editable = 0;
    
    // 存储组件指针到图层的component字段中
    layer->component = component;
    layer->render = text_component_render;
    layer->handle_key_event = text_component_handle_key_event;
    layer->handle_mouse_event = text_component_handle_mouse_event;
    
    return component;
}

TextComponent* text_component_create_from_json(Layer* layer,cJSON* json_obj){
    TextComponent* component = text_component_create(layer);
    // 设置文本内容
    if (cJSON_HasObjectItem(json_obj, "text")) {
        text_component_set_text(layer->component, cJSON_GetObjectItem(json_obj, "text")->valuestring);
    }
    
    // 解析placeholder属性
    if (cJSON_HasObjectItem(json_obj, "placeholder")) {
        text_component_set_placeholder(layer->component, cJSON_GetObjectItem(json_obj, "placeholder")->valuestring);
    }
    
    // 解析maxLength属性
    if (cJSON_HasObjectItem(json_obj, "maxLength")) {
        text_component_set_max_length(layer->component, cJSON_GetObjectItem(json_obj, "maxLength")->valueint);
    }
    
    // 解析multiline属性
    if (cJSON_HasObjectItem(json_obj, "multiline")) {
        text_component_set_multiline(layer->component, cJSON_IsTrue(cJSON_GetObjectItem(json_obj, "multiline")));
    }
    
    // 解析editable属性
    if (cJSON_HasObjectItem(json_obj, "editable")) {
        text_component_set_editable(layer->component, cJSON_IsTrue(cJSON_GetObjectItem(json_obj, "editable")));
    }

    return component;

}

// 销毁文本组件
void text_component_destroy(TextComponent* component) {
    if (component) {
        free(component);
    }
}

// 设置文本内容
void text_component_set_text(TextComponent* component, const char* text) {
    if (!component || !text) {
        return;
    }
    
    strncpy(component->text, text, component->max_length - 1);
    component->text[component->max_length - 1] = '\0';
    component->cursor_pos = strlen(component->text);
    component->selection_start = -1;
    component->selection_end = -1;
}

// 设置占位符
void text_component_set_placeholder(TextComponent* component, const char* placeholder) {
    if (!component || !placeholder) {
        return;
    }
    
    strncpy(component->placeholder, placeholder, MAX_TEXT - 1);
    component->placeholder[MAX_TEXT - 1] = '\0';
}

// 设置最大长度
void text_component_set_max_length(TextComponent* component, int max_length) {
    if (!component) {
        return;
    }
    
    component->max_length = max_length;
    // 确保文本不超过新的最大长度
    if (strlen(component->text) >= max_length) {
        component->text[max_length - 1] = '\0';
        component->cursor_pos = max_length - 1;
    }
}

// 设置光标颜色
void text_component_set_cursor_color(TextComponent* component, Color cursor_color) {
    if (!component) {
        return;
    }
    
    component->cursor_color = cursor_color;
}

// 设置多行模式
void text_component_set_multiline(TextComponent* component, int multiline) {
    if (!component) {
        return;
    }
    
    component->multiline = multiline;
}

// 设置可编辑性
void text_component_set_editable(TextComponent* component, int editable) {
    if (!component) {
        return;
    }
    
    component->editable = editable;
}

// 删除选中文本
static void text_component_delete_selection(TextComponent* component) {
    if (component->selection_start == -1 || component->selection_end == -1) {
        return;
    }
    
    int start = component->selection_start;
    int end = component->selection_end;
    
    if (start > end) {
        int temp = start;
        start = end;
        end = temp;
    }
    
    memmove(component->text + start, component->text + end, strlen(component->text) - end + 1);
    component->cursor_pos = start;
    component->selection_start = -1;
    component->selection_end = -1;
}

// 在光标位置插入字符
static void text_component_insert_char(TextComponent* component, char c) {
    if (!component->editable) {
        return;
    }
    
    int len = strlen(component->text);
    
    // 如果达到最大长度，不插入
    if (len >= component->max_length - 1) {
        return;
    }
    
    // 如果有选中的文本，先删除
    if (component->selection_start != -1 && component->selection_end != -1) {
        text_component_delete_selection(component);
    }
    
    // 插入字符
    memmove(component->text + component->cursor_pos + 1, component->text + component->cursor_pos, len - component->cursor_pos + 1);
    component->text[component->cursor_pos] = c;
    component->cursor_pos++;
}

// 删除光标前的字符
static void text_component_delete_prev_char(TextComponent* component) {
    if (!component->editable || component->cursor_pos <= 0) {
        return;
    }
    
    // 如果有选中的文本，先删除
    if (component->selection_start != -1 && component->selection_end != -1) {
        text_component_delete_selection(component);
        return;
    }
    
    memmove(component->text + component->cursor_pos - 1, component->text + component->cursor_pos, strlen(component->text) - component->cursor_pos + 1);
    component->cursor_pos--;
}

// 删除光标后的字符
static void text_component_delete_next_char(TextComponent* component) {
    if (!component->editable || component->cursor_pos >= strlen(component->text)) {
        return;
    }
    
    // 如果有选中的文本，先删除
    if (component->selection_start != -1 && component->selection_end != -1) {
        text_component_delete_selection(component);
        return;
    }
    
    memmove(component->text + component->cursor_pos, component->text + component->cursor_pos + 1, strlen(component->text) - component->cursor_pos);
}

// 处理键盘事件
void text_component_handle_key_event(Layer* layer, KeyEvent* event) {
    if (!layer || !event || !layer->component) {
        return;
    }
    
    TextComponent* component = (TextComponent*)layer->component;
    
    if (!component->editable) {
        return;
    }
    
    // 处理特殊键
    if (event->type == KEY_EVENT_DOWN) {
        switch (event->data.key.key_code) {
            case SDLK_BACKSPACE:
                text_component_delete_prev_char(component);
                break;
            case SDLK_DELETE:
                text_component_delete_next_char(component);
                break;
            case SDLK_LEFT:
                if (component->cursor_pos > 0) {
                    component->cursor_pos--;
                    // 清除选择
                    component->selection_start = -1;
                    component->selection_end = -1;
                }
                break;
            case SDLK_RIGHT:
                if (component->cursor_pos < strlen(component->text)) {
                    component->cursor_pos++;
                    // 清除选择
                    component->selection_start = -1;
                    component->selection_end = -1;
                }
                break;
            case SDLK_UP:
                if (component->multiline) {
                    // 简单的向上移动光标实现
                    int pos = component->cursor_pos;
                    while (pos > 0 && component->text[pos] != '\n') {
                        pos--;
                    }
                    if (pos > 0) {
                        pos--;
                        // 尽量保持在同一列
                        int col = 0;
                        int temp = component->cursor_pos;
                        while (temp > 0 && component->text[temp - 1] != '\n') {
                            col++;
                            temp--;
                        }
                        
                        temp = pos;
                        while (col > 0 && temp < strlen(component->text) && component->text[temp] != '\n') {
                            temp++;
                            col--;
                        }
                        component->cursor_pos = temp;
                    } else {
                        component->cursor_pos = 0;
                    }
                    // 清除选择
                    component->selection_start = -1;
                    component->selection_end = -1;
                }
                break;
            case SDLK_DOWN:
                if (component->multiline) {
                    // 简单的向下移动光标实现
                    int pos = component->cursor_pos;
                    while (pos < strlen(component->text) && component->text[pos] != '\n') {
                        pos++;
                    }
                    if (pos < strlen(component->text)) {
                        pos++;
                        // 尽量保持在同一列
                        int col = 0;
                        int temp = component->cursor_pos;
                        while (temp > 0 && component->text[temp - 1] != '\n') {
                            col++;
                            temp--;
                        }
                        
                        temp = pos;
                        while (col > 0 && temp < strlen(component->text) && component->text[temp] != '\n') {
                            temp++;
                            col--;
                        }
                        component->cursor_pos = temp;
                    } else {
                        component->cursor_pos = strlen(component->text);
                    }
                    // 清除选择
                    component->selection_start = -1;
                    component->selection_end = -1;
                }
                break;
            case SDLK_RETURN:
            case SDLK_KP_ENTER:
                if (component->multiline) {
                    text_component_insert_char(component, '\n');
                }
                break;
        }
    } else if (event->type == KEY_EVENT_TEXT_INPUT) {
        // 处理文本输入
        for (int i = 0; i < strlen(event->data.text.text); i++) {
            if (isprint(event->data.text.text[i])) {
                text_component_insert_char(component, event->data.text.text[i]);
            }
        }
    }
}

// 处理鼠标事件
void text_component_handle_mouse_event(Layer* layer, MouseEvent* event) {
    if (!layer || !event || !layer->component) {
        return;
    }
    
    TextComponent* component = (TextComponent*)layer->component;
    
    // 简单实现点击设置光标位置（实际应用中需要更复杂的计算）
    if (event->state == BUTTON_PRESSED && event->button == BUTTON_LEFT) {
        Point pt = {event->x, event->y};
        if (point_in_rect(pt, layer->rect)) {
            // 这里只是简单设置光标到文本末尾，实际应用中需要计算点击位置对应的文本位置
            component->cursor_pos = strlen(component->text);
            component->selection_start = -1;
            component->selection_end = -1;
            // 设置焦点
            focused_layer = layer;
        }
    }
}

// 渲染文本组件
void text_component_render(Layer* layer) {
    if (!layer || !layer->component) {
        return;
    }
    
    TextComponent* component = (TextComponent*)layer->component;
    
    // 绘制背景
    backend_render_fill_rect(&layer->rect, layer->bg_color);
    
    // 准备渲染区域
    Rect render_rect = layer->rect;
    render_rect.x += 5;  // 内边距
    render_rect.y += 5;
    render_rect.w -= 10;
    render_rect.h -= 10;
    
    // 保存当前裁剪区域
    Rect prev_clip;
    backend_render_get_clip_rect(&prev_clip);
    
    // 设置裁剪区域
    backend_render_set_clip_rect(&render_rect);
    
    // 如果文本为空且有占位符，显示占位符
    if (strlen(component->text) == 0 && strlen(component->placeholder) > 0) {
        // 使用透明度较低的颜色绘制占位符
        Color placeholder_color = {150, 150, 150, 128};
        Texture* tex = backend_render_texture(layer->font->default_font, component->placeholder, placeholder_color);
        if (tex) {
            backend_render_text_copy(tex, NULL, &render_rect);
            backend_render_text_destroy(tex);
        }
    } else {
        // 绘制文本
        Texture* tex = backend_render_texture(layer->font->default_font, component->text, layer->color);
        if (tex) {
            backend_render_text_copy(tex, NULL, &render_rect);
            backend_render_text_destroy(tex);
        }
    }
    
    // 如果组件可编辑，绘制光标
    if (component->editable && HAS_STATE(layer, LAYER_STATE_FOCUSED)) {
        // 简单计算光标位置（实际应用中需要更复杂的计算）
        char* temp_text = (char*)malloc(component->cursor_pos + 1);
        if (temp_text) {
            strncpy(temp_text, component->text, component->cursor_pos);
            temp_text[component->cursor_pos] = '\0';
            
            // 计算光标位置
            int w, h;
            TTF_SizeUTF8(layer->font->default_font, temp_text, &w, &h);
            
            // 绘制光标
            Rect cursor_rect;
            cursor_rect.x = render_rect.x + w;
            cursor_rect.y = render_rect.y;
            cursor_rect.w = 2;
            cursor_rect.h = h;
            
            backend_render_fill_rect(&cursor_rect, component->cursor_color);
            
            free(temp_text);
        }
    }
    
    // 恢复裁剪区域
    backend_render_set_clip_rect(&prev_clip);
    
    // 如果是多行模式且需要滚动，绘制滚动条（简单实现）
    if (component->multiline) {
        // 这里可以添加滚动条的渲染逻辑
    }
}