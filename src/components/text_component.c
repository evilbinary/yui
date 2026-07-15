#include "text_component.h"
#include "../ytype.h"
#include "../layer.h"
#include "../layer_update.h"
#include "../event.h"
#include "../backend.h"
#include "../render.h"
#include "../util.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>


#define TEXT_LINE_SPACING 2
#define TEXT_LINE_NUMBER_GAP 12
#define TEXT_DEFAULT_PADDING 5
#define TEXT_INTERNAL_SCROLLBAR_WIDTH 5

static void text_component_get_padding(Layer* layer, int* top, int* right, int* bottom, int* left)
{
    int pad_top;
    int pad_right;
    int pad_bottom;
    int pad_left;

    if (!layer) {
        if (top) {
            *top = TEXT_DEFAULT_PADDING;
        }
        if (right) {
            *right = TEXT_DEFAULT_PADDING;
        }
        if (bottom) {
            *bottom = TEXT_DEFAULT_PADDING;
        }
        if (left) {
            *left = TEXT_DEFAULT_PADDING;
        }
        return;
    }

    pad_top = layer_padding_get(layer, 0);
    pad_right = layer_padding_get(layer, 1);
    pad_bottom = layer_padding_get(layer, 2);
    pad_left = layer_padding_get(layer, 3);
    if (pad_top == 0 && pad_right == 0 && pad_bottom == 0 && pad_left == 0) {
        pad_top = pad_right = pad_bottom = pad_left = TEXT_DEFAULT_PADDING;
    }

    if (top) {
        *top = pad_top;
    }
    if (right) {
        *right = pad_right;
    }
    if (bottom) {
        *bottom = pad_bottom;
    }
    if (left) {
        *left = pad_left;
    }
}

static int text_component_get_left_padding(TextComponent* component) {
    int left_padding = TEXT_DEFAULT_PADDING;
    Layer* layer = component ? component->layer : NULL;

    if (layer) {
        left_padding = layer_padding_get(layer, 3);
        if (left_padding == 0) {
            left_padding = TEXT_DEFAULT_PADDING;
        }
    }
    if (component && component->show_line_numbers && component->multiline) {
        left_padding += component->line_number_width + TEXT_LINE_NUMBER_GAP;
    }
    return left_padding;
}

static void text_component_get_content_rect(TextComponent* component, Layer* layer, Rect* out);

static int text_component_get_line_height(TextComponent* component) {
    if (!component || !component->layer) return 20;
    if (component->line_height_valid && component->cached_line_height > 0) {
        return component->cached_line_height;
    }
    int line_height = component->line_height > 0 ? component->line_height : 20;
    if (component->layer->font && component->layer->font->default_font) {
        Texture* temp_tex = backend_render_texture(component->layer->font->default_font, "X", component->layer->color);
        if (temp_tex) {
            int temp_width = 0, temp_height = 0;
            backend_query_texture(temp_tex, NULL, NULL, &temp_width, &temp_height);
            line_height = temp_height / scale;
            backend_render_text_destroy(temp_tex);
            component->cached_line_height = line_height;
            component->line_height_valid = 1;
        }
    }
    return line_height;
}

static int text_component_measure_width(TextComponent* component, const char* text, int start, int end) {
    if (!component || !component->layer || !component->layer->font || !component->layer->font->default_font) {
        return 0;
    }
    return text_syntax_measure_width(component->layer->font->default_font, text, start, end, component->layer->color);
}

