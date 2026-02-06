#include "layer_properties.h"
#include "layer.h"
#include "layer_update.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ====================== 属性处理器 ======================

// 属性处理器函数类型
typedef int (*PropertyHandler)(Layer* layer, cJSON* value, int is_creating);

// 属性处理器结构
typedef struct {
    const char* key;
    PropertyHandler handler;
} PropertyHandlerEntry;

// 字符串属性处理器
static int handle_text(Layer* layer, cJSON* value, int is_creating) {
    if (!cJSON_IsString(value)) return 0;
    if (is_creating) {
        strncpy(layer->text, value->valuestring, sizeof(layer->text) - 1);
        layer->text[sizeof(layer->text) - 1] = '\0';
    } else {
        yui_set_text(layer, value->valuestring);
    }
    return 1;
}

static int handle_label(Layer* layer, cJSON* value, int is_creating) {
    if (!cJSON_IsString(value)) return 0;
    layer_set_label(layer, value->valuestring);
    if (!is_creating) {
        mark_layer_dirty(layer, DIRTY_TEXT);
    }
    return 1;
}

static int handle_id(Layer* layer, cJSON* value, int is_creating) {
    if (!cJSON_IsString(value)) return 0;
    strncpy(layer->id, value->valuestring, sizeof(layer->id) - 1);
    layer->id[sizeof(layer->id) - 1] = '\0';
    return 1;
}

static int handle_color(Layer* layer, cJSON* value, int is_creating) {
    if (!cJSON_IsString(value)) return 0;
    Color color;
    parse_color(value->valuestring, &color);
    layer->color = color;
    if (!is_creating) {
        mark_layer_dirty(layer, DIRTY_COLOR);
    }
    return 1;
}

static int handle_bg_color(Layer* layer, cJSON* value, int is_creating) {
    if (!cJSON_IsString(value)) return 0;
    if (is_creating) {
        Color color;
        parse_color(value->valuestring, &color);
        layer->bg_color = color;
        mark_layer_dirty(layer, DIRTY_COLOR);
    } else {
        yui_set_bg_color(layer, value->valuestring);
    }
    return 1;
}

// 字体相关处理器
static int handle_font(Layer* layer, cJSON* value, int is_creating) {
    if (!cJSON_IsString(value)) return 0;
    if (!layer->font && is_creating) {
        layer->font = malloc(sizeof(Font));
        layer->font->default_font = NULL;
        layer->font->size = 16;
        strcpy(layer->font->weight, "normal");
    }
    if (layer->font) {
        strncpy(layer->font->path, value->valuestring, MAX_PATH - 1);
        layer->font->path[MAX_PATH - 1] = '\0';
        if (!is_creating) {
            mark_layer_dirty(layer, DIRTY_TEXT);
        }
    }
    return 1;
}

static int handle_font_size(Layer* layer, cJSON* value, int is_creating) {
    if (!cJSON_IsNumber(value)) return 0;
    if (!layer->font && is_creating) {
        layer->font = malloc(sizeof(Font));
        layer->font->default_font = NULL;
        strcpy(layer->font->path, "Roboto-Regular.ttf");
        strcpy(layer->font->weight, "normal");
    }
    if (layer->font) {
        layer->font->size = value->valueint;
        if (!is_creating) {
            mark_layer_dirty(layer, DIRTY_TEXT | DIRTY_LAYOUT);
        }
    }
    return 1;
}

static int handle_font_weight(Layer* layer, cJSON* value, int is_creating) {
    if (!cJSON_IsString(value)) return 0;
    if (!layer->font && is_creating) {
        layer->font = malloc(sizeof(Font));
        layer->font->default_font = NULL;
        strcpy(layer->font->path, "Roboto-Regular.ttf");
        layer->font->size = 16;
    }
    if (layer->font) {
        strncpy(layer->font->weight, value->valuestring, sizeof(layer->font->weight) - 1);
        layer->font->weight[sizeof(layer->font->weight) - 1] = '\0';
        if (!is_creating) {
            mark_layer_dirty(layer, DIRTY_TEXT);
        }
    }
    return 1;
}

static int handle_border_radius(Layer* layer, cJSON* value, int is_creating) {
    if (!cJSON_IsNumber(value)) return 0;
    layer->radius = value->valueint;
    if (!is_creating) {
        mark_layer_dirty(layer, DIRTY_STYLE);
    }
    return 1;
}

