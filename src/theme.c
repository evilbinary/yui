#include "theme.h"
#include "util.h"
#include "layer_update.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define printf

// 创建主题对象
Theme* theme_create(const char* name, const char* version) {
    Theme* theme = (Theme*)malloc(sizeof(Theme));
    if (!theme) {
        return NULL;
    }
    
    memset(theme, 0, sizeof(Theme));
    
    if (name) {
        strncpy(theme->name, name, sizeof(theme->name) - 1);
        theme->name[sizeof(theme->name) - 1] = '\0';
    }
    
    if (version) {
        strncpy(theme->version, version, sizeof(theme->version) - 1);
        theme->version[sizeof(theme->version) - 1] = '\0';
    }
    
    theme->rules = NULL;
    
    return theme;
}

// 销毁主题对象
void theme_destroy(Theme* theme) {
    if (!theme) {
        return;
    }
    
    // 销毁所有规则
    ThemeRule* current = theme->rules;
    while (current) {
        ThemeRule* next = current->next;
        theme_rule_destroy(current);
        current = next;
    }
    
    free(theme);
}

// 从JSON文件加载主题
Theme* theme_load_from_file(const char* json_path) {
    if (!json_path) {
        return NULL;
    }
    
    // 读取JSON文件
    FILE* file = fopen(json_path, "r");
    if (!file) {
        printf("Failed to open theme file: %s\n", json_path);
        return NULL;
    }
    
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    char* json_str = (char*)malloc(file_size + 1);
    if (!json_str) {
        fclose(file);
        return NULL;
    }
    
    size_t read_size = fread(json_str, 1, file_size, file);
    json_str[read_size] = '\0';
    fclose(file);
    
    // 解析JSON
    cJSON* json = cJSON_Parse(json_str);
    free(json_str);
    
    if (!json) {
        printf("Failed to parse theme JSON\n");
        return NULL;
    }
    
    Theme* theme = theme_load_from_json(json);
    cJSON_Delete(json);
    
    return theme;
}

// 从JSON对象加载主题
Theme* theme_load_from_json(cJSON* json) {
    if (!json || !cJSON_IsObject(json)) {
        return NULL;
    }
    
    // 获取主题名称和版本
    const char* name = "";
    const char* version = "1.0";
    
    cJSON* name_obj = cJSON_GetObjectItem(json, "name");
    if (name_obj && cJSON_IsString(name_obj)) {
        name = name_obj->valuestring;
    }
    
    cJSON* version_obj = cJSON_GetObjectItem(json, "version");
    if (version_obj && cJSON_IsString(version_obj)) {
        version = version_obj->valuestring;
    }
    
    // 创建主题对象
    Theme* theme = theme_create(name, version);
    if (!theme) {
        return NULL;
    }
    
    // 解析样式规则
    cJSON* styles = cJSON_GetObjectItem(json, "styles");
    if (styles && cJSON_IsArray(styles)) {
        int rule_count = cJSON_GetArraySize(styles);
        
        for (int i = 0; i < rule_count; i++) {
            cJSON* style_obj = cJSON_GetArrayItem(styles, i);
            ThemeRule* rule = theme_rule_create_from_json(style_obj);
            
            if (rule) {
                theme_add_rule(theme, rule);
            }
        }
    }
    
    return theme;
}

// 添加规则到主题
void theme_add_rule(Theme* theme, ThemeRule* rule) {
    if (!theme || !rule) {
        return;
    }
    
    // 插入到链表头部（后添加的规则优先级更高）
    rule->next = theme->rules;
    theme->rules = rule;
}

