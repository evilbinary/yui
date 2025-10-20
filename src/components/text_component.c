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
    component->editable = 1;  // 默认设置为可编辑状态
    
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
                // 无论是否为多行模式，都支持换行
                text_component_insert_char(component, '\n');
                // 如果是单行模式，设置为多行模式以显示换行效果
                if (!component->multiline) {
                    component->multiline = true;
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
    
    // 处理滚轮事件
    if (event->state == SDL_MOUSEWHEEL) {
        if (component->multiline) {
            // 计算渲染区域
            Rect render_rect = layer->rect;
            render_rect.x += 5;  // 内边距
            render_rect.y += 5;
            render_rect.w -= 10;
            render_rect.h -= 10;
            
            // 计算文本实际的总行数（包括自动换行）
            char* text = component->text;
            int line_height = 20; // 默认行高
            int max_width = render_rect.w;
            
            // 获取实际行高
            Texture* temp_tex = backend_render_texture(layer->font->default_font, "X", layer->color);
            if (temp_tex) {
                int temp_width, temp_height;
                backend_query_texture(temp_tex, NULL, NULL, &temp_width, &temp_height);
                line_height = temp_height / scale;
                backend_render_text_destroy(temp_tex);
            }
            
            // 模拟渲染计算，得到实际的总行数（包括自动换行）
            int current_pos = 0;
            int actual_line_count = 0;
            
            while (current_pos < strlen(text)) {
                // 查找当前行可以显示的最大文本长度
                int line_end = current_pos;
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
                
                // 增加行数计数
                actual_line_count++;
                
                // 如果是在换行符处结束，直接跳到下一个字符
                if (split_pos < strlen(text) && text[split_pos] == '\n') {
                    current_pos = split_pos + 1;
                } else {
                    current_pos = split_pos;
                }
            }
            
            // 计算实际的文本总高度
            int total_text_height = actual_line_count * (line_height + 2); // 2为行间距
            int visible_height = render_rect.h;
            
            // 只有当文本总高度大于可见高度时才允许滚动
            if (total_text_height > visible_height) {
                // SDL_MOUSEWHEEL事件的处理
                int scroll_delta = ((SDL_MouseWheelEvent*)event)->y;
                component->scroll_y += scroll_delta * -20; // 每次滚动20像素
                
                // 限制滚动范围
                if (component->scroll_y < 0) {
                    component->scroll_y = 0;
                } else if (component->scroll_y > total_text_height - visible_height) {
                    component->scroll_y = total_text_height - visible_height;
                }
            } else {
                component->scroll_y = 0; // 文本高度小于可见区域，不需要滚动
            }
            return;
        }
    }
    
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
        // 绘制文本
        if (component->multiline) {
            // 多行模式下，实现自动换行
            char* text = component->text;
            int current_pos = 0;
            int line_y = render_rect.y - component->scroll_y; // 考虑滚动偏移
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
        // 简单计算光标位置（实际应用中需要更复杂的计算）
        char* temp_text = (char*)malloc(component->cursor_pos + 1);
        if (temp_text) {
            strncpy(temp_text, component->text, component->cursor_pos);
            temp_text[component->cursor_pos] = '\0';
            
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
                        cursor_rect.y = render_rect.y + line_count * (line_height + 2) - component->scroll_y; // +2 是行间距
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
                        cursor_rect.y = render_rect.y + line_count * 22 - component->scroll_y; // 使用默认行高和间距
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