static int handle_opacity(Layer* layer, cJSON* value, int is_creating) {
    if (!cJSON_IsNumber(value)) return 0;
    int opacity = value->valueint;
    if (opacity < 0) opacity = 0;
    if (opacity > 255) opacity = 255;
    layer->bg_color.a = (uint8_t)opacity;
    if (!is_creating) {
        mark_layer_dirty(layer, DIRTY_COLOR);
    }
    return 1;
}

// 数组属性处理器
static int handle_size(Layer* layer, cJSON* value, int is_creating) {
    if (!cJSON_IsArray(value)) return 0;
    int w, h;
    if (parse_int_array(value, &w, &h) == 0) {
        layer->rect.w = w;
        layer->rect.h = h;
        layer->fixed_width = w;
        layer->fixed_height = h;
        if (!is_creating) {
            mark_layer_dirty(layer, DIRTY_RECT | DIRTY_LAYOUT);
        }
    }
    return 1;
}

static int handle_position(Layer* layer, cJSON* value, int is_creating) {
    if (!cJSON_IsArray(value)) return 0;
    int x, y;
    if (parse_int_array(value, &x, &y) == 0) {
        layer->rect.x = x;
        layer->rect.y = y;
        if (!is_creating) {
            mark_layer_dirty(layer, DIRTY_RECT);
        }
    }
    return 1;
}

static int handle_padding(Layer* layer, cJSON* value, int is_creating) {
    if (!cJSON_IsArray(value)) return 0;
    if (!layer->layout_manager) return 0;
    
    int count = cJSON_GetArraySize(value);
    if (count >= 4) {
        layer->layout_manager->padding[0] = cJSON_GetArrayItem(value, 0)->valueint;  // top
        layer->layout_manager->padding[1] = cJSON_GetArrayItem(value, 1)->valueint;  // right
        layer->layout_manager->padding[2] = cJSON_GetArrayItem(value, 2)->valueint;  // bottom
        layer->layout_manager->padding[3] = cJSON_GetArrayItem(value, 3)->valueint;  // left
    } else if (count == 2) {
        int vertical = cJSON_GetArrayItem(value, 0)->valueint;
        int horizontal = cJSON_GetArrayItem(value, 1)->valueint;
        layer->layout_manager->padding[0] = layer->layout_manager->padding[2] = vertical;
        layer->layout_manager->padding[1] = layer->layout_manager->padding[3] = horizontal;
    } else if (count == 1) {
        int all = cJSON_GetArrayItem(value, 0)->valueint;
        layer->layout_manager->padding[0] = layer->layout_manager->padding[2] = all;
        layer->layout_manager->padding[1] = layer->layout_manager->padding[3] = all;
    }
    
    if (!is_creating) {
        mark_layer_dirty(layer, DIRTY_LAYOUT);
    }
    return 1;
}

// 布尔属性处理器
static int handle_visible(Layer* layer, cJSON* value, int is_creating) {
    if (!cJSON_IsBool(value)) return 0;
    if (is_creating) {
        layer->visible = cJSON_IsTrue(value) ? 1 : 0;
    } else {
        yui_set_visible(layer, cJSON_IsTrue(value));
    }
    return 1;
}

static int handle_enabled(Layer* layer, cJSON* value, int is_creating) {
    if (!cJSON_IsBool(value)) return 0;
    // enabled 属性暂时不支持（Layer 结构中没有此字段）
    // 可以通过 state 字段来控制是否可用
    return 1;
}

static int handle_focusable(Layer* layer, cJSON* value, int is_creating) {
    if (!cJSON_IsBool(value)) return 0;
    layer->focusable = cJSON_IsTrue(value) ? 1 : 0;
    return 1;
}

static int handle_scrollable(Layer* layer, cJSON* value, int is_creating) {
    if (cJSON_IsNumber(value)) {
        layer->scrollable = value->valueint;
    } else if (cJSON_IsBool(value)) {
        layer->scrollable = cJSON_IsTrue(value) ? 1 : 0;
    } else {
        return 0;
    }
    if (!is_creating) {
        mark_layer_dirty(layer, DIRTY_LAYOUT);
    }
    return 1;
}