// 从JSON创建规则
ThemeRule* theme_rule_create_from_json(cJSON* json) {
    if (!json || !cJSON_IsObject(json)) {
        return NULL;
    }
    
    ThemeRule* rule = (ThemeRule*)malloc(sizeof(ThemeRule));
    if (!rule) {
        return NULL;
    }
    
    memset(rule, 0, sizeof(ThemeRule));
    
    // 解析选择器
    cJSON* selector_obj = cJSON_GetObjectItem(json, "selector");
    if (!selector_obj || !cJSON_IsString(selector_obj)) {
        free(rule);
        return NULL;
    }
    
    const char* selector = selector_obj->valuestring;
    strncpy(rule->selector, selector, sizeof(rule->selector) - 1);
    rule->selector[sizeof(rule->selector) - 1] = '\0';
    
    // 解析选择器类型
    rule->selector_type = theme_parse_selector_type(selector);
    
    // 解析样式属性
    cJSON* style_obj = cJSON_GetObjectItem(json, "style");
    if (!style_obj || !cJSON_IsObject(style_obj)) {
        free(rule);
        return NULL;
    }
    
    // 颜色
    cJSON* color_obj = cJSON_GetObjectItem(style_obj, "color");
    if (color_obj && cJSON_IsString(color_obj)) {
        printf("[Theme] Parsing color: %s\n", color_obj->valuestring);
        parse_color(color_obj->valuestring, &rule->color);
        printf("[Theme] Parsed color RGBA: %d, %d, %d, %d\n", rule->color.r, rule->color.g, rule->color.b, rule->color.a);
    }
    
    // 背景颜色
    cJSON* bg_color_obj = cJSON_GetObjectItem(style_obj, "bgColor");
    if (bg_color_obj && cJSON_IsString(bg_color_obj)) {
        printf("[Theme] Parsing bgColor: %s\n", bg_color_obj->valuestring);
        parse_color(bg_color_obj->valuestring, &rule->bg_color);
        printf("[Theme] Parsed bgColor RGBA: %d, %d, %d, %d\n", rule->bg_color.r, rule->bg_color.g, rule->bg_color.b, rule->bg_color.a);
    }
    
    // 字体大小
    cJSON* font_size_obj = cJSON_GetObjectItem(style_obj, "fontSize");
    if (font_size_obj && cJSON_IsNumber(font_size_obj)) {
        rule->font_size = font_size_obj->valueint;
    }
    
    // 字体粗细
    cJSON* font_weight_obj = cJSON_GetObjectItem(style_obj, "fontWeight");
    if (font_weight_obj && cJSON_IsString(font_weight_obj)) {
        strncpy(rule->font_weight, font_weight_obj->valuestring, sizeof(rule->font_weight) - 1);
        rule->font_weight[sizeof(rule->font_weight) - 1] = '\0';
    }
    
    // 圆角半径
    cJSON* radius_obj = cJSON_GetObjectItem(style_obj, "radius");
    if (!radius_obj) {
        radius_obj = cJSON_GetObjectItem(style_obj, "borderRadius");
    }
    if (radius_obj && cJSON_IsNumber(radius_obj)) {
        rule->radius = radius_obj->valueint;
    }
    
    // 边框宽度
    cJSON* border_width_obj = cJSON_GetObjectItem(style_obj, "borderWidth");
    if (border_width_obj && cJSON_IsNumber(border_width_obj)) {
        rule->border_width = border_width_obj->valueint;
    }
    
    // 边框颜色
    cJSON* border_color_obj = cJSON_GetObjectItem(style_obj, "borderColor");
    if (border_color_obj && cJSON_IsString(border_color_obj)) {
        parse_color(border_color_obj->valuestring, &rule->border_color);
    }
    
    // 间距
    cJSON* spacing_obj = cJSON_GetObjectItem(style_obj, "spacing");
    if (spacing_obj && cJSON_IsNumber(spacing_obj)) {
        rule->spacing = spacing_obj->valueint;
    }
    
    // 内边距
    cJSON* padding_obj = cJSON_GetObjectItem(style_obj, "padding");
    if (padding_obj && cJSON_IsArray(padding_obj)) {
        int padding_size = cJSON_GetArraySize(padding_obj);
        for (int i = 0; i < padding_size && i < 4; i++) {
            cJSON* pad = cJSON_GetArrayItem(padding_obj, i);
            if (pad && cJSON_IsNumber(pad)) {
                rule->padding[i] = pad->valueint;
            }
        }
    }
    
    // 透明度
    cJSON* opacity_obj = cJSON_GetObjectItem(style_obj, "opacity");
    if (opacity_obj && cJSON_IsNumber(opacity_obj)) {
        rule->opacity = opacity_obj->valueint;
    }
    
    // 宽度
    cJSON* width_obj = cJSON_GetObjectItem(style_obj, "width");
    if (width_obj && cJSON_IsNumber(width_obj)) {
        rule->width = width_obj->valueint;
    }
    
    // 高度
    cJSON* height_obj = cJSON_GetObjectItem(style_obj, "height");
    if (height_obj && cJSON_IsNumber(height_obj)) {
        rule->height = height_obj->valueint;
    }
    
    rule->next = NULL;
    
    return rule;
}

// 销毁规则
void theme_rule_destroy(ThemeRule* rule) {
    if (rule) {
        free(rule);
    }
}

