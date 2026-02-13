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
    memset(component, 0, sizeof(TextComponent));
    if (!component) {
        return NULL;
    }
    
    component->layer = layer;
    // 文本内容现在存储在 layer->text 中，不需要初始化 component->text
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
    component->is_selecting = 0;  // 默认不在选择状态
    component->cached_line_height = 0;  // 行高缓存初始化为0（无效）
    component->line_height_valid = 0;  // 标记缓存无效
    
    // 初始化图层的文本字段
    if (!layer->text) {
        layer_set_text(layer, "");  // 初始化为空字符串
    }
    
    // 设置图层的焦点属性，使其可以获得焦点
    layer->focusable = 1;
    
    // 存储组件指针到图层的component字段中
    layer->component = component;
    layer->render = text_component_render;
    layer->handle_key_event = text_component_handle_key_event;
    layer->handle_mouse_event = text_component_handle_mouse_event;
    layer->register_event = text_component_register_event;
    layer->get_property = text_component_get_property;

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
    
      // 解析事件绑定
    cJSON* events = cJSON_GetObjectItem(json_obj, "events");
    if (events) {
        // 解析onChange事件
        if (cJSON_HasObjectItem(events, "onChange")) {
            cJSON* on_change_obj = cJSON_GetObjectItem(events, "onChange");
            if (cJSON_IsString(on_change_obj)) {
                const char* event_name = on_change_obj->valuestring;
                // 将事件名称存储在 user_data 中，稍后由事件系统处理
                component->change_name = strdup(event_name);
                EventHandler handler = find_event_by_name(event_name);
                component->on_change = handler;
            }
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
    
    // 计算文本长度
    size_t text_len = strlen(text);
    
    // 移除最大长度限制，使用动态内存分配
    // 注释掉最大长度检查，因为现在使用 layer_set_text_with_size 进行动态内存分配
    /*
    size_t copy_len = text_len < (size_t)(component->max_length - 1) ? text_len : (size_t)(component->max_length - 1);
    
    // 创建临时字符串以存储截断后的文本
    char* temp_text = malloc(copy_len + 1);
    if (!temp_text) {
        return;
    }
    
    memcpy(temp_text, text, copy_len);
    temp_text[copy_len] = '\0';
    */
    
    // 使用 layer_set_text 设置文本（使用动态内存分配）
    layer_set_text(component->layer, text);
    
    // 更新组件状态
    component->cursor_pos = text_len;
    component->selection_start = -1;
    component->selection_end = -1;
    
    // 使行高缓存失效（因为文本或字体可能改变）
    component->line_height_valid = 0;
    
    // 更新内容高度（只调用一次）
    text_component_update_content_height(component);
    
    // 触发 onChange 事件
    text_component_trigger_on_change(component);
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
    // 移除最大长度限制，使用动态内存分配
    // 注释掉最大长度检查，因为现在使用 layer_set_text_with_size 进行动态内存分配
    /*
    // 确保文本不超过新的最大长度
    if (strlen(component->layer->text) >= max_length) {
        // 截断文本
        char* temp_text = malloc(max_length);
        if (temp_text) {
            strncpy(temp_text, component->layer->text, max_length - 1);
            temp_text[max_length - 1] = '\0';
            layer_set_text(component->layer, temp_text);
            free(temp_text);
            component->cursor_pos = max_length - 1;
        }
    }
    */
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

// 注册事件处理函数
void text_component_register_event(Layer* layer, const char* event_name, const char* event_func_name, EventHandler event_handler){
    if(strcmp(event_name,"change")==0 || strcmp(event_name,"onChange")==0){
        TextComponent* component = (TextComponent*)layer->component;
        component->on_change = event_handler;
        component->change_name = strdup(event_func_name);
    }
}

// 通用属性获取函数
cJSON* text_component_get_property(Layer* layer, const char* property_name) {
    if (!layer || !property_name || !layer->component) {
        return NULL;
    }
    
    TextComponent* component = (TextComponent*)layer->component;
    
    if (strcmp(property_name, "value") == 0 || strcmp(property_name, "text") == 0) {
        // 获取文本内容
        if (layer->text && strlen(layer->text) > 0) {
            return cJSON_CreateString(layer->text);
        }
        return cJSON_CreateNull();
    }
    else if (strcmp(property_name, "placeholder") == 0) {
        // 获取占位符
        if (component->placeholder && strlen(component->placeholder) > 0) {
            return cJSON_CreateString(component->placeholder);
        }
        return cJSON_CreateNull();
    }
    else if (strcmp(property_name, "multiline") == 0) {
        // 是否多行
        return cJSON_CreateBool(component->multiline);
    }
    else if (strcmp(property_name, "editable") == 0) {
        // 是否可编辑
        return cJSON_CreateBool(component->editable);
    }
    else if (strcmp(property_name, "maxLength") == 0) {
        // 最大长度
        return cJSON_CreateNumber(component->max_length);
    }
    
    // 未知的属性，返回 NULL
    return NULL;
}




// 设置 onChange 回调函数
void text_component_set_on_change(TextComponent* component, EventHandler callback, void* user_data) {
    if (!component) {
        return;
    }
    
    component->on_change = callback;
    component->change_name = user_data;
}

// 调用 onChange 回调函数
void text_component_trigger_on_change(TextComponent* component) {
    // 如果没有事件处理器但有事件名称，尝试查找事件处理器
    if(component->on_change == NULL && component->change_name != NULL){
        EventHandler handler = find_event_by_name(component->change_name);
        component->on_change = handler;
    }
    
    // 检查是否有可用的事件处理器
    if (component->on_change) {
        // 调用事件处理器
        component->on_change(component->layer);
    } else if (component->change_name) {
        // 只有在指定了事件名称但找不到处理器时才打印警告
        printf("select_component_trigger_on_change not found onchange event %s\n", component->change_name);
        print_registered_events();
    }
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
    int text_len = strlen(component->layer->text);
    if (end > text_len) end = text_len;
    
    // 只有当start < end时才执行删除操作
    if (start < end) {
        // 计算新文本的长度
        size_t new_len = text_len - (end - start);
        
        // 创建临时缓冲区存储新文本
        char* new_text = malloc(new_len + 1);
        if (!new_text) {
            return;
        }
        
        // 复制start之前的部分
        memcpy(new_text, component->layer->text, start);
        // 复制end之后的部分
        memcpy(new_text + start, component->layer->text + end, text_len - end + 1);
        
        // 设置新文本
        layer_set_text(component->layer, new_text);
        free(new_text);
        
        component->cursor_pos = start;
        
        // 更新内容高度
        text_component_update_content_height(component);
        
        // 触发 onChange 事件
        text_component_trigger_on_change(component);
    }
    
    component->selection_start = -1;
    component->selection_end = -1;
}

// 在光标位置插入字符
static void text_component_insert_char(TextComponent* component, char c) {
    if (!component->editable) {
        return;
    }

    int len = strlen(component->layer->text);

    // 移除最大长度限制，使用动态内存分配
    // 注释掉最大长度检查，因为现在使用 layer_set_text_with_size 进行动态内存分配
    /*
    if (len >= component->max_length - 1) {
        return;
    }
    */

    // 如果有选中的文本，先删除
    if (component->selection_start != -1 && component->selection_end != -1) {
        text_component_delete_selection(component);
        // 更新len，因为删除选择后文本长度可能改变
        len = strlen(component->layer->text);
    }
    
    // 确保cursor_pos在有效范围内
    if (component->cursor_pos < 0) component->cursor_pos = 0;
    if (component->cursor_pos > len) component->cursor_pos = len;

    // 插入字符，使用动态内存分配
    // 移除最大长度限制，使用动态内存分配
    // 注释掉最大长度检查，因为现在使用 layer_set_text_with_size 进行动态内存分配
    /*
    if (component->cursor_pos < component->max_length - 1) {
    */
    
    // 计算新文本长度
    size_t new_len = len + 1;
    
    // 创建临时缓冲区存储新文本
    char* new_text = malloc(new_len + 1);
    if (!new_text) {
        return;
    }
    
    // 复制光标位置之前的部分
    memcpy(new_text, component->layer->text, component->cursor_pos);
    // 插入新字符
    new_text[component->cursor_pos] = c;
    // 复制光标位置之后的部分
    memcpy(new_text + component->cursor_pos + 1, component->layer->text + component->cursor_pos, len - component->cursor_pos + 1);
    
    // 设置新文本
    layer_set_text(component->layer, new_text);
    free(new_text);
    
    component->cursor_pos++;
    
    // 更新内容高度（只调用一次）
    text_component_update_content_height(component);
    
    // 触发 onChange 事件
    text_component_trigger_on_change(component);
    
    // 更新滚动位置，确保光标可见
    text_component_update_scroll_for_cursor(component);
}

// 更新滚动位置以确保光标可见
void text_component_update_scroll_for_cursor(TextComponent* component) {
    if (!component || !component->layer || !component->layer->font || !component->layer->font->default_font) {
        return;
    }
    
    // 只在单行模式下处理水平滚动
    if (component->multiline) {
        return;
    }
    
    // 准备渲染区域（与渲染函数中的逻辑一致）
    Rect render_rect = component->layer->rect;
    int left_padding = 5;
    render_rect.x += left_padding;
    render_rect.y += 5;
    render_rect.w -= (left_padding + 5);
    render_rect.h -= 10;
    
    // 计算整个文本的宽度
    int full_text_width = 0;
    if (strlen(component->layer->text) > 0) {
        Texture* full_tex = backend_render_texture(component->layer->font->default_font, component->layer->text, component->layer->color);
        if (full_tex) {
            int full_width, full_height;
            backend_query_texture(full_tex, NULL, NULL, &full_width, &full_height);
            full_text_width = full_width / scale;
            backend_render_text_destroy(full_tex);
        }
    }
    
    // 获取可视区域宽度
    int visible_width = render_rect.w;
    
    // 如果文本没有超过宽度，不需要滚动
    if (full_text_width <= visible_width) {
        component->scroll_x = 0;
        return;
    }
    
    // 获取光标位置之前的文本
    if (component->cursor_pos <= 0) {
        component->scroll_x = 0;
        return;
    }
    
    char* before_cursor = (char*)malloc(component->cursor_pos + 1);
    if (!before_cursor) {
        return;
    }
    
    strncpy(before_cursor, component->layer->text, component->cursor_pos);
    before_cursor[component->cursor_pos] = '\0';
    
    // 计算光标位置的X坐标
    int cursor_x = render_rect.x;
    if (component->cursor_pos > 0) {
        Texture* text_tex = backend_render_texture(component->layer->font->default_font, before_cursor, component->layer->color);
        if (text_tex) {
            int text_width, text_height;
            backend_query_texture(text_tex, NULL, NULL, &text_width, &text_height);
            cursor_x = render_rect.x + text_width / scale;
            backend_render_text_destroy(text_tex);
        }
    }
    
    free(before_cursor);
    
    // 确保光标在可视区域内
    int cursor_pixel_x = cursor_x - render_rect.x + component->scroll_x;
    
    // 如果光标在可视区域左侧，向左滚动
    if (cursor_pixel_x < component->scroll_x) {
        component->scroll_x = cursor_pixel_x;
    }
    // 如果光标在可视区域右侧，向右滚动
    else if (cursor_pixel_x > component->scroll_x + visible_width) {
        component->scroll_x = cursor_pixel_x - visible_width;
    }
    
    // 限制滚动范围
    if (component->scroll_x < 0) {
        component->scroll_x = 0;
    }
    if (component->scroll_x > full_text_width - visible_width) {
        component->scroll_x = full_text_width - visible_width;
    }
}

// 计算文本内容总高度
int text_component_calculate_content_height(TextComponent* component) {
    if (!component || !component->layer || !component->layer->text) {
        return 0;
    }
    
    // 如果不是多行模式，内容高度就是可见区域高度
    if (!component->multiline) {
        return component->layer->rect.h;
    }
    
    // 多行模式需要计算实际行数
    char* text = component->layer->text;
    int text_len = strlen(text);
    int current_pos = 0;
    int line_count = 0;
    
    // 性能优化：使用缓存的行高，避免每帧创建纹理
    int line_height = component->line_height;
    if (!component->line_height_valid) {
        // 只在缓存无效时计算行高
        if (component->layer->font && component->layer->font->default_font) {
            Texture* temp_tex = backend_render_texture(component->layer->font->default_font, "X", component->layer->color);
            if (temp_tex) {
                int temp_width, temp_height;
                backend_query_texture(temp_tex, NULL, NULL, &temp_width, &temp_height);
                line_height = temp_height / scale;
                backend_render_text_destroy(temp_tex);
            }
        }
        // 缓存行高
        component->cached_line_height = line_height;
        component->line_height_valid = 1;
    } else {
        line_height = component->cached_line_height;
    }
    
    // 计算文本内容区域的宽度（减去内边距和行号区域）
    // 与渲染函数中的计算保持一致
    int left_padding = 5;
    if (component->show_line_numbers && component->multiline) {
        left_padding += component->line_number_width;
    }
    int max_width = component->layer->rect.w - (left_padding + 5); // 左内边距 + 右内边距
    
    // 如果没有文本，返回一行的高度
    if (text_len == 0) {
        return line_height + 2;
    }
    
    // 遍历文本，计算视觉行数
    while (current_pos < text_len) {
        // 查找当前行的结束位置
        int line_end = current_pos;
        while (line_end < text_len && text[line_end] != '\n') {
            line_end++;
        }
        
        // 计算这段文本的宽度
        int current_width = 0;
        char* temp_line = (char*)malloc(line_end - current_pos + 1);
        if (temp_line) {
            strncpy(temp_line, text + current_pos, line_end - current_pos);
            temp_line[line_end - current_pos] = '\0';
            
            Texture* line_tex = backend_render_texture(component->layer->font->default_font, temp_line, component->layer->color);
            if (line_tex) {
                int line_width, line_height_ignore;
                backend_query_texture(line_tex, NULL, NULL, &line_width, &line_height_ignore);
                current_width = line_width / scale;
                backend_render_text_destroy(line_tex);
            }
            
            free(temp_line);
        }
        
        // 如果文本超过宽度限制，需要计算需要多少视觉行
        if (current_width <= max_width) {
            // 不需要换行，只有一行
            line_count++;
        } else {
            // 需要自动换行，计算需要多少行
            int needed_lines = 0;
            int segment_start = current_pos;
            
            while (segment_start < line_end) {
                int segment_end = segment_start;
                
                // 找到最大的不超过宽度的位置
                int test_width = 0;
                while (segment_end < line_end) {
                    // 计算当前段的宽度
                    char* test_segment = (char*)malloc(segment_end - segment_start + 1);
                    if (test_segment) {
                        strncpy(test_segment, text + segment_start, segment_end - segment_start);
                        test_segment[segment_end - segment_start] = '\0';
                        
                        Texture* test_tex = backend_render_texture(component->layer->font->default_font, test_segment, component->layer->color);
                        if (test_tex) {
                            int test_w, test_h;
                            backend_query_texture(test_tex, NULL, NULL, &test_w, &test_h);
                            test_width = test_w / scale;
                            backend_render_text_destroy(test_tex);
                        }
                        
                        free(test_segment);
                        
                        // 如果超过宽度，回退到上一个字符
                        if (test_width > max_width && segment_end > segment_start) {
                            segment_end--;
                            break;
                        }
                    }
                    
                    segment_end++;
                }
                
                // 如果找到了一个有效的段
                if (segment_end > segment_start) {
                    needed_lines++;
                    segment_start = segment_end;
                } else {
                    // 单个字符就超过了宽度，至少需要一行
                    needed_lines++;
                    segment_start++;
                }
            }
            
            line_count += needed_lines; // 添加计算出的行数
        }
        
        // 移动到下一行
        if (line_end < text_len && text[line_end] == '\n') {
            current_pos = line_end + 1;
        } else {
            current_pos = line_end;
        }
    }
    
    // 返回总高度（行高 + 行间距）
    // 确保至少有一行的高度
    if (line_count == 0) {
        return line_height + 2;
    }
    return line_count * (line_height + 2);
}



// 删除光标前的字符
static void text_component_delete_prev_char(TextComponent* component) {
    if (!component->editable) {
        return;
    }
    
    // 如果有选中的文本，先删除（只要选区存在就删除）
    if (component->selection_start != -1 && component->selection_end != -1) {
        // 确保start和end不相等，或者都指向有效位置
        int start = component->selection_start;
        int end = component->selection_end;
        if (start > end) {
            int temp = start;
            start = end;
            end = temp;
        }
        
        // 如果选区有效（start != end），删除选中内容
        if (start != end) {
            text_component_delete_selection(component);
            return;
        } else {
            // 选区无效，清除选区标记
            component->selection_start = -1;
            component->selection_end = -1;
        }
    }
    
    // 没有选区或选区无效，删除光标前的字符
    if (component->cursor_pos <= 0) {
        return;
    }
    
    int len = strlen(component->layer->text);
    
    // 确保不会越界
    if (component->cursor_pos > len) {
        component->cursor_pos = len;
    }
    
    if (component->cursor_pos > 0) {
        // 计算新文本长度
        size_t new_len = len - 1;
        
        // 创建临时缓冲区存储新文本
        char* new_text = malloc(new_len + 1);
        if (!new_text) {
            return;
        }
        
        // 复制光标位置之前的部分（不包括要删除的字符）
        memcpy(new_text, component->layer->text, component->cursor_pos - 1);
        // 复制光标位置之后的部分
        memcpy(new_text + component->cursor_pos - 1, component->layer->text + component->cursor_pos, len - component->cursor_pos + 1);
        
        // 设置新文本
        layer_set_text(component->layer, new_text);
        free(new_text);
        
        component->cursor_pos--;
        
        // 触发 onChange 事件
        text_component_trigger_on_change(component);
        
        // 更新滚动位置，确保光标可见
        text_component_update_scroll_for_cursor(component);
    }
}

// 删除光标后的字符
static void text_component_delete_next_char(TextComponent* component) {
    if (!component->editable) {
        return;
    }
    
    // 如果有选中的文本，先删除（只要选区存在就删除）
    if (component->selection_start != -1 && component->selection_end != -1) {
        // 确保start和end不相等，或者都指向有效位置
        int start = component->selection_start;
        int end = component->selection_end;
        if (start > end) {
            int temp = start;
            start = end;
            end = temp;
        }
        
        // 如果选区有效（start != end），删除选中内容
        if (start != end) {
            text_component_delete_selection(component);
            return;
        } else {
            // 选区无效，清除选区标记
            component->selection_start = -1;
            component->selection_end = -1;
        }
    }
    
    int len = strlen(component->layer->text);
    
    // 确保cursor_pos在有效范围内
    if (component->cursor_pos < 0) component->cursor_pos = 0;
    if (component->cursor_pos >= len) {
        return; // 已经在文本末尾，无需删除
    }
    
    // 计算新文本长度
    size_t new_len = len - 1;
    
    // 创建临时缓冲区存储新文本
    char* new_text = malloc(new_len + 1);
    if (!new_text) {
        return;
    }
    
    // 复制光标位置之前的部分
    memcpy(new_text, component->layer->text, component->cursor_pos);
    // 复制光标位置之后的部分（跳过要删除的字符）
    memcpy(new_text + component->cursor_pos, component->layer->text + component->cursor_pos + 1, len - component->cursor_pos);
    
    // 设置新文本
    layer_set_text(component->layer, new_text);
    free(new_text);
    
    // 更新内容高度
    text_component_update_content_height(component);
    
    // 触发 onChange 事件
    text_component_trigger_on_change(component);
}

// 在光标位置插入文本
static void text_component_insert_text(TextComponent* component, const char* text) {
    if (!component || !text || !component->editable) {
        return;
    }
    
    int text_len = strlen(text);
    if (text_len == 0) {
        return; // 空文本，无需插入
    }
    
    int current_len = strlen(component->layer->text);
    
    // 如果有选中的文本，先删除
    if (component->selection_start != -1 && component->selection_end != -1) {
        text_component_delete_selection(component);
        current_len = strlen(component->layer->text);
    }
    
    // 确保cursor_pos在有效范围内
    if (component->cursor_pos < 0) component->cursor_pos = 0;
    if (component->cursor_pos > current_len) component->cursor_pos = current_len;
    
    // 检查是否包含换行符，如果包含则设置为多行模式
    int has_newlines = 0;
    for (int i = 0; i < text_len; i++) {
        if (text[i] == '\n') {
            has_newlines = 1;
            break;
        }
    }
    if (has_newlines && !component->multiline) {
        component->multiline = 1;
    }
    
    // 移除最大长度限制，使用动态内存分配
    // 注释掉最大长度检查，因为现在使用 layer_set_text_with_size 进行动态内存分配
    /*
    if (current_len + text_len >= component->max_length) {
        // 截断文本以适应最大长度
        text_len = component->max_length - current_len - 1;
        if (text_len <= 0) {
            return; // 没有足够空间
        }
    }
    */
    
    // 计算新文本长度
    size_t new_len = current_len + text_len;
    
    // 创建临时缓冲区存储新文本
    char* new_text = malloc(new_len + 1);
    if (!new_text) {
        return;
    }
    
    // 复制光标位置之前的部分
    memcpy(new_text, component->layer->text, component->cursor_pos);
    // 插入新文本
    memcpy(new_text + component->cursor_pos, text, text_len);
    // 复制光标位置之后的部分
    memcpy(new_text + component->cursor_pos + text_len, 
           component->layer->text + component->cursor_pos, 
           current_len - component->cursor_pos + 1);
    
    // 设置新文本
    layer_set_text(component->layer, new_text);
    free(new_text);
    
    component->cursor_pos += text_len;
    
    // 更新内容高度（只调用一次）
    text_component_update_content_height(component);
    
    // 触发 onChange 事件
    text_component_trigger_on_change(component);
    
    // 清除选择
    component->selection_start = -1;
    component->selection_end = -1;
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
                    // 如果按住Shift键，需要先检查是否已有选择
                    if (event->data.key.mod & KMOD_SHIFT) {
                        // 如果没有选择，开始新选择
                        if (component->selection_start == -1) {
                            component->selection_start = component->cursor_pos;
                        }
                        // 移动光标
                        component->cursor_pos--;
                        // 更新选择结束位置
                        component->selection_end = component->cursor_pos;
                    } else {
                        // 如果没有按Shift，只是移动光标并清除选择
                        component->cursor_pos--;
                        component->selection_start = -1;
                        component->selection_end = -1;
                    }
                    
                    // 更新滚动位置，确保光标可见
                    text_component_update_scroll_for_cursor(component);
                }
                break;
            case SDLK_RIGHT:
                if (component->cursor_pos < strlen(component->layer->text)) {
                    // 如果按住Shift键，需要先检查是否已有选择
                    if (event->data.key.mod & KMOD_SHIFT) {
                        // 如果没有选择，开始新选择
                        if (component->selection_start == -1) {
                            component->selection_start = component->cursor_pos;
                        }
                        // 移动光标
                        component->cursor_pos++;
                        // 更新选择结束位置
                        component->selection_end = component->cursor_pos;
                    } else {
                        // 如果没有按Shift，只是移动光标并清除选择
                        component->cursor_pos++;
                        component->selection_start = -1;
                        component->selection_end = -1;
                    }
                    
                    // 更新滚动位置，确保光标可见
                    text_component_update_scroll_for_cursor(component);
                }
                break;
            case SDLK_UP:
                if (component->multiline) {
                    // 先保存当前光标位置作为选择起始点
                    int old_cursor_pos = component->cursor_pos;
                    
                    // 简单的向上移动光标实现
                    int pos = component->cursor_pos;
                    while (pos > 0 && component->layer->text[pos] != '\n') {
                        pos--;
                    }
                    if (pos > 0) {
                        pos--;
                        // 尽量保持在同一列
                        int col = 0;
                        int temp = component->cursor_pos;
                        while (temp > 0 && component->layer->text[temp - 1] != '\n') {
                            col++;
                            temp--;
                        }
                        
                        temp = pos;
                        while (col > 0 && temp < strlen(component->layer->text) && component->layer->text[temp] != '\n') {
                            temp++;
                            col--;
                        }
                        component->cursor_pos = temp;
                    } else {
                        component->cursor_pos = 0;
                    }
                    
                    // 处理选择
                    if (event->data.key.mod & KMOD_SHIFT) {
                        // 如果没有选择，开始新选择
                        if (component->selection_start == -1) {
                            component->selection_start = old_cursor_pos;
                        }
                        // 更新选择结束位置
                        component->selection_end = component->cursor_pos;
                    } else {
                        // 清除选择
                        component->selection_start = -1;
                        component->selection_end = -1;
                    }
                }
                break;
            case SDLK_DOWN:
                if (component->multiline) {
                    // 先保存当前光标位置作为选择起始点
                    int old_cursor_pos = component->cursor_pos;
                    
                    // 简单的向下移动光标实现
                    int pos = component->cursor_pos;
                    while (pos < strlen(component->layer->text) && component->layer->text[pos] != '\n') {
                        pos++;
                    }
                    if (pos < strlen(component->layer->text)) {
                        pos++;
                        // 尽量保持在同一列
                        int col = 0;
                        int temp = component->cursor_pos;
                        while (temp > 0 && component->layer->text[temp - 1] != '\n') {
                            col++;
                            temp--;
                        }
                        
                        temp = pos;
                        while (col > 0 && temp < strlen(component->layer->text) && component->layer->text[temp] != '\n') {
                            temp++;
                            col--;
                        }
                        component->cursor_pos = temp;
                    } else {
                        component->cursor_pos = strlen(component->layer->text);
                    }
                    
                    // 处理选择
                    if (event->data.key.mod & KMOD_SHIFT) {
                        // 如果没有选择，开始新选择
                        if (component->selection_start == -1) {
                            component->selection_start = old_cursor_pos;
                        }
                        // 更新选择结束位置
                        component->selection_end = component->cursor_pos;
                    } else {
                        // 清除选择
                        component->selection_start = -1;
                        component->selection_end = -1;
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
                    component->selection_end = strlen(component->layer->text);
                    component->cursor_pos = strlen(component->layer->text);
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
                            strncpy(selected_text, component->layer->text + start, len);
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
                // Ctrl+V 粘贴
                if (event->data.key.mod & KMOD_CTRL) {
                    // 从剪贴板读取文本
                    char* clipboard_text = backend_get_clipboard_text();
                    if (clipboard_text) {
                        text_component_insert_text(component, clipboard_text);
                        free(clipboard_text);
                    }
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
                            strncpy(selected_text, component->layer->text + start, len);
                            selected_text[len] = '\0';
                            printf("Cut: '%s'\n", selected_text);
                            free(selected_text);
                            
                            // 删除选中文本
                            text_component_delete_selection(component);
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
    Point pt = {event->x, event->y};
    
    // 鼠标左键按下 - 设置光标位置并开始选择
    if (event->state == SDL_PRESSED && event->button == SDL_BUTTON_LEFT) {
        // 检查组件是否可获得焦点
        if (layer->focusable) {
            // 设置焦点状态
            SET_STATE(layer, LAYER_STATE_FOCUSED);
        }
        if (point_in_rect(pt, layer->rect)) {
            // 计算点击位置对应的文本位置
            int click_pos = text_component_get_position_from_point(component, pt, layer);
            component->cursor_pos = click_pos;
            
            // 开始新选择
            component->selection_start = click_pos;
            component->selection_end = click_pos;
            component->is_selecting = 1;
            
            // 更新滚动位置，确保光标可见
            text_component_update_scroll_for_cursor(component);
        } else {
            // 点击文本区域外，清除选择并移除焦点
            component->selection_start = -1;
            component->selection_end = -1;
            component->is_selecting = 0;
            // 移除焦点状态
            CLEAR_STATE(layer, LAYER_STATE_FOCUSED);
        }
    }
    // 鼠标拖动 - 更新选择范围
    else if (event->state == SDL_MOUSEMOTION && (event->button == SDL_BUTTON_LEFT) && component->is_selecting) {
        int drag_pos;
        
        if (point_in_rect(pt, layer->rect)) {
            // 在文本区域内，使用正常计算
            drag_pos = text_component_get_position_from_point(component, pt, layer);
        } else {
            // 在文本区域外，根据位置估算
            if (pt.x < layer->rect.x) {
                // 拖到左侧，选择到当前光标所在行的行首
                drag_pos = get_line_start(component, component->cursor_pos);
            } else if (pt.x > layer->rect.x + layer->rect.w) {
                // 拖到右侧，选择到当前光标所在行的行尾
                drag_pos = get_line_end(component, component->cursor_pos);
            } else if (pt.y < layer->rect.y) {
                // 拖到上方，选择到文本开头
                drag_pos = 0;
            } else if (pt.y > layer->rect.y + layer->rect.h) {
                // 拖到下方，选择到文本末尾
                drag_pos = strlen(component->layer->text);
            } else {
                // 默认情况，保持当前位置
                drag_pos = component->cursor_pos;
            }
        }
        
        // 更新选择范围和光标位置
        component->selection_end = drag_pos;
        component->cursor_pos = drag_pos;
        
        // 更新滚动位置，确保光标可见
        text_component_update_scroll_for_cursor(component);
    }
    // 鼠标释放 - 结束选择
    else if (event->state == SDL_RELEASED && event->button == SDL_BUTTON_LEFT) {
        component->is_selecting = 0;
    }
}









// 辅助函数：获取指定位置所在行的起始位置
int get_line_start(TextComponent* component, int pos) {
    if (!component || pos < 0) {
        return 0;
    }
    
    // 限制pos在文本范围内
    if (pos > strlen(component->layer->text)) {
        pos = strlen(component->layer->text);
    }
    
    // 从pos向前查找换行符
    int start = pos;
    while (start > 0 && component->layer->text[start - 1] != '\n') {
        start--;
    }
    
    return start;
}

// 辅助函数：获取指定位置所在行的结束位置
int get_line_end(TextComponent* component, int pos) {
    if (!component || pos < 0) {
        return 0;
    }
    
    // 限制pos在文本范围内
    if (pos > strlen(component->layer->text)) {
        pos = strlen(component->layer->text);
    }
    
    // 从pos向后查找换行符或文本末尾
    int end = pos;
    while (end < strlen(component->layer->text) && component->layer->text[end] != '\n') {
        end++;
    }
    
    return end;
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
    
    // 边界检查
    if (pt.y < render_rect.y) return 0;
    if (pt.y > render_rect.y + render_rect.h) return strlen(component->layer->text);
    if (pt.x < render_rect.x) return 0;
    
    // 如果没有文本，返回0
    if (strlen(component->layer->text) == 0) {
        return 0;
    }
    
    // 获取行高
    int line_height = 20;
    
    if (layer->font && layer->font->default_font) {
        Texture* temp_tex = backend_render_texture(layer->font->default_font, "X", layer->color);
        if (temp_tex) {
            int temp_width, temp_height;
            backend_query_texture(temp_tex, NULL, NULL, &temp_width, &temp_height);
            line_height = temp_height / scale;
            backend_render_text_destroy(temp_tex);
        }
    }
    
    if (component->multiline) {
        // 多行模式处理 - 使用与文本渲染完全相同的算法
        char* text = component->layer->text;
        int text_len = strlen(text);
        int current_pos = 0;
        int visual_line = 0;  // 视觉上的行号（包括自动换行）
        int max_width = render_rect.w;
        
        // 计算点击位置对应的视觉行号
        int click_y = pt.y - render_rect.y;
        int target_visual_line = click_y / (line_height + 2);
        
        // 考虑滚动偏移，计算实际的目标视觉行
        int actual_scroll_line = layer->scroll_offset / (line_height + 2);
        target_visual_line += actual_scroll_line;
        if (target_visual_line < 0) target_visual_line = 0;
        
        // 遍历文本，模拟渲染过程（与文本渲染使用相同的算法）
        while (current_pos < text_len) {
            // 查找当前行可以显示的最大文本长度
            int line_end = current_pos;
            
            // 尝试找到合适的换行点（找到\n或文本末尾）
            while (line_end < text_len) {
                // 遇到换行符时立即换行
                if (text[line_end] == '\n') {
                    break;
                }
                line_end++;
            }
            
            // 计算整行文本的宽度
            int current_width = 0;
            char* temp_line = (char*)malloc(line_end - current_pos + 1);
            if (temp_line) {
                strncpy(temp_line, text + current_pos, line_end - current_pos);
                temp_line[line_end - current_pos] = '\0';
                
                Texture* line_tex = backend_render_texture(layer->font->default_font, temp_line, layer->color);
                if (line_tex) {
                    int line_width, line_height_ignore;
                    backend_query_texture(line_tex, NULL, NULL, &line_width, &line_height_ignore);
                    current_width = line_width / scale;
                    backend_render_text_destroy(line_tex);
                }
                
                free(temp_line);
            }
            
            // 确定当前视觉行的结束位置
            int split_pos = current_pos;
            
            // 如果文本没有超过宽度限制
            if (current_width <= max_width) {
                // 使用整行（到\n或文本末尾）
                split_pos = line_end;
            }
            // 如果文本超过宽度限制，需要硬换行
            else {
                // 找到最大的不超过宽度的位置
                split_pos = current_pos;
                while (split_pos < line_end) {
                    char* test_line = (char*)malloc(split_pos - current_pos + 1);
                    if (test_line) {
                        strncpy(test_line, text + current_pos, split_pos - current_pos);
                        test_line[split_pos - current_pos] = '\0';
                        
                        Texture* test_tex = backend_render_texture(layer->font->default_font, test_line, layer->color);
                        if (test_tex) {
                            int test_width, test_height;
                            backend_query_texture(test_tex, NULL, NULL, &test_width, &test_height);
                            if (test_width / scale > max_width) {
                                backend_render_text_destroy(test_tex);
                                free(test_line);
                                break;
                            }
                            backend_render_text_destroy(test_tex);
                        }
                        
                        free(test_line);
                    }
                    split_pos++;
                } 
                
                if (split_pos > current_pos) {
                    split_pos--;
                }
            }
            
            // 确保split_pos >= current_pos，防止负长度
            if (split_pos < current_pos) {
                split_pos = current_pos;
            }
            
            // 检查是否是目标视觉行
            if (visual_line == target_visual_line) {
                // 这就是目标行，计算点击位置
                int click_x = pt.x - render_rect.x;
                int best_pos = current_pos;
                int min_distance = abs(click_x);
                
                // 测试这一段内每个位置
                for (int pos = current_pos; pos <= split_pos; pos++) {
                    int len = pos - current_pos;
                    char* temp_text = (char*)malloc(len + 1);
                    if (temp_text) {
                        strncpy(temp_text, text + current_pos, len);
                        temp_text[len] = '\0';
                        
                        int actual_width = 0;
                        if (len > 0) {
                            Texture* temp_tex = backend_render_texture(layer->font->default_font, temp_text, layer->color);
                            if (temp_tex) {
                                int temp_w, temp_h;
                                backend_query_texture(temp_tex, NULL, NULL, &temp_w, &temp_h);
                                actual_width = temp_w / scale;
                                backend_render_text_destroy(temp_tex);
                            }
                        }
                        
                        int distance = abs(click_x - actual_width);
                        if (distance < min_distance) {
                            min_distance = distance;
                            best_pos = pos;
                        }
                        
                        free(temp_text);
                    }
                }
                
                return best_pos;
            }
            
            // 移动到下一视觉行
            visual_line++;
            
            // 如果是在换行符处结束，直接跳到下一个字符
            if (split_pos < text_len && text[split_pos] == '\n') {
                current_pos = split_pos + 1;
            } else {
                current_pos = split_pos;
            }
        }
        
        // 如果超出范围，返回文本末尾
        return text_len;
    } else {
        // 单行模式处理 - 使用实际渲染宽度
        int click_x = pt.x - render_rect.x;
        int text_len = strlen(component->layer->text);
        int best_pos = 0;
        int min_distance = abs(click_x);
        
        // 测试每个位置
        for (int pos = 0; pos <= text_len; pos++) {
            char* temp_text = (char*)malloc(pos + 1);
            if (temp_text) {
                strncpy(temp_text, component->layer->text, pos);
                temp_text[pos] = '\0';
                
                // 渲染这段文本以获取实际宽度
                int actual_width = 0;
                if (pos > 0) {
                    Texture* temp_tex = backend_render_texture(layer->font->default_font, temp_text, layer->color);
                    if (temp_tex) {
                        int temp_w, temp_h;
                        backend_query_texture(temp_tex, NULL, NULL, &temp_w, &temp_h);
                        actual_width = temp_w / scale;
                        backend_render_text_destroy(temp_tex);
                    }
                }
                
                // 计算到点击位置的距离
                int distance = abs(click_x - actual_width);
                if (distance < min_distance) {
                    min_distance = distance;
                    best_pos = pos;
                }
                
                free(temp_text);
            }
        }
        
        return best_pos;
    }
}

// 渲染文本组件
void text_component_render(Layer* layer) {
    if (!layer || !layer->component) {
        return;
    }
    TextComponent* component = (TextComponent*)layer->component;
    text_component_update_content_height(component);
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
        
        // 获取行高
        int line_height = component->line_height;
        Texture* temp_tex = backend_render_texture(layer->font->default_font, "X", component->line_number_color);
        if (temp_tex) {
            int temp_width, temp_height;
            backend_query_texture(temp_tex, NULL, NULL, &temp_width, &temp_height);
            line_height = temp_height / scale;
            backend_render_text_destroy(temp_tex);
        }
        
        // 遍历文本，使用与渲染相同的算法，为每个逻辑行渲染行号
        char* text = component->layer->text;
        int text_len = strlen(text);
        int current_pos = 0;
        int visual_line = 0;  // 视觉行号
        int logical_line = 1;  // 逻辑行号（从1开始）
        // 计算文本内容区域的宽度（减去内边距和行号区域）
        int max_width = layer->rect.w - (component->show_line_numbers ? component->line_number_width : 0) - 15;
        
        while (current_pos < text_len) {
            // 记录当前逻辑行的起始视觉行
            int logical_line_visual_start = visual_line;
            
            // 查找当前逻辑行的结束位置
            int line_end = current_pos;
            while (line_end < text_len) {
                if (text[line_end] == '\n') {
                    break;
                }
                line_end++;
            }
            
            // 处理这个逻辑行的所有视觉行（包括自动换行产生的）
            int logical_line_pos = current_pos;
            if (logical_line_pos >= line_end) {
                // 空行，也占一个视觉行
                visual_line++;
            } else {
                while (logical_line_pos < line_end) {
                    // 计算剩余文本的宽度
                    int current_width = 0;
                    char* temp_line = (char*)malloc(line_end - logical_line_pos + 1);
                    if (temp_line) {
                        strncpy(temp_line, text + logical_line_pos, line_end - logical_line_pos);
                        temp_line[line_end - logical_line_pos] = '\0';
                        
                        Texture* line_tex = backend_render_texture(layer->font->default_font, temp_line, layer->color);
                        if (line_tex) {
                            int line_width, line_height_ignore;
                            backend_query_texture(line_tex, NULL, NULL, &line_width, &line_height_ignore);
                            current_width = line_width / scale;
                            backend_render_text_destroy(line_tex);
                        }
                        
                        free(temp_line);
                    }
                    
                    // 确定当前视觉行的结束位置
                    int split_pos = logical_line_pos;
                    if (current_width <= max_width) {
                        split_pos = line_end;
                    } else {
                        split_pos = logical_line_pos;
                        while (split_pos < line_end) {
                            char* test_line = (char*)malloc(split_pos - logical_line_pos + 1);
                            if (test_line) {
                                strncpy(test_line, text + logical_line_pos, split_pos - logical_line_pos);
                                test_line[split_pos - logical_line_pos] = '\0';
                                
                                Texture* test_tex = backend_render_texture(layer->font->default_font, test_line, layer->color);
                                if (test_tex) {
                                    int test_width, test_height;
                                    backend_query_texture(test_tex, NULL, NULL, &test_width, &test_height);
                                    if (test_width / scale > max_width) {
                                        backend_render_text_destroy(test_tex);
                                        free(test_line);
                                        break;
                                    }
                                    backend_render_text_destroy(test_tex);
                                }
                                
                                free(test_line);
                            }
                            split_pos++;
                        } 
                        
                        if (split_pos > logical_line_pos) {
                            split_pos--;
                        }
                    }
                    
                    if (split_pos < logical_line_pos) {
                        split_pos = logical_line_pos;
                    }
                    
                    logical_line_pos = split_pos;
                    visual_line++;
                }
            }
            
            // 为这个逻辑行渲染行号（只在第一个视觉行的位置）
            int line_y = line_number_bg.y + logical_line_visual_start * (line_height + 2) - layer->scroll_offset;
            
            // 检查是否在可见范围内
            if (line_y + line_height > line_number_bg.y && line_y < line_number_bg.y + line_number_bg.h) {
                char line_num_str[16];
                snprintf(line_num_str, sizeof(line_num_str), "%d", logical_line);
                
                Texture* line_num_tex = backend_render_texture(layer->font->default_font, line_num_str, component->line_number_color);
                if (line_num_tex) {
                    int num_width, num_height;
                    backend_query_texture(line_num_tex, NULL, NULL, &num_width, &num_height);
                    
                    Rect line_num_rect = {
                        line_number_bg.x + line_number_bg.w - num_width / scale - 5,
                        line_y,
                        num_width / scale,
                        num_height / scale
                    };
                    
                    backend_render_text_copy(line_num_tex, NULL, &line_num_rect);
                    backend_render_text_destroy(line_num_tex);
                }
            }
            
            // 移动到下一个逻辑行
            current_pos = line_end;
            if (current_pos < text_len && text[current_pos] == '\n') {
                current_pos++;
            }
            logical_line++;
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
    if (strlen(component->layer->text) == 0 && strlen(component->placeholder) > 0) {
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
                // 多行模式：需要考虑自动换行，使用与文本渲染完全相同的逻辑
                char* text = component->layer->text;
                int text_len = strlen(text);
                int line_y = render_rect.y - layer->scroll_offset;
                int current_pos = 0;
                int max_width = render_rect.w;
                
                // 遍历文本，模拟渲染过程（与文本渲染使用相同的算法）
                while (current_pos < text_len) {
                    // 查找当前行可以显示的最大文本长度
                    int line_end = current_pos;
                    
                    // 尝试找到合适的换行点（找到\n或文本末尾）
                    while (line_end < text_len) {
                        // 遇到换行符时立即换行
                        if (text[line_end] == '\n') {
                            break;
                        }
                        line_end++;
                    }
                    
                    // 计算整行文本的宽度
                    int current_width = 0;
                    char* temp_line = (char*)malloc(line_end - current_pos + 1);
                    if (temp_line) {
                        strncpy(temp_line, text + current_pos, line_end - current_pos);
                        temp_line[line_end - current_pos] = '\0';
                        
                        Texture* line_tex = backend_render_texture(layer->font->default_font, temp_line, layer->color);
                        if (line_tex) {
                            int line_width, line_height_ignore;
                            backend_query_texture(line_tex, NULL, NULL, &line_width, &line_height_ignore);
                            current_width = line_width / scale;
                            backend_render_text_destroy(line_tex);
                        }
                        
                        free(temp_line);
                    }
                    
                    // 确定当前视觉行的结束位置
                    int split_pos = current_pos;
                    
                    // 如果文本没有超过宽度限制
                    if (current_width <= max_width) {
                        // 使用整行（到\n或文本末尾）
                        split_pos = line_end;
                    }
                    // 如果文本超过宽度限制，需要硬换行
                    else {
                        // 找到最大的不超过宽度的位置
                        split_pos = current_pos;
                        while (split_pos < line_end) {
                            char* test_line = (char*)malloc(split_pos - current_pos + 1);
                            if (test_line) {
                                strncpy(test_line, text + current_pos, split_pos - current_pos);
                                test_line[split_pos - current_pos] = '\0';
                                
                                Texture* test_tex = backend_render_texture(layer->font->default_font, test_line, layer->color);
                                if (test_tex) {
                                    int test_width, test_height;
                                    backend_query_texture(test_tex, NULL, NULL, &test_width, &test_height);
                                    if (test_width / scale > max_width) {
                                        backend_render_text_destroy(test_tex);
                                        free(test_line);
                                        break;
                                    }
                                    backend_render_text_destroy(test_tex);
                                }
                                
                                free(test_line);
                            }
                            split_pos++;
                        } 
                        
                        if (split_pos > current_pos) {
                            split_pos--;
                        }
                    }
                    
                    // 确保split_pos >= current_pos，防止负长度
                    if (split_pos < current_pos) {
                        split_pos = current_pos;
                    }
                    
                    // 处理这一视觉行的选择背景
                    int visual_line_start = current_pos;
                    int visual_line_end = split_pos;
                    
                    // 检查选择区域是否与当前视觉行相交
                    if (start < visual_line_end && end > visual_line_start) {
                        // 计算当前视觉行内的选择起始和结束位置
                        int sel_start_in_line = (start > visual_line_start) ? start : visual_line_start;
                        int sel_end_in_line = (end < visual_line_end) ? end : visual_line_end;
                        
                        // 计算选择起始位置的X坐标
                        int sel_start_x = render_rect.x;
                        if (sel_start_in_line > visual_line_start) {
                            int before_len = sel_start_in_line - visual_line_start;
                            char* before_sel = (char*)malloc(before_len + 1);
                            if (before_sel) {
                                strncpy(before_sel, text + visual_line_start, before_len);
                                before_sel[before_len] = '\0';
                                
                                Texture* before_tex = backend_render_texture(layer->font->default_font, before_sel, layer->color);
                                if (before_tex) {
                                    int before_width, before_height;
                                    backend_query_texture(before_tex, NULL, NULL, &before_width, &before_height);
                                    sel_start_x = render_rect.x + before_width / scale;
                                    backend_render_text_destroy(before_tex);
                                }
                                free(before_sel);
                            }
                        }
                        
                        // 计算选择区域的宽度
                        int sel_width = 0;
                        int sel_len = sel_end_in_line - sel_start_in_line;
                        if (sel_len > 0) {
                            char* sel_text = (char*)malloc(sel_len + 1);
                            if (sel_text) {
                                strncpy(sel_text, text + sel_start_in_line, sel_len);
                                sel_text[sel_len] = '\0';
                                
                                Texture* sel_tex = backend_render_texture(layer->font->default_font, sel_text, layer->color);
                                if (sel_tex) {
                                    int sel_tex_width, sel_tex_height;
                                    backend_query_texture(sel_tex, NULL, NULL, &sel_tex_width, &sel_tex_height);
                                    sel_width = sel_tex_width / scale;
                                    backend_render_text_destroy(sel_tex);
                                }
                                free(sel_text);
                            }
                        }
                        
                        // 绘制选择区域
                        Rect sel_rect = {
                            sel_start_x,
                            line_y,
                            sel_width,
                            line_height
                        };
                        
                        // 确保选择区域在可见范围内
                        if (sel_rect.y + sel_rect.h > render_rect.y && sel_rect.y < render_rect.y + render_rect.h) {
                            backend_render_fill_rect(&sel_rect, selection_bg);
                        }
                    }
                    
                    // 移动到下一视觉行
                    // 如果是在换行符处结束，直接跳到下一个字符
                    if (split_pos < text_len && text[split_pos] == '\n') {
                        current_pos = split_pos + 1;
                    } else {
                        current_pos = split_pos;
                    }
                    
                    // 移动到下一行
                    line_y += line_height + 2; // 加2作为行间距
                }
            } else {
                // 单行模式 - 使用实际渲染宽度
                int sel_start_x = render_rect.x;
                int sel_width = 0;
                
                // 计算选择起始位置的X坐标
                if (start > 0) {
                    char* before_sel = (char*)malloc(start + 1);
                    if (before_sel) {
                        strncpy(before_sel, component->layer->text, start);
                        before_sel[start] = '\0';
                        
                        Texture* before_tex = backend_render_texture(layer->font->default_font, before_sel, layer->color);
                        if (before_tex) {
                            int before_width, before_height;
                            backend_query_texture(before_tex, NULL, NULL, &before_width, &before_height);
                            sel_start_x = render_rect.x + before_width / scale;
                            backend_render_text_destroy(before_tex);
                        }
                        free(before_sel);
                    }
                }
                
                // 计算选择区域的宽度
                int sel_len = end - start;
                if (sel_len > 0) {
                    char* sel_text = (char*)malloc(sel_len + 1);
                    if (sel_text) {
                        strncpy(sel_text, component->layer->text + start, sel_len);
                        sel_text[sel_len] = '\0';
                        
                        Texture* sel_tex = backend_render_texture(layer->font->default_font, sel_text, layer->color);
                        if (sel_tex) {
                            int sel_tex_width, sel_tex_height;
                            backend_query_texture(sel_tex, NULL, NULL, &sel_tex_width, &sel_tex_height);
                            sel_width = sel_tex_width / scale;
                            backend_render_text_destroy(sel_tex);
                        }
                        free(sel_text);
                    }
                }
                
                Rect sel_rect = {
                    sel_start_x,
                    render_rect.y + (render_rect.h - line_height) / 2,
                    sel_width,
                    line_height
                };
                
                backend_render_fill_rect(&sel_rect, selection_bg);
            }
        }
        
        // 绘制文本
        if (component->multiline) {
            // 多行模式下，实现自动换行
            char* text = component->layer->text;
            int text_len = strlen(text);
            int current_pos = 0;
            int line_y = render_rect.y - layer->scroll_offset; // 减去滚动偏移，实现滚动效果
            int line_height = 0;
            int char_width = 8; // 默认字符宽度
            
            // 先获取字符尺寸
            Texture* temp_tex = backend_render_texture(layer->font->default_font, "X", layer->color);
            if (temp_tex) {
                int temp_width, temp_height;
                backend_query_texture(temp_tex, NULL, NULL, &temp_width, &temp_height);
                char_width = temp_width / scale;
                line_height = temp_height / scale;
                backend_render_text_destroy(temp_tex);
            }
            
            // 优化：预先计算最大可见行数，避免渲染不可见行
            int max_visible_lines = (render_rect.h + (line_height + 2) - 1) / (line_height + 2) + 1; // 向上取整
            int first_visible_line = layer->scroll_offset / (line_height + 2);
            int last_visible_line = first_visible_line + max_visible_lines;
            
            // 循环处理每一行
            int current_line = 0;
            while (current_pos < text_len) {
            // 优化：快速跳过不可见的行
            if (current_line < first_visible_line) {
                // 快速跳过行，不需要详细渲染
                // 使用与高度计算相同的精确算法计算当前行会分成多少视觉行
                int line_end = current_pos;
                while (line_end < text_len && text[line_end] != '\n') {
                    line_end++;
                }
                
                // 计算整行文本的宽度
                int current_width = 0;
                char* temp_line = (char*)malloc(line_end - current_pos + 1);
                if (temp_line) {
                    strncpy(temp_line, text + current_pos, line_end - current_pos);
                    temp_line[line_end - current_pos] = '\0';
                    
                    Texture* line_tex = backend_render_texture(layer->font->default_font, temp_line, layer->color);
                    if (line_tex) {
                        int line_width, line_height_ignore;
                        backend_query_texture(line_tex, NULL, NULL, &line_width, &line_height_ignore);
                        current_width = line_width / scale;
                        backend_render_text_destroy(line_tex);
                    }
                    
                    free(temp_line);
                }
                
                // 计算当前行会分成多少视觉行（使用与高度计算相同的精确算法）
                int visual_lines_in_this_line = 1;
                if (current_width > render_rect.w) {
                    // 使用与 text_component_calculate_content_height 相同的精确算法
                    int segment_start = current_pos;
                    int needed_lines = 0;
                    
                    while (segment_start < line_end) {
                        int segment_end = segment_start;
                        int test_width = 0;
                        
                        while (segment_end < line_end) {
                            char* test_segment = (char*)malloc(segment_end - segment_start + 1);
                            if (test_segment) {
                                strncpy(test_segment, text + segment_start, segment_end - segment_start);
                                test_segment[segment_end - segment_start] = '\0';
                                
                                Texture* test_tex = backend_render_texture(layer->font->default_font, test_segment, layer->color);
                                if (test_tex) {
                                    int test_w, test_h;
                                    backend_query_texture(test_tex, NULL, NULL, &test_w, &test_h);
                                    test_width = test_w / scale;
                                    backend_render_text_destroy(test_tex);
                                }
                                
                                free(test_segment);
                                
                                if (test_width > render_rect.w && segment_end > segment_start) {
                                    segment_end--;
                                    break;
                                }
                            }
                            segment_end++;
                        }
                        
                        if (segment_end > segment_start) {
                            needed_lines++;
                            segment_start = segment_end;
                        } else {
                            needed_lines++;
                            segment_start++;
                        }
                    }
                    
                    visual_lines_in_this_line = needed_lines;
                }
                
                // 跳过这些视觉行
                current_line += visual_lines_in_this_line;
                
                // 移动到下一逻辑行
                if (line_end < text_len && text[line_end] == '\n') {
                    current_pos = line_end + 1;
                } else {
                    current_pos = line_end;
                }
                continue;
            }
                
                // 如果已经渲染了足够的可见行，退出循环
                if (current_line >= last_visible_line) {
                    break;
                }
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
                    
                    line_end++;
                }
                
                // 计算整行文本的宽度
                char* temp_line = (char*)malloc(line_end - current_pos + 1);
                if (temp_line) {
                    strncpy(temp_line, text + current_pos, line_end - current_pos);
                    temp_line[line_end - current_pos] = '\0';
                    
                    Texture* line_tex = backend_render_texture(layer->font->default_font, temp_line, layer->color);
                    if (line_tex) {
                        int line_width, line_height_ignore;
                        backend_query_texture(line_tex, NULL, NULL, &line_width, &line_height_ignore);
                        current_width = line_width / scale;
                        backend_render_text_destroy(line_tex);
                    }
                    
                    free(temp_line);
                }
                
                // 确定当前行的结束位置
                int split_pos = current_pos;
                
                // 如果文本没有超过宽度限制
                if (current_width <= max_width) {
                    // 使用整行（到\n或文本末尾）
                    split_pos = line_end;
                }
                // 如果文本超过宽度限制，需要硬换行
                else {
                    // 找到最大的不超过宽度的位置
                    split_pos = current_pos;
                    while (split_pos < line_end) {
                        char* test_line = (char*)malloc(split_pos - current_pos + 1);
                        if (test_line) {
                            strncpy(test_line, text + current_pos, split_pos - current_pos);
                            test_line[split_pos - current_pos] = '\0';
                            
                            Texture* test_tex = backend_render_texture(layer->font->default_font, test_line, layer->color);
                            if (test_tex) {
                                int test_width, test_height;
                                backend_query_texture(test_tex, NULL, NULL, &test_width, &test_height);
                                if (test_width / scale > max_width) {
                                    backend_render_text_destroy(test_tex);
                                    free(test_line);
                                    break;
                                }
                                backend_render_text_destroy(test_tex);
                            }
                            
                            free(test_line);
                        }
                        split_pos++;
                    } 
                    
                    if (split_pos > current_pos) {
                        split_pos--;
                    }
                }
                
                // 渲染当前行
                // 确保split_pos >= current_pos，防止负长度
                if (split_pos < current_pos) {
                    split_pos = current_pos;
                }
                
                char* current_line = (char*)malloc(split_pos - current_pos + 2);
                if (current_line) {
                    // 复制当前行文本，但不包含换行符
                    int copy_len = split_pos - current_pos;
                    // 检查是否在换行符处结束
                    if (copy_len > 0 && split_pos > current_pos && text[split_pos - 1] == '\n') {
                        copy_len--; // 不包含换行符
                    }
                    
                    // 确保copy_len不为负数
                    if (copy_len < 0) {
                        copy_len = 0;
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
                          
                        // 渲染文本
                        backend_render_text_copy(line_tex, NULL, &text_rect);
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
            current_line++; // 增加行计数
            }
            
            // 计算文本总高度（考虑所有行的高度和间距）
            // 使用content_height而不是line_y计算
            int total_text_height = layer->content_height;
            
            // 限制滚动范围
            if (total_text_height <= render_rect.h) {
                layer->scroll_offset = 0; // 文本高度小于可见区域，不需要滚动
            } else {
                // 确保滚动不会超出上边界
                if (layer->scroll_offset < 0) {
                    layer->scroll_offset = 0;
                }
                // 确保滚动不会超出下边界
                int max_scroll_y = total_text_height - render_rect.h;
                if (layer->scroll_offset > max_scroll_y) {
                    layer->scroll_offset = max_scroll_y;
                }
            }
        } else {
            // 单行模式，应用水平滚动
            Texture* tex = backend_render_texture(layer->font->default_font, component->layer->text, layer->color);
            if (tex) {
                int text_width, text_height;
                backend_query_texture(tex, NULL, NULL, &text_width, &text_height);
                
                // 计算文本位置，应用滚动偏移
                Rect text_rect = {
                    render_rect.x - component->scroll_x,  // 应用水平滚动
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
        // 确保光标位置在有效范围内
        int text_len = strlen(component->layer->text);
        if (component->cursor_pos < 0) component->cursor_pos = 0;
        if (component->cursor_pos > text_len) component->cursor_pos = text_len;
        
        // 获取行高
        int line_height = 20;
        if (layer->font && layer->font->default_font) {
            Texture* temp_tex = backend_render_texture(layer->font->default_font, "X", layer->color);
            if (temp_tex) {
                int temp_width, temp_height;
                backend_query_texture(temp_tex, NULL, NULL, &temp_width, &temp_height);
                line_height = temp_height / scale;
                backend_render_text_destroy(temp_tex);
            }
        }
        
        if (component->multiline) {
            // 多行模式：需要考虑自动换行来计算光标位置
            char* text = component->layer->text;
            int current_pos = 0;
            int visual_line = 0;
            int max_width = render_rect.w;
            int cursor_x = render_rect.x;
            int cursor_y = render_rect.y - layer->scroll_offset;
            
            // 遍历文本，使用与渲染相同的算法找到光标所在的视觉行
            while (current_pos < text_len && current_pos < component->cursor_pos) {
                // 查找当前行可以显示的最大文本长度
                int line_end = current_pos;
                
                // 尝试找到合适的换行点
                while (line_end < text_len) {
                    if (text[line_end] == '\n') {
                        break;
                    }
                    line_end++;
                }
                
                // 计算整行文本的宽度
                int current_width = 0;
                char* temp_line = (char*)malloc(line_end - current_pos + 1);
                if (temp_line) {
                    strncpy(temp_line, text + current_pos, line_end - current_pos);
                    temp_line[line_end - current_pos] = '\0';
                    
                    Texture* line_tex = backend_render_texture(layer->font->default_font, temp_line, layer->color);
                    if (line_tex) {
                        int line_width, line_height_ignore;
                        backend_query_texture(line_tex, NULL, NULL, &line_width, &line_height_ignore);
                        current_width = line_width / scale;
                        backend_render_text_destroy(line_tex);
                    }
                    
                    free(temp_line);
                }
                
                // 确定当前视觉行的结束位置
                int split_pos = current_pos;
                
                // 如果文本没有超过宽度限制
                if (current_width <= max_width) {
                    // 使用整行（到\n或文本末尾）
                    split_pos = line_end;
                }
                // 如果文本超过宽度限制，需要硬换行
                else {
                    split_pos = current_pos;
                    while (split_pos < line_end) {
                        char* test_line = (char*)malloc(split_pos - current_pos + 1);
                        if (test_line) {
                            strncpy(test_line, text + current_pos, split_pos - current_pos);
                            test_line[split_pos - current_pos] = '\0';
                            
                            Texture* test_tex = backend_render_texture(layer->font->default_font, test_line, layer->color);
                            if (test_tex) {
                                int test_width, test_height;
                                backend_query_texture(test_tex, NULL, NULL, &test_width, &test_height);
                                if (test_width / scale > max_width) {
                                    backend_render_text_destroy(test_tex);
                                    free(test_line);
                                    break;
                                }
                                backend_render_text_destroy(test_tex);
                            }
                            
                            free(test_line);
                        }
                        split_pos++;
                    } 
                    
                    if (split_pos > current_pos) {
                        split_pos--;
                    }
                }
                
                if (split_pos < current_pos) {
                    split_pos = current_pos;
                }
                
                // 检查光标是否在当前视觉行内
                if (component->cursor_pos <= split_pos) {
                    // 光标在当前视觉行，计算X坐标
                    int len_to_cursor = component->cursor_pos - current_pos;
                    if (len_to_cursor > 0) {
                        char* text_before_cursor = (char*)malloc(len_to_cursor + 1);
                        if (text_before_cursor) {
                            strncpy(text_before_cursor, text + current_pos, len_to_cursor);
                            text_before_cursor[len_to_cursor] = '\0';
                            
                            Texture* before_tex = backend_render_texture(layer->font->default_font, text_before_cursor, layer->color);
                            if (before_tex) {
                                int before_width, before_height;
                                backend_query_texture(before_tex, NULL, NULL, &before_width, &before_height);
                                cursor_x = render_rect.x + before_width / scale;
                                backend_render_text_destroy(before_tex);
                            }
                            free(text_before_cursor);
                        }
                    } else {
                        cursor_x = render_rect.x;
                    }
                    
                    cursor_y = render_rect.y + visual_line * (line_height + 2) - layer->scroll_offset;
                    break;
                }
                
                // 移动到下一视觉行
                visual_line++;
                if (split_pos < text_len && text[split_pos] == '\n') {
                    current_pos = split_pos + 1;
                } else {
                    current_pos = split_pos;
                }
            }
            
            // 如果光标在文本末尾
            if (current_pos >= component->cursor_pos && component->cursor_pos == text_len) {
                // 计算文本最后一行
                cursor_x = render_rect.x;
                
                // 获取最后一行文本（从当前pos到文本末尾）
                int remaining_len = text_len - current_pos;
                if (remaining_len > 0) {
                    char* last_line = (char*)malloc(remaining_len + 1);
                    if (last_line) {
                        strncpy(last_line, text + current_pos, remaining_len);
                        last_line[remaining_len] = '\0';
                        
                        // 计算最后一行文本的宽度
                        Texture* last_tex = backend_render_texture(layer->font->default_font, last_line, layer->color);
                        if (last_tex) {
                            int last_width, last_height;
                            backend_query_texture(last_tex, NULL, NULL, &last_width, &last_height);
                            cursor_x = render_rect.x + last_width / scale;
                            backend_render_text_destroy(last_tex);
                        }
                        free(last_line);
                    }
                }
                
                cursor_y = render_rect.y + visual_line * (line_height + 2) - layer->scroll_offset;
            }
            
            // 绘制光标
            Rect cursor_rect = {
                cursor_x,
                cursor_y,
                2,
                line_height
            };
            
            backend_render_fill_rect(&cursor_rect, component->cursor_color);
        } else {
            // 单行模式：计算光标前文本的宽度，并应用滚动偏移
            int char_width = 0;
            if (component->cursor_pos > 0) {
                char* temp_text = (char*)malloc(component->cursor_pos + 1);
                if (temp_text) {
                    strncpy(temp_text, component->layer->text, component->cursor_pos);
                    temp_text[component->cursor_pos] = '\0';
                    
                    Texture* cursor_text_texture = backend_render_texture(layer->font->default_font, temp_text, layer->color);
                    if (cursor_text_texture) {
                        int text_width;
                        backend_query_texture(cursor_text_texture, NULL, NULL, &text_width, NULL);
                        char_width = text_width / scale;
                        backend_render_text_destroy(cursor_text_texture);
                    }
                    free(temp_text);
                }
            }
            
            Rect cursor_rect = {
                render_rect.x + char_width - component->scroll_x,  // 应用水平滚动偏移
                render_rect.y + (render_rect.h - line_height) / 2,
                2,
                line_height
            };
            
            backend_render_fill_rect(&cursor_rect, component->cursor_color);
        }
    }
    
    // 恢复裁剪区域
    backend_render_set_clip_rect(&prev_clip);
    
    // 如果是多行模式且需要滚动，绘制滚动条
    if (component->multiline) {
        // 使用已经计算好的内容高度，而不是重新计算
        int total_text_height = layer->content_height;
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
            float scroll_percent = 0;
            if (total_text_height > visible_height) {
                scroll_percent = (float)layer->scroll_offset / (total_text_height - visible_height);
            }
            int slider_height = (int)(visible_height * (float)visible_height / total_text_height);
            if (slider_height < 20) slider_height = 20; // 滑块最小高度
            
            Rect scrollbar_slider = {
                scrollbar_bg.x,
                scrollbar_bg.y + (int)(scroll_percent * (scrollbar_bg.h - slider_height)),
                scrollbar_bg.w,
                slider_height
            };
            Color scrollbar_slider_color = {150, 150, 150, 255};
            backend_render_fill_rect(&scrollbar_slider, scrollbar_slider_color);
        }
    }
}

 

// 更新图层的内容高度
void text_component_update_content_height(TextComponent* component) {
    if (!component || !component->layer) {
        return;
    }
    
    // 计算并设置内容高度
    int content_height = text_component_calculate_content_height(component);
    component->layer->content_height = content_height;
    
    // 如果内容高度小于可见区域高度（减去内边距），重置滚动偏移
    int visible_height = component->layer->rect.h - 10; // 与滚动条计算保持一致
    if (content_height <= visible_height) {
        component->layer->scroll_offset = 0;
    }
}