// 尺寸属性处理器
static int handle_width(Layer* layer, cJSON* value, int is_creating) {
    if (!cJSON_IsNumber(value)) return 0;
    layer->fixed_width = value->valueint;
    layer->rect.w = layer->fixed_width;
    if (!is_creating) {
        mark_layer_dirty(layer, DIRTY_RECT | DIRTY_LAYOUT);
    }
    return 1;
}

static int handle_height(Layer* layer, cJSON* value, int is_creating) {
    if (!cJSON_IsNumber(value)) return 0;
    layer->fixed_height = value->valueint;
    layer->rect.h = layer->fixed_height;
    if (!is_creating) {
        mark_layer_dirty(layer, DIRTY_RECT | DIRTY_LAYOUT);
    }
    return 1;
}

static int handle_flex(Layer* layer, cJSON* value, int is_creating) {
    if (!cJSON_IsNumber(value)) return 0;
    layer->flex_ratio = (float)value->valuedouble;
    if (!is_creating) {
        mark_layer_dirty(layer, DIRTY_LAYOUT);
    }
    return 1;
}

// 其他属性处理器
static int handle_rotation(Layer* layer, cJSON* value, int is_creating) {
    if (!cJSON_IsNumber(value)) return 0;
    layer->rotation = value->valueint;
    if (!is_creating) {
        mark_layer_dirty(layer, DIRTY_RECT);
    }
    return 1;
}

static int handle_source(Layer* layer, cJSON* value, int is_creating) {
    if (!cJSON_IsString(value)) return 0;
    strncpy(layer->source, value->valuestring, sizeof(layer->source) - 1);
    layer->source[sizeof(layer->source) - 1] = '\0';
    if (!is_creating) {
        mark_layer_dirty(layer, DIRTY_STYLE);
    }
    return 1;
}

// 属性处理器查找表
static const PropertyHandlerEntry property_handlers[] = {
    // 基础属性
    {"id", handle_id},
    
    // 文本属性
    {"text", handle_text},
    {"label", handle_label},
    
    // 颜色属性
    {"color", handle_color},
    {"bgColor", handle_bg_color},
    {"opacity", handle_opacity},
    
    // 字体属性
    {"font", handle_font},
    {"fontSize", handle_font_size},
    {"fontWeight", handle_font_weight},
    
    // 样式属性
    {"borderRadius", handle_border_radius},
    {"source", handle_source},
    
    // 尺寸和位置属性
    {"size", handle_size},
    {"position", handle_position},
    {"width", handle_width},
    {"height", handle_height},
    {"padding", handle_padding},
    
    // 布局属性
    {"flex", handle_flex},
    {"rotation", handle_rotation},
    
    // 状态属性
    {"visible", handle_visible},
    {"enabled", handle_enabled},
    {"focusable", handle_focusable},
    {"scrollable", handle_scrollable},
    
    // 结束标记
    {NULL, NULL}
};

// ====================== 公共 API ======================

int layer_set_property_from_json(Layer* layer, const char* key, cJSON* value, int is_creating) {
    if (!layer || !key || !value) {
        return 0;
    }
    
    // 特殊处理：null 值表示删除/隐藏
    if (cJSON_IsNull(value)) {
        if (strcmp(key, "visible") == 0) {
            if (is_creating) {
                layer->visible = 0;
            } else {
                yui_set_visible(layer, 0);
            }
            return 1;
        }
        return 0;
    }
    
    // 在查找表中查找处理器
    for (int i = 0; property_handlers[i].key != NULL; i++) {
        if (strcmp(key, property_handlers[i].key) == 0) {
            return property_handlers[i].handler(layer, value, is_creating);
        }
    }
    
    // 未找到处理器
    return 0;
}

int layer_set_properties_from_json(Layer* layer, cJSON* json_obj, int is_creating) {
    if (!layer || !json_obj || !cJSON_IsObject(json_obj)) {
        return 0;
    }
    
    int count = 0;
    cJSON* item = NULL;
    
    cJSON_ArrayForEach(item, json_obj) {
        const char* key = item->string;
        if (key && layer_set_property_from_json(layer, key, item, is_creating)) {
            count++;
        }
    }
    
    return count;
}

// 辅助函数：创建颜色 JSON 对象
static cJSON* create_color_json(Color color) {
    char color_str[20];
    snprintf(color_str, sizeof(color_str), "#%02x%02x%02x%02x", 
             color.r, color.g, color.b, color.a);
    return cJSON_CreateString(color_str);
}

