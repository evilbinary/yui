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
    if (parse_color_string(value->valuestring, &color) == 0) {
        layer->color = color;
        if (!is_creating) {
            mark_layer_dirty(layer, DIRTY_COLOR);
        }
    }
    return 1;
}

static int handle_bg_color(Layer* layer, cJSON* value, int is_creating) {
    if (!cJSON_IsString(value)) return 0;
    if (is_creating) {
        Color color;
        if (parse_color_string(value->valuestring, &color) == 0) {
            layer->bg_color = color;
        }
    } else {
        yui_set_bg_color(layer, value->valuestring);
    }
    return 1;
}

// 数值属性处理器
static int handle_font_size(Layer* layer, cJSON* value, int is_creating) {
    if (!cJSON_IsNumber(value)) return 0;
    if (layer->font) {
        layer->font->size = value->valueint;
        if (!is_creating) {
            mark_layer_dirty(layer, DIRTY_TEXT | DIRTY_LAYOUT);
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
    if (!cJSON_IsBool(value)) return 0;
    layer->scrollable = cJSON_IsTrue(value) ? 1 : 0;
    if (!is_creating) {
        mark_layer_dirty(layer, DIRTY_LAYOUT);
    }
    return 1;
}

// 属性处理器查找表
static const PropertyHandlerEntry property_handlers[] = {
    // 基础属性
    {"id", handle_id},
    
    // 字符串属性
    {"text", handle_text},
    {"label", handle_label},
    {"color", handle_color},
    {"bgColor", handle_bg_color},
    
    // 数值属性
    {"fontSize", handle_font_size},
    {"borderRadius", handle_border_radius},
    {"opacity", handle_opacity},
    
    // 数组属性
    {"size", handle_size},
    {"position", handle_position},
    {"padding", handle_padding},
    
    // 布尔属性
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
