#include "text_component.h"
#include "../ytype.h"
#include "../layer.h"
#include "../event.h"
#include "../backend.h"
#include "../render.h"
#include "../util.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>


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
    component->editable = 1;  // 默认设置为可编辑状态
    component->show_line_numbers = 0;  // 默认不显示行号
    component->line_number_width = 40;  // 默认行号区域宽度
    component->line_number_color = (Color){133, 133, 133, 255};  // 默认行号颜色
    component->line_number_bg_color = (Color){30, 30, 30, 255};  // 默认行号背景颜色
    component->selection_color = (Color){70, 130, 180, 100};  // 默认选中颜色（半透明蓝色）
    
    // 设置图层的焦点属性，使其可以获得焦点
    layer->focusable = 1;
    
    // 存储组件指针到图层的component字段中
    layer->component = component;
    layer->render = text_component_render;
    layer->handle_key_event = text_component_handle_key_event;
    layer->handle_mouse_event = text_component_handle_mouse_event;
    
    return component;
}

TextComponent* text_component_create_from_json(Layer* layer,cJSON* json_obj){
    TextComponent* component = text_component_create(layer);
    
    // 强制设置可编辑和可获得焦点
    layer->focusable = 1;
    component->editable = 1;
    
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
    
    // 解析showLineNumbers属性
    if (cJSON_HasObjectItem(json_obj, "showLineNumbers")) {
        text_component_set_show_line_numbers(layer->component, cJSON_IsTrue(cJSON_GetObjectItem(json_obj, "showLineNumbers")));
    }
    
    // 解析lineNumberWidth属性
    if (cJSON_HasObjectItem(json_obj, "lineNumberWidth")) {
        text_component_set_line_number_width(layer->component, cJSON_GetObjectItem(json_obj, "lineNumberWidth")->valueint);
    }
    
    // 解析selectionColor属性
    if (cJSON_HasObjectItem(json_obj, "selectionColor")) {
        cJSON* color_obj = cJSON_GetObjectItem(json_obj, "selectionColor");
        if (cJSON_IsString(color_obj)) {
            // 使用util.c中的parse_color函数解析十六进制颜色字符串
            Color selection_color = {70, 130, 180, 100}; // 默认值
            parse_color(color_obj->valuestring, &selection_color);
            text_component_set_selection_color(layer->component, selection_color);
        }
        else if (cJSON_IsObject(color_obj)) {
            // 兼容旧的RGB对象格式
            Color selection_color = {70, 130, 180, 100}; // 默认值
            if (cJSON_HasObjectItem(color_obj, "r")) {
                selection_color.r = cJSON_GetObjectItem(color_obj, "r")->valueint;
            }
            if (cJSON_HasObjectItem(color_obj, "g")) {
                selection_color.g = cJSON_GetObjectItem(color_obj, "g")->valueint;
            }
            if (cJSON_HasObjectItem(color_obj, "b")) {
                selection_color.b = cJSON_GetObjectItem(color_obj, "b")->valueint;
            }
            if (cJSON_HasObjectItem(color_obj, "a")) {
                selection_color.a = cJSON_GetObjectItem(color_obj, "a")->valueint;
            }
            text_component_set_selection_color(layer->component, selection_color);
        }
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

    // 同时更新焦点属性
    if (component->layer) {
        component->layer->focusable = editable;
    }
}

// 设置是否显示行号
void text_component_set_show_line_numbers(TextComponent* component, int show_line_numbers) {
    if (!component) {
        return;
    }
    
    component->show_line_numbers = show_line_numbers;
}

// 设置行号区域宽度
void text_component_set_line_number_width(TextComponent* component, int width) {
    if (!component) {
        return;
    }
    
    component->line_number_width = width;
}

// 设置行号颜色
void text_component_set_line_number_color(TextComponent* component, Color color) {
    if (!component) {
        return;
    }
    
    component->line_number_color = color;
}

// 设置行号背景颜色
void text_component_set_line_number_bg_color(TextComponent* component, Color color) {
    if (!component) {
        return;
    }
    
    component->line_number_bg_color = color;
}

// 设置选中颜色
void text_component_set_selection_color(TextComponent* component, Color color) {
    if (!component) {
        return;
    }
    
    component->selection_color = color;
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
    
    // 确保start不为负数
    if (start < 0) start = 0;
    // 确保end不超过文本长度
    int text_len = strlen(component->text);
    if (end > text_len) end = text_len;
    
    // 只有当start < end时才执行删除操作
    if (start < end) {
        memmove(component->text + start, component->text + end, text_len - end + 1);
        component->cursor_pos = start;
    }
    
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
        // 更新len，因为删除选择后文本长度可能改变
        len = strlen(component->text);
    }
    
    // 确保cursor_pos在有效范围内
    if (component->cursor_pos < 0) component->cursor_pos = 0;
    if (component->cursor_pos > len) component->cursor_pos = len;

    // 插入字符
    if (component->cursor_pos < component->max_length - 1) {
        memmove(component->text + component->cursor_pos + 1, component->text + component->cursor_pos, len - component->cursor_pos + 1);
        component->text[component->cursor_pos] = c;
        component->text[component->cursor_pos + 1 + len - component->cursor_pos] = '\0'; // 确保字符串以null结尾
        component->cursor_pos++;
    }
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
    
    int len = strlen(component->text);
    
    // 确保不会越界
    if (component->cursor_pos > len) {
        component->cursor_pos = len;
    }
    
    if (component->cursor_pos > 0) {
        memmove(component->text + component->cursor_pos - 1, component->text + component->cursor_pos, len - component->cursor_pos + 1);
        component->cursor_pos--;
    }
}