// 辅助函数：创建矩形 JSON 对象
static cJSON* create_rect_json(Rect rect) {
    cJSON* array = cJSON_CreateArray();
    cJSON_AddItemToArray(array, cJSON_CreateNumber(rect.x));
    cJSON_AddItemToArray(array, cJSON_CreateNumber(rect.y));
    cJSON_AddItemToArray(array, cJSON_CreateNumber(rect.w));
    cJSON_AddItemToArray(array, cJSON_CreateNumber(rect.h));
    return array;
}

// 辅助函数：创建位置 JSON 对象
static cJSON* create_position_json(Rect rect) {
    cJSON* array = cJSON_CreateArray();
    cJSON_AddItemToArray(array, cJSON_CreateNumber(rect.x));
    cJSON_AddItemToArray(array, cJSON_CreateNumber(rect.y));
    return array;
}

// 辅助函数：创建尺寸 JSON 对象
static cJSON* create_size_json(Rect rect) {
    cJSON* array = cJSON_CreateArray();
    cJSON_AddItemToArray(array, cJSON_CreateNumber(rect.w));
    cJSON_AddItemToArray(array, cJSON_CreateNumber(rect.h));
    return array;
}

// 辅助函数：创建内边距 JSON 对象
static cJSON* create_padding_json(Layer* layer) {
    if (!layer->layout_manager) {
        return cJSON_CreateNull();
    }
    
    cJSON* array = cJSON_CreateArray();
    cJSON_AddItemToArray(array, cJSON_CreateNumber(layer->layout_manager->padding[0]));  // top
    cJSON_AddItemToArray(array, cJSON_CreateNumber(layer->layout_manager->padding[1]));  // right
    cJSON_AddItemToArray(array, cJSON_CreateNumber(layer->layout_manager->padding[2]));  // bottom
    cJSON_AddItemToArray(array, cJSON_CreateNumber(layer->layout_manager->padding[3]));  // left
    return array;
}

// 辅助函数：创建字体 JSON 对象
static cJSON* create_font_json(Layer* layer) {
    if (!layer->font) {
        return cJSON_CreateNull();
    }
    
    cJSON* font_obj = cJSON_CreateObject();
    cJSON_AddStringToObject(font_obj, "path", layer->font->path);
    cJSON_AddNumberToObject(font_obj, "size", layer->font->size);
    cJSON_AddStringToObject(font_obj, "weight", layer->font->weight);
    
    return font_obj;
}

cJSON* layer_get_property_as_json(Layer* layer, const char* key) {
    if (!layer || !key) {
        return NULL;
    }
    
    // 首先尝试使用组件的通用属性获取函数
    if (layer->component && layer->get_property) {
        cJSON* value = layer->get_property(layer, key);
        if (value) {
            return value;
        }
    }
    
    // 如果组件没有返回值，使用默认的层属性处理
    // 根据属性名返回对应的 JSON 值
    if (strcmp(key, "id") == 0) {
        return cJSON_CreateString(layer->id);
    }
    else if (strcmp(key, "text") == 0) {
        return cJSON_CreateString(layer->text);
    }
    else if (strcmp(key, "label") == 0) {
        return cJSON_CreateString(layer->label);
    }
    else if (strcmp(key, "color") == 0) {
        return create_color_json(layer->color);
    }
    else if (strcmp(key, "bgColor") == 0) {
        return create_color_json(layer->bg_color);
    }
    else if (strcmp(key, "opacity") == 0) {
        return cJSON_CreateNumber(layer->bg_color.a);
    }
    else if (strcmp(key, "font") == 0) {
        return create_font_json(layer);
    }
    else if (strcmp(key, "fontSize") == 0) {
        return layer->font ? cJSON_CreateNumber(layer->font->size) : cJSON_CreateNull();
    }
    else if (strcmp(key, "fontWeight") == 0) {
        return layer->font ? cJSON_CreateString(layer->font->weight) : cJSON_CreateNull();
    }
    else if (strcmp(key, "borderRadius") == 0) {
        return cJSON_CreateNumber(layer->radius);
    }
    else if (strcmp(key, "source") == 0) {
        return cJSON_CreateString(layer->source);
    }
    else if (strcmp(key, "size") == 0) {
        return create_size_json(layer->rect);
    }
    else if (strcmp(key, "position") == 0) {
        return create_position_json(layer->rect);
    }
    else if (strcmp(key, "width") == 0) {
        return cJSON_CreateNumber(layer->rect.w);
    }
    else if (strcmp(key, "height") == 0) {
        return cJSON_CreateNumber(layer->rect.h);
    }
    else if (strcmp(key, "padding") == 0) {
        return create_padding_json(layer);
    }
    else if (strcmp(key, "flex") == 0) {
        return cJSON_CreateNumber(layer->flex_ratio);
    }
    else if (strcmp(key, "rotation") == 0) {
        return cJSON_CreateNumber(layer->rotation);
    }
    else if (strcmp(key, "visible") == 0) {
        return cJSON_CreateBool(layer->visible);
    }
    else if (strcmp(key, "focusable") == 0) {
        return cJSON_CreateBool(layer->focusable);
    }
    else if (strcmp(key, "scrollable") == 0) {
        return cJSON_CreateNumber(layer->scrollable);
    }
    else if (strcmp(key, "rect") == 0) {
        return create_rect_json(layer->rect);
    }
    else if (strcmp(key, "value") == 0) {
        // 对于大多数组件，value 可以从 text 属性获取
        if (layer->text && strlen(layer->text) > 0) {
            return cJSON_CreateString(layer->text);
        }
        
        // 如果没有 text 属性，返回 null
        return cJSON_CreateNull();
    }
    
    // 未知属性，返回 NULL
    return NULL;
}