static int text_component_find_wrap_end(const char* text, int start, int line_end, int max_width, TextComponent* component) {
    if (start >= line_end) return start;
    if (text_component_measure_width(component, text, start, line_end) <= max_width) {
        return line_end;
    }
    int lo = start + 1;
    int hi = line_end;
    while (lo < hi) {
        int mid = lo + (hi - lo) / 2;
        if (text_component_measure_width(component, text, start, mid) <= max_width) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    return lo - 1;
}

void text_component_invalidate_layout(TextComponent* component) {
    if (!component) return;
    component->text_revision++;
    component->line_height_valid = 0;
    component->layout_cache_text_len = -1;
}

static void text_component_free_layout(TextComponent* component) {
    if (!component) return;
    free(component->layout_starts);
    component->layout_starts = NULL;
    component->layout_count = 0;
    component->layout_cache_revision = -1;
    component->layout_cache_max_width = 0;
    component->layout_cache_text_len = -1;
}

static void text_component_ensure_layout(TextComponent* component, int max_width) {
    if (!component || !component->layer) return;
    const char* text = component->layer->text ? component->layer->text : "";
    int text_len = (int)strlen(text);
    if (max_width < 1) max_width = 1;

    if (component->layout_starts &&
        component->layout_cache_revision == component->text_revision &&
        component->layout_cache_max_width == max_width &&
        component->layout_cache_text_len == text_len) {
        return;
    }
    int capacity = 64;
    int* starts = (int*)malloc(sizeof(int) * (size_t)capacity);
    if (!starts) return;

    int count = 0;
    int current_pos = 0;
    while (current_pos <= text_len) {
        if (count >= capacity) {
            capacity *= 2;
            int* grown = (int*)realloc(starts, sizeof(int) * (size_t)capacity);
            if (!grown) break;
            starts = grown;
        }
        starts[count++] = current_pos;
        if (current_pos >= text_len) break;

        int line_end = current_pos;
        while (line_end < text_len && text[line_end] != '\n') {
            line_end++;
        }

        int split_pos = text_component_find_wrap_end(text, current_pos, line_end, max_width, component);
        if (split_pos <= current_pos) {
            split_pos = current_pos + 1;
        }
        if (split_pos < line_end) {
            current_pos = split_pos;
            continue;
        }
        if (line_end < text_len && text[line_end] == '\n') {
            current_pos = line_end + 1;
        } else {
            current_pos = line_end;
        }
    }

    free(component->layout_starts);
    component->layout_starts = starts;
    component->layout_count = count;
    component->layout_cache_revision = component->text_revision;
    component->layout_cache_max_width = max_width;
    component->layout_cache_text_len = text_len;
}

static int text_component_count_visual_lines_in_range(TextComponent* component, int max_width,
                                                      int start, int logical_end) {
    if (!component || !component->layer || !component->layer->text) return 1;
    char* text = component->layer->text;
    if (start >= logical_end) return 1;

    int width = text_component_measure_width(component, text, start, logical_end);
    if (width <= max_width) return 1;

    int lines = 0;
    int pos = start;
    while (pos < logical_end) {
        int split_pos = text_component_find_wrap_end(text, pos, logical_end, max_width, component);
        if (split_pos <= pos) split_pos = pos + 1;
        lines++;
        pos = split_pos;
    }
    return lines > 0 ? lines : 1;
}

static void text_component_render_text_segment(TextComponent* component, const char* text,
                                               int start, int end, int x, int y) {
    if (!component || !component->layer || !component->layer->font || !component->layer->font->default_font) {
        return;
    }
    if (start >= end) return;

    if (component->syntax_config.language != TEXT_SYNTAX_NONE) {
        text_syntax_render_range(component->layer->font->default_font, text, start, end,
                                 &component->syntax_config, x, y);
        return;
    }

    int len = end - start;
    char* buf = (char*)malloc((size_t)len + 1);
    if (!buf) return;
    memcpy(buf, text + start, (size_t)len);
    buf[len] = '\0';
    Texture* tex = backend_render_texture(component->layer->font->default_font, buf, component->layer->color);
    free(buf);
    if (!tex) return;
    int width = 0, height = 0;
    backend_query_texture(tex, NULL, NULL, &width, &height);
    Rect rect = {x, y, width / scale, height / scale};
    backend_render_text_copy(tex, NULL, &rect);
    backend_render_text_destroy(tex);
}

static TextSyntaxLanguage text_component_parse_syntax_language(const char* language) {
    if (!language || language[0] == '\0') return TEXT_SYNTAX_NONE;
    if (strcmp(language, "json") == 0) return TEXT_SYNTAX_JSON;
    if (strcmp(language, "sql") == 0) return TEXT_SYNTAX_SQL;
    if (strcmp(language, "markdown") == 0 || strcmp(language, "md") == 0) return TEXT_SYNTAX_MARKDOWN;
    return TEXT_SYNTAX_NONE;
}

void text_component_set_syntax_highlight(TextComponent* component, const char* language) {
    if (!component) return;
    TextSyntaxLanguage lang = text_component_parse_syntax_language(language);
    text_syntax_config_init(&component->syntax_config, lang, component->layer ? component->layer->color : (Color){0, 0, 0, 255});
}

static void text_component_parse_syntax_colors(TextComponent* component, cJSON* colors_obj) {
    if (!component || !colors_obj || !cJSON_IsObject(colors_obj)) return;
    cJSON* child = colors_obj->child;
    while (child) {
        if (cJSON_IsString(child) && child->valuestring) {
            Color color = component->syntax_config.default_color;
            parse_color(child->valuestring, &color);
            text_syntax_config_set_color(&component->syntax_config, child->string, color);
        }
        child = child->next;
    }
}

static const char* const TEXT_SYNTAX_COLOR_KEYS[] = {
    "keyword", "string", "number", "comment", "punctuation", NULL
};

static void text_component_apply_syntax_color_key(TextComponent* component, const char* key, const char* value) {
    if (!component || !key || !value) return;
    Color color = component->syntax_config.default_color;
    parse_color(value, &color);
    text_syntax_config_set_color(&component->syntax_config, key, color);
}

static int text_component_apply_selection_color_from_json(TextComponent* component, cJSON* color_obj) {
    if (!component || !color_obj) {
        return 0;
    }

    Color selection_color = component->selection_color;
    if (cJSON_IsString(color_obj)) {
        parse_color(color_obj->valuestring, &selection_color);
        text_component_set_selection_color(component, selection_color);
        return 1;
    }

    if (cJSON_IsObject(color_obj)) {
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
        text_component_set_selection_color(component, selection_color);
        return 1;
    }

    return 0;
}

static void text_component_apply_theme_style(Layer* layer, cJSON* style) {
    if (!layer || !style || !layer->component) return;

    TextComponent* component = (TextComponent*)layer->component;

    cJSON* bg_color = cJSON_GetObjectItem(style, "bgColor");
    if (bg_color && cJSON_IsString(bg_color)) {
        parse_color(bg_color->valuestring, &layer->bg_color);
    }

    cJSON* color = cJSON_GetObjectItem(style, "color");
    if (color && cJSON_IsString(color)) {
        parse_color(color->valuestring, &layer->color);
        component->syntax_config.default_color = layer->color;
    }

    cJSON* font_size = cJSON_GetObjectItem(style, "fontSize");
    if (font_size && cJSON_IsNumber(font_size) && layer->font) {
        layer->font->size = font_size->valueint;
        component->line_height_valid = 0;
    }

    cJSON* line_number_bg = cJSON_GetObjectItem(style, "lineNumberBgColor");
    if (line_number_bg && cJSON_IsString(line_number_bg)) {
        Color c;
        parse_color(line_number_bg->valuestring, &c);
        text_component_set_line_number_bg_color(component, c);
    }

    cJSON* line_number_color = cJSON_GetObjectItem(style, "lineNumberColor");
    if (line_number_color && cJSON_IsString(line_number_color)) {
        Color c;
        parse_color(line_number_color->valuestring, &c);
        text_component_set_line_number_color(component, c);
    }

    cJSON* cursor_color = cJSON_GetObjectItem(style, "cursorColor");
    if (cursor_color && cJSON_IsString(cursor_color)) {
        Color c;
        parse_color(cursor_color->valuestring, &c);
        text_component_set_cursor_color(component, c);
    }

    cJSON* selection_color = cJSON_GetObjectItem(style, "selectionColor");
    text_component_apply_selection_color_from_json(component, selection_color);

    cJSON* syntax_colors = cJSON_GetObjectItem(style, "syntaxColors");
    if (syntax_colors && cJSON_IsObject(syntax_colors)) {
        text_component_parse_syntax_colors(component, syntax_colors);
    }

    for (int i = 0; TEXT_SYNTAX_COLOR_KEYS[i]; i++) {
        cJSON* item = cJSON_GetObjectItem(style, TEXT_SYNTAX_COLOR_KEYS[i]);
        if (item && cJSON_IsString(item)) {
            text_component_apply_syntax_color_key(component, TEXT_SYNTAX_COLOR_KEYS[i], item->valuestring);
        }
    }

    {
        cJSON* padding = cJSON_GetObjectItem(style, "padding");
        if (padding && layer_padding_apply_from_json(layer->padding, padding) && layer->layout_manager) {
            memcpy(layer->layout_manager->padding, layer->padding, sizeof(layer->padding));
            mark_layer_dirty(layer, DIRTY_LAYOUT);
        }
    }

    mark_layer_dirty(layer, DIRTY_COLOR | DIRTY_TEXT);
}

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
    component->text_revision = 0;
    component->layout_starts = NULL;
    component->layout_count = 0;
    component->layout_cache_revision = -1;
    component->layout_cache_max_width = 0;
    component->layout_cache_text_len = -1;
    text_syntax_config_init(&component->syntax_config, TEXT_SYNTAX_NONE, layer->color);
    
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
    layer->set_style = text_component_apply_theme_style;

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
    
    // 解析selectionColor属性（顶层，兼容旧配置；style 内由 set_style 处理）
    if (cJSON_HasObjectItem(json_obj, "selectionColor")) {
        text_component_apply_selection_color_from_json(
            layer->component, cJSON_GetObjectItem(json_obj, "selectionColor"));
    }

      // 解析事件绑定
    cJSON* events = cJSON_GetObjectItem(json_obj, "events");
    if (events) {
        // 解析onChange事件
        if (cJSON_HasObjectItem(events, "onChange")) {
            cJSON* on_change_obj = cJSON_GetObjectItem(events, "onChange");
            if (cJSON_IsString(on_change_obj)) {
                const char* event_name = on_change_obj->valuestring;
                if (event_name[0] == '@') {
                    event_name++;
                }
                component->change_name = strdup(event_name);
                EventHandler handler = find_event_by_name(event_name);
                component->on_change = handler;
            }
        }
    }

    if (cJSON_HasObjectItem(json_obj, "syntaxHighlight")) {
        cJSON* syntax_obj = cJSON_GetObjectItem(json_obj, "syntaxHighlight");
        if (cJSON_IsString(syntax_obj) && syntax_obj->valuestring) {
            text_component_set_syntax_highlight(component, syntax_obj->valuestring);
        }
    }
    if (cJSON_HasObjectItem(json_obj, "syntaxColors")) {
        text_component_parse_syntax_colors(component, cJSON_GetObjectItem(json_obj, "syntaxColors"));
    }

    cJSON* style_obj = cJSON_GetObjectItem(json_obj, "style");
    if (style_obj && layer->set_style) {
        layer->set_style(layer, style_obj);
    }

    return component;
}