// 应用主题样式到图层
void theme_apply_to_layer(Theme* theme, Layer* layer, const char* id, const char* type) {
    if (!theme || !layer || !id || !type) {
        return;
    }
    
    // 遍历所有规则，应用匹配的规则
    ThemeRule* current = theme->rules;
    printf("[Theme] Applying theme to layer id='%s', type='%s'\n", id, type);
    while (current) {
        int should_apply = 0;
        
        printf("[Theme] Checking rule selector='%s', type=%d\n", current->selector, current->selector_type);
        
        // 检查选择器是否匹配
        if (current->selector_type == THEME_SELECTOR_ID) {
            // ID选择器：格式为 "#id"
            const char* selector_id = current->selector + 1;  // 跳过#
            if (strcmp(selector_id, id) == 0) {
                should_apply = 1;
            }
        } else if (current->selector_type == THEME_SELECTOR_TYPE) {
            // 类型选择器：直接比较
            printf("[Theme] Comparing selector '%s' with type '%s'\n", current->selector, type);
            if (strcmp(current->selector, type) == 0) {
                should_apply = 1;
                printf("[Theme] MATCH! Applying rule to layer id='%s'\n", id);
            }
        }
        
        // 如果匹配，应用样式
        if (should_apply) {
            printf("[Theme] Merging style for layer id='%s'\n", id);
            theme_merge_style(current, layer);
        }
        
        current = current->next;
    }
}

// 合并样式到图层
void theme_merge_style(ThemeRule* rule, Layer* layer) {
    if (!rule || !layer) {
        return;
    }
    
    printf("[Theme] Merging style: rule bg_color RGBA=%d,%d,%d,%d, layer id='%s'\n", 
           rule->bg_color.r, rule->bg_color.g, rule->bg_color.b, rule->bg_color.a, layer->id);
    
    // 颜色（主题总是覆盖图层文字颜色）
    if (rule->color.a > 0) {
        printf("[Theme] Applying color to layer id='%s' (rule color RGBA=%d,%d,%d,%d)\n", 
               layer->id, rule->color.r, rule->color.g, rule->color.b, rule->color.a);
        layer->color = rule->color;
    }
    
    // 背景颜色（主题总是覆盖图层背景色）
    if (rule->bg_color.a > 0) {
        printf("[Theme] Applying bg_color to layer id='%s' (rule bg_color RGBA=%d,%d,%d,%d, old bg_color RGBA=%d,%d,%d,%d)\n", 
               layer->id, rule->bg_color.r, rule->bg_color.g, rule->bg_color.b, rule->bg_color.a,
               layer->bg_color.r, layer->bg_color.g, layer->bg_color.b, layer->bg_color.a);
        layer->bg_color = rule->bg_color;
        printf("[Theme] Layer bg_color set to RGBA=%d,%d,%d,%d\n", 
               layer->bg_color.r, layer->bg_color.g, layer->bg_color.b, layer->bg_color.a);
        
        // 标记图层需要重绘
        mark_layer_dirty(layer, DIRTY_COLOR);
        printf("[Theme] Layer marked dirty for redraw: id='%s'\n", layer->id);
    }
    
    // 圆角半径
    if (rule->radius > 0) {
        layer->radius = rule->radius;
    }
    
    // 字体大小
    if (rule->font_size > 0 && layer->font) {
        // TODO: 需要重新加载字体或调整字体大小
    }
    
    // 边框颜色
    if (rule->border_color.a > 0) {
        // TODO: 需要添加边框支持
    }
    
    // 间距（应用到布局管理器）
    if (rule->spacing > 0 && layer->layout_manager) {
        layer->layout_manager->spacing = rule->spacing;
    }
    
    // 内边距（应用到布局管理器）
    if (rule->padding[0] > 0 || rule->padding[1] > 0 || 
        rule->padding[2] > 0 || rule->padding[3] > 0) {
        if (layer->layout_manager) {
            layer->layout_manager->padding[0] = rule->padding[0];
            layer->layout_manager->padding[1] = rule->padding[1];
            layer->layout_manager->padding[2] = rule->padding[2];
            layer->layout_manager->padding[3] = rule->padding[3];
        }
    }
    
    // 透明度
    if (rule->opacity > 0 && rule->opacity < 255) {
        layer->color.a = rule->opacity;
        layer->bg_color.a = rule->opacity;
    }
}

// 解析选择器类型
ThemeSelectorType theme_parse_selector_type(const char* selector) {
    if (!selector || selector[0] == '\0') {
        return THEME_SELECTOR_TYPE;  // 默认按类型处理
    }
    
    // 如果以#开头，是ID选择器
    if (selector[0] == '#') {
        return THEME_SELECTOR_ID;
    }
    
    // 否则按类型选择器处理
    return THEME_SELECTOR_TYPE;
}