// 删除光标后的字符
static void text_component_delete_next_char(TextComponent* component) {
    if (!component->editable) {
        return;
    }
    
    // 如果有选中的文本，先删除
    if (component->selection_start != -1 && component->selection_end != -1) {
        text_component_delete_selection(component);
        return;
    }
    
    int len = strlen(component->text);
    
    // 确保cursor_pos在有效范围内
    if (component->cursor_pos < 0) component->cursor_pos = 0;
    if (component->cursor_pos >= len) {
        return; // 已经在文本末尾，无需删除
    }
    
    memmove(component->text + component->cursor_pos, component->text + component->cursor_pos + 1, len - component->cursor_pos);
    component->text[len - 1] = '\0'; // 确保字符串以null结尾
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
                    // 如果按住Shift，则扩展选择，否则清除选择
                    if (!(event->data.key.mod & KMOD_SHIFT)) {
                        component->selection_start = -1;
                        component->selection_end = -1;
                    } else if (component->selection_start == -1) {
                        // 开始新的选择
                        component->selection_start = component->cursor_pos + 1;
                        component->selection_end = component->cursor_pos;
                    } else {
                        // 扩展现有选择
                        component->selection_end = component->cursor_pos;
                    }
                }
                break;
            case SDLK_RIGHT:
                if (component->cursor_pos < strlen(component->text)) {
                    component->cursor_pos++;
                    // 如果按住Shift，则扩展选择，否则清除选择
                    if (!(event->data.key.mod & KMOD_SHIFT)) {
                        component->selection_start = -1;
                        component->selection_end = -1;
                    } else if (component->selection_start == -1) {
                        // 开始新的选择
                        component->selection_start = component->cursor_pos - 1;
                        component->selection_end = component->cursor_pos;
                    } else {
                        // 扩展现有选择
                        component->selection_end = component->cursor_pos;
                    }
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
                    // 如果按住Shift，则扩展选择，否则清除选择
                    if (!(event->data.key.mod & KMOD_SHIFT)) {
                        component->selection_start = -1;
                        component->selection_end = -1;
                    } else if (component->selection_start == -1) {
                        component->selection_start = component->cursor_pos;
                        component->selection_end = component->cursor_pos;
                    } else {
                        component->selection_end = component->cursor_pos;
                    }
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
                    // 如果按住Shift，则扩展选择，否则清除选择
                    if (!(event->data.key.mod & KMOD_SHIFT)) {
                        component->selection_start = -1;
                        component->selection_end = -1;
                    } else if (component->selection_start == -1) {
                        component->selection_start = component->cursor_pos;
                        component->selection_end = component->cursor_pos;
                    } else {
                        component->selection_end = component->cursor_pos;
                    }
                }
                break;
            case SDLK_RETURN:
            case SDLK_KP_ENTER:
                // 无论是否为多行模式，都支持换行
                text_component_insert_char(component, '\n');
                // 如果是单行模式，设置为多行模式以显示换行效果
                if (!component->multiline) {
                    component->multiline = true;
                }
                break;
            case SDLK_a:
                // Ctrl+A 全选
                if (event->data.key.mod & KMOD_CTRL) {
                    component->selection_start = 0;
                    component->selection_end = strlen(component->text);
                    component->cursor_pos = strlen(component->text);
                } else if (event->type == KEY_EVENT_TEXT_INPUT) {
                    text_component_insert_char(component, 'a');
                }
                break;
            case SDLK_c:
                // Ctrl+C 复制（暂时仅打印到控制台）
                if (event->data.key.mod & KMOD_CTRL) {
                    if (component->selection_start != -1 && component->selection_end != -1) {
                        int start = component->selection_start;
                        int end = component->selection_end;
                        if (start > end) {
                            int temp = start;
                            start = end;
                            end = temp;
                        }
                        
                        // 提取选中文本
                        int len = end - start;
                        char* selected_text = (char*)malloc(len + 1);
                        if (selected_text) {
                            strncpy(selected_text, component->text + start, len);
                            selected_text[len] = '\0';
                            printf("Copied: '%s'\n", selected_text);
                            free(selected_text);
                        }
                    }
                } else if (event->type == KEY_EVENT_TEXT_INPUT) {
                    text_component_insert_char(component, 'c');
                }
                break;
            case SDLK_v:
                // Ctrl+V 粘贴（暂时仅从控制台读取）
                if (event->data.key.mod & KMOD_CTRL) {
                    // 这里应该从剪贴板读取，但暂时跳过
                    printf("Paste not implemented yet\n");
                } else if (event->type == KEY_EVENT_TEXT_INPUT) {
                    text_component_insert_char(component, 'v');
                }
                break;
            case SDLK_x:
                // Ctrl+X 剪切
                if (event->data.key.mod & KMOD_CTRL) {
                    if (component->selection_start != -1 && component->selection_end != -1) {
                        int start = component->selection_start;
                        int end = component->selection_end;
                        if (start > end) {
                            int temp = start;
                            start = end;
                            end = temp;
                        }
                        
                        // 提取选中文本
                        int len = end - start;
                        char* selected_text = (char*)malloc(len + 1);
                        if (selected_text) {
                            strncpy(selected_text, component->text + start, len);
                            selected_text[len] = '\0';
                            printf("Cut: '%s'\n", selected_text);
                            free(selected_text);
                            
                            // 删除选中文本
                            memmove(component->text + start, component->text + end, strlen(component->text) - end + 1);
                            component->cursor_pos = start;
                            component->selection_start = -1;
                            component->selection_end = -1;
                        }
                    }
                } else if (event->type == KEY_EVENT_TEXT_INPUT) {
                    text_component_insert_char(component, 'x');
                }
                break;
            default:
                // 处理其他普通字符输入
                if (event->type == KEY_EVENT_TEXT_INPUT) {
                    for (int i = 0; i < strlen(event->data.text.text); i++) {
                        // 移除isprint限制，支持所有字符（包括中文等多字节字符）
                        // 但过滤掉控制字符（ASCII < 32，但不包括制表符、换行符等特殊控制字符）
                        unsigned char c = event->data.text.text[i];
                        if (c >= 32 || c == '\t' || c == '\n') {
                            text_component_insert_char(component, event->data.text.text[i]);
                        }
                    }
                }
                break;
        }
    } else if (event->type == KEY_EVENT_TEXT_INPUT) {
        // 处理文本输入
        for (int i = 0; i < strlen(event->data.text.text); i++) {
            // 移除isprint限制，支持所有字符（包括中文等多字节字符）
            // 但过滤掉控制字符（ASCII < 32，但不包括制表符、换行符等特殊控制字符）
            unsigned char c = event->data.text.text[i];
            if (c >= 32 || c == '\t' || c == '\n') {
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
    
    // 处理滚轮事件
    // 由于Layer结构体没有handle_scroll_event字段
    // 我们需要在backend_sdl.c中特别处理文本组件的滚轮事件
    // 这里仅作为占位
    
    // 鼠标按下时设置光标位置
    if (event->state == BUTTON_PRESSED && event->button == BUTTON_LEFT) {
        Point pt = {event->x, event->y};
        if (point_in_rect(pt, layer->rect)) {
            // 计算点击位置对应的文本位置
            int click_pos = text_component_get_position_from_point(component, pt, layer);
            component->cursor_pos = click_pos;
            
            // 如果按住Shift键，则扩展选择
            if (component->selection_start == -1) {
                // 开始新选择
                component->selection_start = click_pos;
                component->selection_end = click_pos;
            } else {
                // 扩展现有选择
                component->selection_end = click_pos;
            }
        } else {
            // 点击文本区域外，清除选择
            component->selection_start = -1;
            component->selection_end = -1;
        }
    }
    // 鼠标拖动时更新选择
    else if (event->state == SDL_MOUSEMOTION && (event->button == BUTTON_LEFT)) {
        Point pt = {event->x, event->y};
        if (point_in_rect(pt, layer->rect)) {
            // 计算拖动位置对应的文本位置
            int drag_pos = text_component_get_position_from_point(component, pt, layer);
            
            // 更新选择终点
            if (component->selection_start != -1) {
                component->selection_end = drag_pos;
                component->cursor_pos = drag_pos;
            }
        }
    }
}







// 辅助函数：根据鼠标坐标计算对应的文本位置
int text_component_get_position_from_point(TextComponent* component, Point pt, Layer* layer) {
    if (!component || !layer) {
        return 0;
    }
    
    // 准备渲染区域
    Rect render_rect = layer->rect;
    int left_padding = 5;
    
    // 如果显示行号，为行号区域预留空间
    if (component->show_line_numbers && component->multiline) {
        left_padding += component->line_number_width;
    }
    
    render_rect.x += left_padding;
    render_rect.y += 5;
    render_rect.w -= (left_padding + 5);
    render_rect.h -= 10;
    
    // 检查点是否在渲染区域内
    if (!point_in_rect(pt, render_rect)) {
        // 如果点击在文本区域上方，返回0
        if (pt.y < render_rect.y) {
            return 0;
        }
        // 如果点击在文本区域下方，返回文本末尾
        if (pt.y > render_rect.y + render_rect.h) {
            return strlen(component->text);
        }
        // 如果点击在左侧，返回0
        if (pt.x < render_rect.x) {
            return 0;
        }
        // 如果点击在右侧，返回文本末尾
        if (pt.x > render_rect.x + render_rect.w) {
            return strlen(component->text);
        }
    }
    
    // 如果没有文本，返回0
    if (strlen(component->text) == 0) {
        return 0;
    }
    
    // 简单实现：假设等宽字体，计算大致位置
    if (component->multiline) {
        // 多行模式：需要计算行和列
        int line_height = 20; // 默认行高
        int char_width = 8;  // 假设平均字符宽度
        
        // 获取实际字符宽度
        Texture* temp_tex = backend_render_texture(layer->font->default_font, "X", layer->color);
        if (temp_tex) {
            int temp_width, temp_height;
            backend_query_texture(temp_tex, NULL, NULL, &temp_width, &temp_height);
            char_width = temp_width / scale;
            line_height = temp_height / scale;
            backend_render_text_destroy(temp_tex);
        }
        
        // 计算行号（考虑滚动偏移）
        int line_y = pt.y - render_rect.y + component->scroll_y;
        int line_num = line_y / line_height;
        
        // 查找对应行的起始位置
        int current_line = 0;
        int pos = 0;
        int col = 0;
        
        for (int i = 0; i <= strlen(component->text); i++) {
            if (i == strlen(component->text) || component->text[i] == '\n') {
                if (current_line == line_num) {
                    // 计算列位置
                    int line_start_pos = pos - col;
                    col = (pt.x - render_rect.x) / char_width;
                    
                    // 限制列位置在行范围内
                    if (col < 0) col = 0;
                    if (col > strlen(component->text) - line_start_pos - (component->text[i] == '\n' ? 1 : 0)) {
                        col = strlen(component->text) - line_start_pos - (component->text[i] == '\n' ? 1 : 0);
                    }
                    
                    return line_start_pos + col;
                }
                current_line++;
                pos = i + 1;
                col = 0;
            } else {
                col++;
            }
        }
        
        // 如果超出文本行数，返回文本末尾
        return strlen(component->text);
    } else {
        // 单行模式：计算列位置
        int char_width = 8; // 假设平均字符宽度
        
        // 获取实际字符宽度
        Texture* temp_tex = backend_render_texture(layer->font->default_font, "X", layer->color);
        if (temp_tex) {
            int temp_width, temp_height;
            backend_query_texture(temp_tex, NULL, NULL, &temp_width, &temp_height);
            char_width = temp_width / scale;
            backend_render_text_destroy(temp_tex);
        }
        
        // 计算列位置
        int col = (pt.x - render_rect.x) / char_width;
        
        // 限制列位置在文本范围内
        if (col < 0) col = 0;
        if (col > strlen(component->text)) col = strlen(component->text);
        
        return col;
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
    
    // 如果显示行号且为多行模式，绘制行号背景和行号
    if (component->show_line_numbers && component->multiline) {
        // 绘制行号背景区域
        Rect line_number_bg = {
            layer->rect.x + 5,
            layer->rect.y + 5,
            component->line_number_width,
            layer->rect.h - 10
        };
        backend_render_fill_rect(&line_number_bg, component->line_number_bg_color);
        
        // 绘制分隔线
        Rect separator_line = {
            line_number_bg.x + line_number_bg.w,
            line_number_bg.y,
            1,
            line_number_bg.h
        };
        Color separator_color = {51, 51, 51, 255};
        backend_render_fill_rect(&separator_line, separator_color);
        
        // 计算文本行数
        char* text = component->text;
        int line_count = 1;
        if (strlen(text) > 0) {
            for (int i = 0; i < strlen(text); i++) {
                if (text[i] == '\n') {
                    line_count++;
                }
            }
        }
        
        // 获取行高用于行号定位
        int line_height = component->line_height;
        Texture* temp_tex = backend_render_texture(layer->font->default_font, "X", component->line_number_color);
        if (temp_tex) {
            int temp_width, temp_height;
            backend_query_texture(temp_tex, NULL, NULL, &temp_width, &temp_height);
            line_height = temp_height / scale;
            backend_render_text_destroy(temp_tex);
        }
        
        // 计算可见行范围
        int first_visible_line = component->scroll_y / (line_height + 2);
        int last_visible_line = (component->scroll_y + layer->rect.h - 10) / (line_height + 2) + 1;
        if (last_visible_line > line_count) {
            last_visible_line = line_count;
        }
        
        // 渲染可见行号
        for (int i = first_visible_line; i < last_visible_line; i++) {
            char line_num_str[16];
            snprintf(line_num_str, sizeof(line_num_str), "%d", i + 1);
            
            Texture* line_num_tex = backend_render_texture(layer->font->default_font, line_num_str, component->line_number_color);
            if (line_num_tex) {
                int num_width, num_height;
                backend_query_texture(line_num_tex, NULL, NULL, &num_width, &num_height);
                
                Rect line_num_rect = {
                    line_number_bg.x + line_number_bg.w - num_width / scale - 5,  // 右对齐，留5像素边距
                    line_number_bg.y + i * (line_height + 2) - component->scroll_y,
                    num_width / scale,
                    num_height / scale
                };
                
                // 只有当行号在可见区域内时才渲染
                if (line_num_rect.y + line_num_rect.h > line_number_bg.y && line_num_rect.y < line_number_bg.y + line_number_bg.h) {
                    backend_render_text_copy(line_num_tex, NULL, &line_num_rect);
                }
                
                backend_render_text_destroy(line_num_tex);
            }
        }
    }
    
    // 准备渲染区域
    Rect render_rect = layer->rect;
    int left_padding = 5;
    
    // 如果显示行号，为行号区域预留空间并添加分隔线
    if (component->show_line_numbers && component->multiline) {
        left_padding += component->line_number_width;
    }
    
    render_rect.x += left_padding;
    render_rect.y += 5;
    render_rect.w -= (left_padding + 5);  // 左内边距 + 右内边距
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
            int text_width, text_height;
            backend_query_texture(tex, NULL, NULL, &text_width, &text_height);
            
            // 计算文本位置
            Rect text_rect = {
                render_rect.x,
                // 多行模式下在顶部对齐，单行模式下垂直居中
                component->multiline ? render_rect.y : render_rect.y + (render_rect.h - text_height / scale) / 2,
                text_width / scale,
                text_height / scale
            };
            
            // 确保文本不会超出边界
            if (text_rect.x + text_rect.w > render_rect.x + render_rect.w) {
                text_rect.w = render_rect.x + render_rect.w - text_rect.x;
            }
            if (text_rect.y + text_rect.h > render_rect.y + render_rect.h) {
                text_rect.h = render_rect.y + render_rect.h - text_rect.y;
            }
            
            backend_render_text_copy(tex, NULL, &text_rect);
            backend_render_text_destroy(tex);
        }
    } else {
        // 绘制选择背景
        if (component->selection_start != -1 && component->selection_end != -1) {
            // 使用可配置的选择背景颜色
            Color selection_bg = component->selection_color;
            
            // 获取字符宽度
            int char_width = 8; // 假设平均字符宽度
            Texture* temp_tex = backend_render_texture(layer->font->default_font, "X", layer->color);
            if (temp_tex) {
                int temp_width, temp_height;
                backend_query_texture(temp_tex, NULL, NULL, &temp_width, &temp_height);
                char_width = temp_width / scale;
                backend_render_text_destroy(temp_tex);
            }
            
            // 获取行高
            int line_height = 20;
            Texture* line_tex = backend_render_texture(layer->font->default_font, "X", layer->color);
            if (line_tex) {
                int temp_width, temp_height;
                backend_query_texture(line_tex, NULL, NULL, &temp_width, &temp_height);
                line_height = temp_height / scale;
                backend_render_text_destroy(line_tex);
            }
            
            // 确保start <= end
            int start = component->selection_start;
            int end = component->selection_end;
            if (start > end) {
                int temp = start;
                start = end;
                end = temp;
            }
            
            if (component->multiline) {
                // 多行模式：逐行绘制选择背景
                char* text = component->text;
                int line_y = render_rect.y - component->scroll_y;
                int current_pos = 0;
                int line_start = 0;
                
                // 遍历每一行
                while (current_pos <= strlen(text)) {
                    // 找到当前行的结束位置
                    if (current_pos == strlen(text) || text[current_pos] == '\n') {
                        int line_end = current_pos;
                        
                        // 检查选择区域是否与当前行相交
                        if (start < line_end && end > line_start) {
                            // 计算当前行内的选择起始和结束位置
                            int sel_start_in_line = (start > line_start) ? start - line_start : 0;
                            int sel_end_in_line = (end < line_end) ? end - line_start : line_end - line_start;
                            
                            // 计算选择区域的屏幕坐标
                            Rect sel_rect = {
                                render_rect.x + sel_start_in_line * char_width,
                                line_y,
                                (sel_end_in_line - sel_start_in_line) * char_width,
                                line_height
                            };
                            
                            // 确保选择区域在可见范围内
                            if (sel_rect.y + sel_rect.h > render_rect.y && sel_rect.y < render_rect.y + render_rect.h) {
                                backend_render_fill_rect(&sel_rect, selection_bg);
                            }
                        }
                        
                        line_start = current_pos + 1;
                        line_y += line_height + 2; // 加2作为行间距
                    }
                    
                    current_pos++;
                }
            } else {
                // 单行模式
                Rect sel_rect = {
                    render_rect.x + start * char_width,
                    render_rect.y + (render_rect.h - line_height) / 2,
                    (end - start) * char_width,
                    line_height
                };
                
                backend_render_fill_rect(&sel_rect, selection_bg);
            }
        }
        
        // 绘制文本
        if (component->multiline) {
            // 多行模式下，实现自动换行
            char* text = component->text;
            int current_pos = 0;
            int line_y = render_rect.y; // 不减去layer->scroll_offset，因为在渲染时已经考虑了滚动
            int line_height = 0;
            
            // 先获取单行文本的高度
            Texture* temp_tex = backend_render_texture(layer->font->default_font, "X", layer->color);
            if (temp_tex) {
                int temp_width, temp_height;
                backend_query_texture(temp_tex, NULL, NULL, &temp_width, &temp_height);
                line_height = temp_height / scale;
                backend_render_text_destroy(temp_tex);
            }
            
            // 循环处理每一行
            while (current_pos < strlen(text)) {
                // 查找当前行可以显示的最大文本长度
                int line_end = current_pos;
                int max_width = render_rect.w;
                int current_width = 0;
                
                // 尝试找到合适的换行点
                while (line_end < strlen(text)) {
                    // 遇到换行符时立即换行
                    if (text[line_end] == '\n') {
                        break;
                    }
                    
                    // 临时文本（从current_pos到line_end+1）
                    char* temp_line = (char*)malloc(line_end - current_pos + 2);
                    if (temp_line) {
                        strncpy(temp_line, text + current_pos, line_end - current_pos + 1);
                        temp_line[line_end - current_pos + 1] = '\0';
                        
                        // 计算临时文本宽度
                        Texture* line_tex = backend_render_texture(layer->font->default_font, temp_line, layer->color);
                        if (line_tex) {
                            int line_width, line_height_ignore;
                            backend_query_texture(line_tex, NULL, NULL, &line_width, &line_height_ignore);
                            current_width = line_width / scale;
                            backend_render_text_destroy(line_tex);
                        }
                        
                        free(temp_line);
                    }
                    
                    // 如果超出宽度，停止
                    if (current_width > max_width && line_end > current_pos) {
                        break;
                    }
                    
                    // 记录空格位置，但只有当文本接近宽度限制时才在空格处换行
                    if (text[line_end] == ' ') {
                        int space_line_end = line_end;
                        int space_width = 0;
                        
                        // 计算到空格位置的文本宽度
                        char* temp_line = (char*)malloc(space_line_end - current_pos + 2);
                        if (temp_line) {
                            strncpy(temp_line, text + current_pos, space_line_end - current_pos + 1);
                            temp_line[space_line_end - current_pos + 1] = '\0';
                               
                            Texture* line_tex = backend_render_texture(layer->font->default_font, temp_line, layer->color);
                            if (line_tex) {
                                int line_width, line_height_ignore;
                                backend_query_texture(line_tex, NULL, NULL, &line_width, &line_height_ignore);
                                space_width = line_width / scale;
                                backend_render_text_destroy(line_tex);
                            }
                               
                            free(temp_line);
                        }
                        
                        // 只有当文本接近宽度限制时才在空格处换行
                        if (space_width > max_width * 0.9) { // 当文本达到宽度的90%时考虑换行
                            current_pos = space_line_end + 1;
                            break;
                        }
                    }
                    
                    line_end++;
                }
                
                // 确定当前行的结束位置
                int split_pos = current_pos;
                
                // 特殊处理换行符：如果遇到换行符，直接在换行符处结束当前行
                if (line_end < strlen(text) && text[line_end] == '\n') {
                    split_pos = line_end;
                }
                // 如果文本没有超过宽度限制，并且没有到文本末尾
                else if (current_width <= max_width && line_end < strlen(text)) {
                    split_pos = line_end; // 直接使用找到的line_end作为当前行的结束位置
                } 
                // 如果文本超过宽度限制，需要硬换行
                else if (current_width > max_width) {
                    // 找到最大的不超过宽度的位置
                    split_pos = current_pos;
                    while (split_pos < line_end) {
                        char* temp_line = (char*)malloc(split_pos - current_pos + 2);
                        if (temp_line) {
                            strncpy(temp_line, text + current_pos, split_pos - current_pos + 1);
                            temp_line[split_pos - current_pos + 1] = '\0';
                                  
                            Texture* line_tex = backend_render_texture(layer->font->default_font, temp_line, layer->color);
                            if (line_tex) {
                                int line_width, line_height_ignore;
                                backend_query_texture(line_tex, NULL, NULL, &line_width, &line_height_ignore);
                                if (line_width / scale > max_width) {
                                    break;
                                }
                                backend_render_text_destroy(line_tex);
                            }
                                  
                            free(temp_line);
                        }
                        split_pos++;
                    } 
                    
                    if (split_pos > current_pos) {
                        split_pos--;
                    }
                }
                // 如果到达文本末尾
                else if (line_end >= strlen(text)) {
                    split_pos = line_end;
                }
                
                // 渲染当前行
                char* current_line = (char*)malloc(split_pos - current_pos + 2);
                if (current_line) {
                    // 复制当前行文本，但不包含换行符
                    int copy_len = split_pos - current_pos;
                    // 检查是否在换行符处结束
                    if (copy_len > 0 && text[split_pos - 1] == '\n') {
                        copy_len--; // 不包含换行符
                    }
                    strncpy(current_line, text + current_pos, copy_len);
                    current_line[copy_len] = '\0';
                      
                    Texture* line_tex = backend_render_texture(layer->font->default_font, current_line, layer->color);
                    if (line_tex) {
                        int actual_width, actual_height;
                        backend_query_texture(line_tex, NULL, NULL, &actual_width, &actual_height);
                          
                        Rect text_rect = {
                            render_rect.x,
                            line_y,
                            actual_width / scale,  // 使用实际文本宽度，避免文字变形
                            actual_height / scale  // 使用实际文本高度
                        };
                          
                        // 确保文本不会超出边界
                        if (text_rect.x + text_rect.w > render_rect.x + render_rect.w) {
                            text_rect.w = render_rect.x + render_rect.w - text_rect.x;
                        }
                          
                        // 只有当行在可见区域内时才渲染
                    if (line_y + actual_height / scale > render_rect.y && line_y < render_rect.y + render_rect.h) {
                        backend_render_text_copy(line_tex, NULL, &text_rect);
                    }
                    backend_render_text_destroy(line_tex);
                } 
                
                free(current_line);
            }
              
            // 如果是在换行符处结束，直接跳到下一个字符
                if (split_pos < strlen(text) && text[split_pos] == '\n') {
                    current_pos = split_pos + 1;
                } else {
                    current_pos = split_pos;
                }
                
                // 移动到下一行
                line_y += line_height + 2; // 加2作为行间距
            }
            
            // 计算文本总高度（考虑所有行的高度和间距）
            // 修正计算方式，确保准确性
            int total_text_height = line_y - (render_rect.y - component->scroll_y);
            
            // 限制滚动范围
            if (total_text_height <= render_rect.h) {
                component->scroll_y = 0; // 文本高度小于可见区域，不需要滚动
            } else {
                // 确保滚动不会超出上边界
                if (component->scroll_y < 0) {
                    component->scroll_y = 0;
                }
                // 确保滚动不会超出下边界
                int max_scroll_y = total_text_height - render_rect.h;
                if (component->scroll_y > max_scroll_y) {
                    component->scroll_y = max_scroll_y;
                }
                // 确保即使文本内容变化，最后一行也能完整显示
                if (line_y >= render_rect.y - component->scroll_y + total_text_height && 
                    component->scroll_y < max_scroll_y) {
                    component->scroll_y = max_scroll_y;
                }
            }
        } else {
            // 单行模式，保持原逻辑
            Texture* tex = backend_render_texture(layer->font->default_font, component->text, layer->color);
            if (tex) {
                int text_width, text_height;
                backend_query_texture(tex, NULL, NULL, &text_width, &text_height);
                
                // 计算文本位置
                Rect text_rect = {
                    render_rect.x,
                    render_rect.y + (render_rect.h - text_height / scale) / 2,  // 单行模式下垂直居中
                    text_width / scale,
                    text_height / scale
                };
                
                // 确保文本不会超出边界
                if (text_rect.x + text_rect.w > render_rect.x + render_rect.w) {
                    text_rect.w = render_rect.x + render_rect.w - text_rect.x;
                }
                if (text_rect.y + text_rect.h > render_rect.y + render_rect.h) {
                    text_rect.h = render_rect.y + render_rect.h - text_rect.y;
                }
                
                backend_render_text_copy(tex, NULL, &text_rect);
                backend_render_text_destroy(tex);
            }
        }
    }
    
    // 如果组件可编辑，绘制光标
    if (component->editable && HAS_STATE(layer, LAYER_STATE_FOCUSED)) {
        // 安全检查：确保光标位置在有效范围内
        int text_len = strlen(component->text);
        int safe_cursor_pos = component->cursor_pos;
        if (safe_cursor_pos < 0) safe_cursor_pos = 0;
        if (safe_cursor_pos > text_len) safe_cursor_pos = text_len;
        
        // 简单计算光标位置（实际应用中需要更复杂的计算）
        char* temp_text = (char*)malloc(safe_cursor_pos + 1);
        if (temp_text) {
            strncpy(temp_text, component->text, safe_cursor_pos);
            temp_text[safe_cursor_pos] = '\0';
            
            // 计算光标位置（使用更跨平台的方法）
            if (layer->font && layer->font->default_font) {
                // 查找最后一个换行符的位置，只计算当前行的文本宽度
                char* last_newline = strrchr(temp_text, '\n');
                char* current_line_text = (last_newline) ? last_newline + 1 : temp_text;
                
                // 为了正确显示光标，特别是在新行的第一个字母前
                Texture* cursor_text_texture = backend_render_texture(layer->font->default_font, current_line_text, layer->color);
                
                // 绘制光标
                Rect cursor_rect;
                int text_height = 20; // 默认高度，以防没有文本
                
                if (cursor_text_texture) {
                    int text_width;
                    backend_query_texture(cursor_text_texture, NULL, NULL, &text_width, &text_height);
                    
                    cursor_rect.x = render_rect.x + text_width / scale;
                    
                    // 确保光标始终可见，即使在空行或新行的开头
                    // 当光标在起始位置时，确保其不会隐藏在渲染区域左侧
                    if (cursor_rect.x < render_rect.x) {
                        cursor_rect.x = render_rect.x;
                    }
                    
                    // 多行模式下需要计算光标所在行的Y坐标，单行模式下垂直居中
                    if (component->multiline) {
                        // 计算光标所在行的Y坐标
                        int line_y = render_rect.y;
                        int line_height = text_height / scale;
                        
                        // 计算光标前有多少个换行符
                        int line_count = 0;
                        for (int i = 0; i < component->cursor_pos; i++) {
                            if (component->text[i] == '\n') {
                                line_count++;
                            }
                        }
                        
                        // 根据行数计算Y坐标，并考虑滚动偏移量
                        cursor_rect.y = render_rect.y + line_count * (line_height + 2) - layer->scroll_offset; // +2 是行间距
                    } else {
                        // 单行模式下垂直居中
                        cursor_rect.y = render_rect.y + (render_rect.h - text_height / scale) / 2;
                    }
                    
                    cursor_rect.w = 2;
                    cursor_rect.h = text_height / scale;
                    
                    backend_render_fill_rect(&cursor_rect, component->cursor_color);
                    
                    // 释放临时纹理
                    backend_render_text_destroy(cursor_text_texture);
                } else {
                    // 特殊情况：没有文本或者文本渲染失败，确保光标显示在起始位置
                    cursor_rect.x = render_rect.x;
                    cursor_rect.w = 2;
                    cursor_rect.h = 20; // 使用默认高度
                    
                    // 多行模式下需要计算光标所在行的Y坐标，单行模式下垂直居中
                    if (component->multiline) {
                        // 计算光标前有多少个换行符
                        int line_count = 0;
                        for (int i = 0; i < component->cursor_pos; i++) {
                            if (component->text[i] == '\n') {
                                line_count++;
                            }
                        }
                        
                        // 根据行数计算Y坐标，并考虑滚动偏移量
                        cursor_rect.y = render_rect.y + line_count * 22 - layer->scroll_offset; // 使用默认行高和间距
                    } else {
                        // 单行模式下垂直居中
                        cursor_rect.y = render_rect.y + (render_rect.h - 20) / 2;
                    }
                    
                    backend_render_fill_rect(&cursor_rect, component->cursor_color);
                }
            } else {
                // 没有字体时，使用默认光标位置和大小
                Rect cursor_rect;
                cursor_rect.x = render_rect.x;
                cursor_rect.y = render_rect.y + render_rect.h / 2 - 10;
                cursor_rect.w = 2;
                cursor_rect.h = 20;
                
                backend_render_fill_rect(&cursor_rect, component->cursor_color);
            }
            
            free(temp_text);
        }
    }
    
    // 恢复裁剪区域
    backend_render_set_clip_rect(&prev_clip);
    
    // 如果是多行模式且需要滚动，绘制滚动条
    if (component->multiline) {
        // 计算文本总高度以确定是否需要显示滚动条
        char* text = component->text;
        int line_count = 1;
        int line_height = 20; // 默认行高
        
        // 计算换行符数量
        for (int i = 0; i < strlen(text); i++) {
            if (text[i] == '\n') {
                line_count++;
            }
        }
        
        // 获取实际行高
        Texture* temp_tex = backend_render_texture(layer->font->default_font, "X", layer->color);
        if (temp_tex) {
            int temp_width, temp_height;
            backend_query_texture(temp_tex, NULL, NULL, &temp_width, &temp_height);
            line_height = temp_height / scale;
            backend_render_text_destroy(temp_tex);
        }
        
        int total_text_height = line_count * (line_height + 2); // 2为行间距
        int visible_height = layer->rect.h - 10; // 减去内边距
        
        // 只有当文本总高度大于可见高度时才显示滚动条
        if (total_text_height > visible_height) {
            // 绘制滚动条背景
            Rect scrollbar_bg = {
                layer->rect.x + layer->rect.w - 10, // 右侧留出10像素宽度的滚动条
                layer->rect.y + 5, // 顶部内边距
                5, // 滚动条宽度
                layer->rect.h - 10 // 滚动条高度（减去上下内边距）
            };
            Color scrollbar_bg_color = {200, 200, 200, 255};
            backend_render_fill_rect(&scrollbar_bg, scrollbar_bg_color);
            
            // 计算滚动条滑块位置和大小
            float scroll_percent = (float)component->scroll_y / (total_text_height - visible_height);
            int slider_height = visible_height * visible_height / total_text_height;
            if (slider_height < 20) slider_height = 20; // 滑块最小高度
            
            Rect scrollbar_slider = {
                scrollbar_bg.x,
                scrollbar_bg.y + scroll_percent * (scrollbar_bg.h - slider_height),
                scrollbar_bg.w,
                slider_height
            };
            Color scrollbar_slider_color = {150, 150, 150, 255};
            backend_render_fill_rect(&scrollbar_slider, scrollbar_slider_color);
        }
    }
}