cJSON* layer_get_properties_as_json(Layer* layer, const char** keys, int count) {
    if (!layer || !keys || count <= 0) {
        return NULL;
    }
    
    cJSON* result = cJSON_CreateObject();
    if (!result) {
        return NULL;
    }
    
    for (int i = 0; i < count; i++) {
        const char* key = keys[i];
        if (!key) continue;
        
        cJSON* value = layer_get_property_as_json(layer, key);
        if (value) {
            cJSON_AddItemToObject(result, key, value);
        }
    }
    
    return result;
}

cJSON* layer_get_all_properties_as_json(Layer* layer) {
    if (!layer) {
        return NULL;
    }
    
    cJSON* result = cJSON_CreateObject();
    if (!result) {
        return NULL;
    }
    
    // 添加所有支持的属性
    cJSON* value;
    
    // 基础属性
    cJSON_AddStringToObject(result, "id", layer->id);
    
    // 文本属性
    if (layer->text && strlen(layer->text) > 0) {
        cJSON_AddStringToObject(result, "text", layer->text);
    }
    if (layer->label && strlen(layer->label) > 0) {
        cJSON_AddStringToObject(result, "label", layer->label);
    }
    
    // 颜色属性
    cJSON_AddItemToObject(result, "color", create_color_json(layer->color));
    cJSON_AddItemToObject(result, "bgColor", create_color_json(layer->bg_color));
    cJSON_AddNumberToObject(result, "opacity", layer->bg_color.a);
    
    // 字体属性
    if (layer->font) {
        cJSON_AddItemToObject(result, "font", create_font_json(layer));
    }
    
    // 样式属性
    cJSON_AddNumberToObject(result, "borderRadius", layer->radius);
    if (layer->source && strlen(layer->source) > 0) {
        cJSON_AddStringToObject(result, "source", layer->source);
    }
    
    // 尺寸和位置属性
    cJSON_AddItemToObject(result, "size", create_size_json(layer->rect));
    cJSON_AddItemToObject(result, "position", create_position_json(layer->rect));
    cJSON_AddNumberToObject(result, "width", layer->rect.w);
    cJSON_AddNumberToObject(result, "height", layer->rect.h);
    
    if (layer->layout_manager) {
        cJSON_AddItemToObject(result, "padding", create_padding_json(layer));
    }
    
    // 布局属性
    cJSON_AddNumberToObject(result, "flex", layer->flex_ratio);
    cJSON_AddNumberToObject(result, "rotation", layer->rotation);
    
    // 状态属性
    cJSON_AddBoolToObject(result, "visible", layer->visible);
    cJSON_AddBoolToObject(result, "focusable", layer->focusable);
    cJSON_AddNumberToObject(result, "scrollable", layer->scrollable);
    
    return result;
}
