#include "theme.h"
#include "util.h"
#include "layer_update.h"
#include "layer_properties.h"
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
    
    // 解析样式规则（兼容 styles 与 rules 两种字段名）
    cJSON* styles = cJSON_GetObjectItem(json, "styles");
    if (!styles || !cJSON_IsArray(styles)) {
        styles = cJSON_GetObjectItem(json, "rules");
    }
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
    
    // 解析样式属性（兼容 style 与 properties 两种字段名）
    cJSON* style_obj = cJSON_GetObjectItem(json, "style");
    if (!style_obj || !cJSON_IsObject(style_obj)) {
        style_obj = cJSON_GetObjectItem(json, "properties");
    }

    // 与 style 同级的图层属性（scrollable、scrollbar 等）
    cJSON* props_obj = cJSON_CreateObject();
    if (props_obj) {
        cJSON* child = json->child;
        while (child) {
            if (child->string &&
                strcmp(child->string, "selector") != 0 &&
                strcmp(child->string, "style") != 0 &&
                strcmp(child->string, "properties") != 0) {
                cJSON_AddItemToObject(props_obj, child->string, cJSON_Duplicate(child, 1));
            }
            child = child->next;
        }
        if (props_obj->child) {
            rule->props_json = props_obj;
        } else {
            cJSON_Delete(props_obj);
        }
    }

    if ((!style_obj || !cJSON_IsObject(style_obj)) && !rule->props_json) {
        free(rule);
        return NULL;
    }

    if (style_obj && cJSON_IsObject(style_obj)) {
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

        rule->style_json = cJSON_Duplicate(style_obj, 1);
    }

    rule->next = NULL;
    
    return rule;
}

// 销毁规则
void theme_rule_destroy(ThemeRule* rule) {
    if (rule) {
        if (rule->style_json) {
            cJSON_Delete(rule->style_json);
        }
        if (rule->props_json) {
            cJSON_Delete(rule->props_json);
        }
        free(rule);
    }
}

#define THEME_COMPOUND_MARKER "."

static int theme_is_variant_separator(char c) {
    return c == ' ' || c == ',' || c == ';' || c == '\t';
}

static int theme_variant_contains(const char* variant_str, const char* modifier) {
    const char* p;

    if (!variant_str || !modifier || modifier[0] == '\0') {
        return 0;
    }

    p = variant_str;
    while (*p) {
        size_t len;

        while (theme_is_variant_separator(*p)) {
            p++;
        }
        if (*p == '\0') {
            break;
        }

        len = 0;
        while (p[len] && !theme_is_variant_separator(p[len])) {
            len++;
        }
        if (len > 0 && strlen(modifier) == len && strncmp(p, modifier, len) == 0) {
            return 1;
        }
        p += len;
    }

    return 0;
}

static int theme_variant_contains_all(const char* variant_str, const char* modifiers) {
    const char* p;

    if (!variant_str || variant_str[0] == '\0' || !modifiers || modifiers[0] != '.') {
        return 0;
    }

    p = modifiers + 1;
    if (*p == '\0') {
        return 0;
    }

    while (*p) {
        char modifier[32];
        int i = 0;

        while (*p && *p != '.' && i < (int)sizeof(modifier) - 1) {
            modifier[i++] = *p++;
        }
        modifier[i] = '\0';
        if (i == 0 || !theme_variant_contains(variant_str, modifier)) {
            return 0;
        }
        if (*p == '.') {
            p++;
        }
    }

    return 1;
}

static int theme_match_compound_selector(const char* selector, const char* id,
                                         const char* type, const char* variant) {
    const char* dot = strchr(selector, THEME_COMPOUND_MARKER[0]);
    if (!dot || dot == selector) {
        return 0;
    }

    if (!theme_variant_contains_all(variant, dot)) {
        return 0;
    }

    if (selector[0] == '#') {
        size_t id_len = (size_t)(dot - selector) - 1;
        if (id_len == 0) {
            return 0;
        }
        return strncmp(id, selector + 1, id_len) == 0 && id[id_len] == '\0';
    }

    {
        size_t type_len = (size_t)(dot - selector);
        return strncmp(type, selector, type_len) == 0 && type[type_len] == '\0';
    }
}

static int theme_rule_matches(ThemeRule* rule, Layer* layer, const char* id, const char* type) {
    if (!rule || !layer || !id || !type) {
        return 0;
    }

    if (rule->selector_type == THEME_SELECTOR_ID) {
        const char* selector_id = rule->selector + 1;
        const char* dot = strchr(selector_id, '.');
        if (dot) {
            return theme_match_compound_selector(rule->selector, id, type, layer->variant);
        }
        return strcmp(selector_id, id) == 0;
    }

    if (rule->selector_type == THEME_SELECTOR_TYPE) {
        return strcmp(rule->selector, type) == 0;
    }

    if (rule->selector_type == THEME_SELECTOR_COMPOUND) {
        return theme_match_compound_selector(rule->selector, id, type, layer->variant);
    }

    return 0;
}