// 销毁文本组件
void text_component_destroy(TextComponent* component) {
    if (component) {
        text_component_free_layout(component);
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
    text_component_invalidate_layout(component);
    
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
int text_component_register_event(Layer* layer, const char* event_name, const char* event_func_name, EventHandler event_handler){
    if(strcmp(event_name,"change")==0 || strcmp(event_name,"onChange")==0){
        TextComponent* component = (TextComponent*)layer->component;
        component->on_change = event_handler;
        if (event_func_name && event_func_name[0] == '@') {
            event_func_name++;
        }
        component->change_name = strdup(event_func_name);
        return 0;
    }
    return -1;
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
        const char* event_name = component->change_name;
        if (event_name[0] == '@') {
            event_name++;
        }
        EventHandler handler = find_event_by_name(event_name);
        component->on_change = handler;
    }
    
    // 检查是否有可用的事件处理器
    if (component->on_change) {
        component->on_change(component->layer);
    } else if (component->change_name) {
        printf("text_component_trigger_on_change not found onchange event %s\n", component->change_name);
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
        text_component_invalidate_layout(component);
        
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
    text_component_invalidate_layout(component);
    
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
    Rect render_rect;
    text_component_get_content_rect(component, component->layer, &render_rect);
    
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
    if (!component || !component->layer) {
        return 0;
    }

    if (!component->multiline) {
        return component->layer->rect.h;
    }

    const char* text = component->layer->text ? component->layer->text : "";
    int line_height = text_component_get_line_height(component);
    Rect content_rect;
    text_component_get_content_rect(component, component->layer, &content_rect);
    int max_width = content_rect.w;
    if (max_width < 1) max_width = 1;

    text_component_ensure_layout(component, max_width);
    if (component->layout_count <= 0) {
        return line_height + TEXT_LINE_SPACING;
    }
    return component->layout_count * (line_height + TEXT_LINE_SPACING);
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
        // 获取光标前一个 UTF-8 字符的字节长度
        int char_len = get_prev_utf8_char_len(component->layer->text, component->cursor_pos);
        if (char_len <= 0) char_len = 1;
        
        // 计算新文本长度
        size_t new_len = len - char_len;
        
        // 创建临时缓冲区存储新文本
        char* new_text = malloc(new_len + 1);
        if (!new_text) {
            return;
        }
        
        // 复制光标位置之前的部分（不包括要删除的字符）
        memcpy(new_text, component->layer->text, component->cursor_pos - char_len);
        // 复制光标位置之后的部分
        memcpy(new_text + component->cursor_pos - char_len, component->layer->text + component->cursor_pos, len - component->cursor_pos + 1);
        
        // 设置新文本
        layer_set_text(component->layer, new_text);
        free(new_text);
        
        component->cursor_pos -= char_len;
        text_component_invalidate_layout(component);
        
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
    text_component_invalidate_layout(component);
    
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
    text_component_invalidate_layout(component);
    
    // 更新内容高度（只调用一次）
    text_component_update_content_height(component);
    
    // 触发 onChange 事件
    text_component_trigger_on_change(component);
    
    // 清除选择
    component->selection_start = -1;
    component->selection_end = -1;
}

// 处理键盘事件
int text_component_handle_key_event(Layer* layer, KeyEvent* event) {
    if (!layer || !event || !layer->component) {
        return 0;
    }

    TextComponent* component = (TextComponent*)layer->component;

    if (!component->editable) {
        return 0;
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
                            // 复制到剪贴板
                            backend_set_clipboard_text(selected_text);
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
                            // 复制到剪贴板
                            backend_set_clipboard_text(selected_text);
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
    } else if (event->type == KEY_EVENT_TEXT_EDITING) {
        // 处理 IME 文本编辑事件（候选词预览）
        // 这里可以显示候选词或组合字符串
        // 暂时只打印调试信息
        printf("IME Editing: '%s', start=%d, length=%d\n",
               event->data.text.text,
               event->data.text.start,
               event->data.text.length);
    }
    return 0;
}







static int text_component_get_vertical_scrollbar_track(Layer* layer, Rect* track_out) {
    if (!layer || !track_out) return 0;

    int visible_height = layer->rect.h;
    visible_height -= layer_padding_get(layer, 0) + layer_padding_get(layer, 2);

    if ((layer->scrollable == 1 || layer->scrollable == 3) &&
        layer->scrollbar_v && layer->scrollbar_v->visible &&
        layer->content_height > visible_height) {
        int scrollbar_width = layer->scrollbar_v->thickness > 0 ? layer->scrollbar_v->thickness : 8;
        track_out->x = layer->rect.x + layer->rect.w - scrollbar_width;
        track_out->y = layer->rect.y;
        track_out->w = scrollbar_width;
        track_out->h = visible_height;
        return 1;
    }

    TextComponent* component = (TextComponent*)layer->component;
    if (component && component->multiline &&
        layer->scrollable != 1 && layer->scrollable != 3) {
        int pad_top;
        int pad_right;
        int pad_bottom;
        int inner_visible;
        text_component_get_padding(layer, &pad_top, &pad_right, &pad_bottom, NULL);
        inner_visible = layer->rect.h - pad_top - pad_bottom;
        if (layer->content_height <= inner_visible) {
            return 0;
        }
        track_out->x = layer->rect.x + layer->rect.w - pad_right - TEXT_INTERNAL_SCROLLBAR_WIDTH;
        track_out->y = layer->rect.y + pad_top;
        track_out->w = TEXT_INTERNAL_SCROLLBAR_WIDTH;
        track_out->h = layer->rect.h - pad_top - pad_bottom;
        return 1;
    }
    return 0;
}

static int text_component_point_in_vertical_scrollbar(Layer* layer, Point pt) {
    Rect track;
    if (!text_component_get_vertical_scrollbar_track(layer, &track)) return 0;
    return point_in_rect(pt, track);
}

static void text_component_get_content_rect(TextComponent* component, Layer* layer, Rect* out) {
    if (!component || !layer || !out) return;

    int pad_top;
    int pad_right;
    int pad_bottom;
    int left_padding = text_component_get_left_padding(component);
    text_component_get_padding(layer, &pad_top, &pad_right, &pad_bottom, NULL);

    out->x = layer->rect.x + left_padding;
    out->y = layer->rect.y + pad_top;
    out->w = layer->rect.w - left_padding - pad_right;
    out->h = layer->rect.h - pad_top - pad_bottom;

    Rect track;
    if (text_component_get_vertical_scrollbar_track(layer, &track)) {
        int content_right = out->x + out->w;
        if (content_right > track.x) {
            out->w = track.x - out->x;
            if (out->w < 0) out->w = 0;
        }
    }
}

static void text_component_get_line_range_at_pos(TextComponent* component, Layer* layer,
                                                 int pos, int* out_start, int* out_end) {
    if (!component || !layer || !out_start || !out_end) return;

    const char* text = layer->text ? layer->text : "";
    int text_len = (int)strlen(text);
    if (pos < 0) pos = 0;
    if (pos > text_len) pos = text_len;

    if (!component->multiline) {
        *out_start = 0;
        *out_end = text_len;
        return;
    }

    Rect render_rect;
    text_component_get_content_rect(component, layer, &render_rect);
    text_component_ensure_layout(component, render_rect.w);

    for (int i = 0; i < component->layout_count; i++) {
        int start = component->layout_starts[i];
        int end = (i + 1 < component->layout_count)
            ? component->layout_starts[i + 1] : text_len;
        while (end > start && text[end - 1] == '\n') {
            end--;
        }

        if (pos >= start && pos <= end) {
            *out_start = start;
            *out_end = end;
            return;
        }
    }

    *out_start = get_line_start(component, pos);
    *out_end = get_line_end(component, pos);
}

// 处理鼠标事件
int text_component_handle_mouse_event(Layer* layer, MouseEvent* event) {
    if (!layer || !event || !layer->component) {
        return 0;
    }
    
    TextComponent* component = (TextComponent*)layer->component;
    Point pt = {event->x, event->y};

    if (layer->scrollbar_v && layer->scrollbar_v->is_dragging) {
        if (event->state == SDL_RELEASED && event->button == SDL_BUTTON_LEFT) {
            component->is_selecting = 0;
        }
        return 1;
    }

    if (text_component_point_in_vertical_scrollbar(layer, pt)) {
        if (event->state == SDL_RELEASED && event->button == SDL_BUTTON_LEFT) {
            component->is_selecting = 0;
        }
        return 1;
    }

    Rect content_rect;
    text_component_get_content_rect(component, layer, &content_rect);
    
    // 鼠标左键按下 - 设置光标位置并开始选择
    if (event->state == SDL_PRESSED && event->button == SDL_BUTTON_LEFT) {
        // 检查组件是否可获得焦点
        if (layer->focusable && point_in_rect(pt, content_rect)) {
            // 设置焦点状态
            SET_STATE(layer, LAYER_STATE_FOCUSED);

            // 启用 IME 文本输入
            if (component->editable) {
                backend_start_text_input();
                // 设置输入区域，让输入法候选框出现在文本框下方
                Rect rect;
                rect.x = layer->rect.x;
                rect.y = layer->rect.y + layer->rect.h;
                rect.w = layer->rect.w;
                rect.h = 0;
                backend_set_text_input_rect(&rect);
            }
        }
        if (point_in_rect(pt, content_rect)) {
            int click_pos = text_component_get_position_from_point(component, pt, layer);

            if (event->clicks >= 2) {
                int line_start, line_end;
                text_component_get_line_range_at_pos(component, layer, click_pos,
                                                       &line_start, &line_end);
                component->selection_start = line_start;
                component->selection_end = line_end;
                component->cursor_pos = line_end;
                component->is_selecting = 0;
                text_component_update_scroll_for_cursor(component);
                mark_layer_dirty(layer, DIRTY_TEXT);
            } else {
                component->cursor_pos = click_pos;
                component->selection_start = click_pos;
                component->selection_end = click_pos;
                component->is_selecting = 1;
                text_component_update_scroll_for_cursor(component);
            }
        } else if (point_in_rect(pt, layer->rect)) {
            component->selection_start = -1;
            component->selection_end = -1;
            component->is_selecting = 0;
        } else {
            // 点击文本区域外，清除选择并移除焦点
            component->selection_start = -1;
            component->selection_end = -1;
            component->is_selecting = 0;
            // 移除焦点状态
            CLEAR_STATE(layer, LAYER_STATE_FOCUSED);
            // 停止 IME 文本输入
            backend_stop_text_input();
        }
    }
    // 鼠标拖动 - 更新选择范围
    else if (event->state == SDL_MOUSEMOTION && (event->button == SDL_BUTTON_LEFT) && component->is_selecting) {
        int drag_pos;
        
        if (point_in_rect(pt, content_rect)) {
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
    return 0;
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
    
    Rect render_rect;
    text_component_get_content_rect(component, layer, &render_rect);
    
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
    if (layer->font && !layer->font->default_font) {
        load_all_fonts(layer);
    }
    if (layer->dirty_flags & DIRTY_TEXT) {
        text_component_invalidate_layout(component);
        layer->dirty_flags &= ~DIRTY_TEXT;
    }
    component->syntax_config.default_color = layer->color;
    text_component_update_content_height(component);
    // 绘制背景
    backend_render_fill_rect(&layer->rect, layer->bg_color);
    
    // 如果显示行号且为多行模式，绘制行号背景和行号
    if (component->show_line_numbers && component->multiline) {
        int pad_top;
        int pad_right;
        int pad_bottom;
        int pad_left;
        Rect line_number_bg;
        text_component_get_padding(layer, &pad_top, &pad_right, &pad_bottom, &pad_left);
        line_number_bg.x = layer->rect.x + pad_left;
        line_number_bg.y = layer->rect.y + pad_top;
        line_number_bg.w = component->line_number_width;
        line_number_bg.h = layer->rect.h - pad_top - pad_bottom;
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
        int line_height = text_component_get_line_height(component);
        
        // 遍历文本，使用与渲染相同的算法，为每个逻辑行渲染行号
        char* text = component->layer->text;
        int text_len = strlen(text);
        int current_pos = 0;
        int visual_line = 0;  // 视觉行号
        int logical_line = 1;  // 逻辑行号（从1开始）
        // 计算文本内容区域的宽度（与正文渲染区域一致）
        Rect content_rect_for_wrap;
        text_component_get_content_rect(component, layer, &content_rect_for_wrap);
        int max_width = content_rect_for_wrap.w;
        
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
                        line_number_bg.x + line_number_bg.w - num_width / scale - 10,
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
    
    // 准备渲染区域（排除滚动条轨道）
    Rect render_rect;
    text_component_get_content_rect(component, layer, &render_rect);
    
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
            int line_height = text_component_get_line_height(component);
            
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
            char* text = component->layer->text;
            int text_len = (int)strlen(text);
            int line_height = text_component_get_line_height(component);
            int line_stride = line_height + TEXT_LINE_SPACING;

            text_component_ensure_layout(component, render_rect.w);

            int first_visible_line = layer->scroll_offset / line_stride;
            int max_visible_lines = render_rect.h / line_stride + 2;
            int last_visible_line = first_visible_line + max_visible_lines;

            for (int line_index = 0; line_index < component->layout_count; line_index++) {
                if (line_index < first_visible_line) continue;
                if (line_index >= last_visible_line) break;

                int start = component->layout_starts[line_index];
                int end = (line_index + 1 < component->layout_count)
                    ? component->layout_starts[line_index + 1] : text_len;
                while (end > start && text[end - 1] == '\n') {
                    end--;
                }
                if (end <= start) continue;

                int line_y = render_rect.y + line_index * line_stride - layer->scroll_offset;
                if (line_y + line_height < render_rect.y || line_y > render_rect.y + render_rect.h) {
                    continue;
                }

                text_component_render_text_segment(component, text, start, end, render_rect.x, line_y);
            }

            if (component->layout_count <= 0 && text_len > 0) {
                text_component_render_text_segment(component, text, 0, text_len,
                                                   render_rect.x, render_rect.y - layer->scroll_offset);
            }

            int total_text_height = layer->content_height;
            if (total_text_height <= render_rect.h) {
                layer->scroll_offset = 0;
            } else {
                if (layer->scroll_offset < 0) layer->scroll_offset = 0;
                int max_scroll_y = total_text_height - render_rect.h;
                if (layer->scroll_offset > max_scroll_y) layer->scroll_offset = max_scroll_y;
            }
        } else {
            int line_height = text_component_get_line_height(component);
            int text_len = (int)strlen(component->layer->text);
            int text_y = render_rect.y + (render_rect.h - line_height) / 2;
            text_component_render_text_segment(component, component->layer->text, 0, text_len,
                                               render_rect.x - component->scroll_x, text_y);
        }
    }
    
    // 如果组件可编辑，绘制光标
    if (component->editable && HAS_STATE(layer, LAYER_STATE_FOCUSED)) {
        // 确保光标位置在有效范围内
        int text_len = strlen(component->layer->text);
        if (component->cursor_pos < 0) component->cursor_pos = 0;
        if (component->cursor_pos > text_len) component->cursor_pos = text_len;
        
        // 获取行高
        int line_height = text_component_get_line_height(component);
        
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
    
    // 多行滚动条：仅当未启用 layer 通用垂直滚动条时自绘，避免与 render_vertical_scrollbar 重复
    if (component->multiline && layer->scrollable != 1 && layer->scrollable != 3) {
        int pad_top;
        int pad_right;
        int pad_bottom;
        int total_text_height = layer->content_height;
        int visible_height;
        Rect scrollbar_bg;
        text_component_get_padding(layer, &pad_top, &pad_right, &pad_bottom, NULL);
        visible_height = layer->rect.h - pad_top - pad_bottom;

        // 只有当文本总高度大于可见高度时才显示滚动条
        if (total_text_height > visible_height) {
            scrollbar_bg.x = layer->rect.x + layer->rect.w - pad_right - TEXT_INTERNAL_SCROLLBAR_WIDTH;
            scrollbar_bg.y = layer->rect.y + pad_top;
            scrollbar_bg.w = TEXT_INTERNAL_SCROLLBAR_WIDTH;
            scrollbar_bg.h = layer->rect.h - pad_top - pad_bottom;
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
    
    {
        int pad_top;
        int pad_right;
        int pad_bottom;
        int visible_height;
        text_component_get_padding(component->layer, &pad_top, &pad_right, &pad_bottom, NULL);
        visible_height = component->layer->rect.h - pad_top - pad_bottom;
        if (content_height <= visible_height) {
            component->layer->scroll_offset = 0;
        }
    }
}