static int theme_rule_specificity(ThemeRule* rule) {
    if (!rule) {
        return 0;
    }

    if (rule->selector_type == THEME_SELECTOR_ID) {
        int count = 1000;
        const char* dot = strchr(rule->selector, '.');
        while (dot) {
            count++;
            dot = strchr(dot + 1, '.');
        }
        return count;
    }

    if (rule->selector_type == THEME_SELECTOR_COMPOUND) {
        int count = 0;
        const char* dot = strchr(rule->selector, '.');
        while (dot) {
            count++;
            dot = strchr(dot + 1, '.');
        }
        return count;
    }

    return 0;
}

void theme_apply_component_style(Layer* layer, cJSON* style) {
    if (!layer || !style || !cJSON_IsObject(style)) {
        return;
    }

    if (layer->set_style) {
        layer->set_style(layer, style);
        return;
    }

    if (layer->set_property) {
        cJSON* item = style->child;
        while (item) {
            if (item->string) {
                layer->set_property(layer, item->string, item, 0);
            }
            item = item->next;
        }
        return;
    }

    layer_set_properties_from_json(layer, style, 0);
}

void theme_apply_layer_properties(Layer* layer, cJSON* props) {
    if (!layer || !props || !cJSON_IsObject(props)) {
        return;
    }
    layer_set_properties_from_json(layer, props, 0);
}

// 应用主题样式到图层
void theme_apply_to_layer(Theme* theme, Layer* layer, const char* id, const char* type) {
    if (!theme || !layer || !id || !type) {
        return;
    }

    ThemeRule* ordered[256];
    int rule_count = 0;
    for (ThemeRule* current = theme->rules; current && rule_count < 256; current = current->next) {
        ordered[rule_count++] = current;
    }

    int max_specificity = -1;
    for (int i = 0; i < rule_count; i++) {
        if (!theme_rule_matches(ordered[i], layer, id, type)) {
            continue;
        }
        int spec = theme_rule_specificity(ordered[i]);
        if (spec > max_specificity) {
            max_specificity = spec;
        }
    }

    if (max_specificity < 0) {
        return;
    }

    printf("[Theme] Applying theme to layer id='%s', type='%s', specificity=%d\n",
           id, type, max_specificity);
    for (int i = rule_count - 1; i >= 0; i--) {
        ThemeRule* current = ordered[i];
        if (!theme_rule_matches(current, layer, id, type)) {
            continue;
        }
        if (theme_rule_specificity(current) != max_specificity) {
            continue;
        }

        printf("[Theme] Merging style for layer id='%s' selector='%s'\n",
               id, current->selector);
        theme_merge_style(current, layer);
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
        mark_layer_dirty(layer, DIRTY_COLOR);
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
        mark_layer_dirty(layer, DIRTY_STYLE);
    }
    
    // 字体大小
    if (rule->font_size > 0) {
        if (!layer->font) {
            layer->font = (Font*)malloc(sizeof(Font));
            memset(layer->font, 0, sizeof(Font));
            strcpy(layer->font->path, "Roboto-Regular.ttf");
            strcpy(layer->font->weight, "normal");
            layer->font->size = 16;
        } else if (layer->parent && layer->parent->font == layer->font) {
            Font* shared = layer->font;
            layer->font = (Font*)malloc(sizeof(Font));
            memcpy(layer->font, shared, sizeof(Font));
            layer->font->default_font = NULL;
        }
        if (layer->font->size != rule->font_size) {
            layer->font->size = rule->font_size;
            layer->font->default_font = NULL;
            mark_layer_dirty(layer, DIRTY_TEXT | DIRTY_LAYOUT);
        }
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

    if (rule->style_json) {
        theme_apply_component_style(layer, rule->style_json);
    }

    if (rule->props_json) {
        theme_apply_layer_properties(layer, rule->props_json);
    }
}

// 解析选择器类型
ThemeSelectorType theme_parse_selector_type(const char* selector) {
    if (!selector || selector[0] == '\0') {
        return THEME_SELECTOR_TYPE;  // 默认按类型处理
    }
    
    // 如果以#开头，是ID选择器或 #id.modifier 复合选择器
    if (selector[0] == '#') {
        if (strchr(selector + 1, '.') != NULL) {
            return THEME_SELECTOR_COMPOUND;
        }
        return THEME_SELECTOR_ID;
    }

    // Type.modifier 复合选择器
    if (strchr(selector, '.') != NULL) {
        return THEME_SELECTOR_COMPOUND;
    }
    
    // 否则按类型选择器处理
    return THEME_SELECTOR_TYPE;
}
