#include "../render.h"
#include "../backend.h"
#include "../util.h"
#include "../layer_update.h"
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>
#include "checkbox_component.h"

static int checkbox_variant_has(const Layer* layer, const char* token) {
    if (!layer || !token || token[0] == '\0' || layer->variant[0] == '\0') {
        return 0;
    }
    const char* p = layer->variant;
    size_t n = strlen(token);
    while (*p) {
        while (*p == ' ') {
            p++;
        }
        if (strncmp(p, token, n) == 0 && (p[n] == '\0' || p[n] == ' ')) {
            return 1;
        }
        while (*p && *p != ' ') {
            p++;
        }
    }
    return 0;
}

static int checkbox_is_switch(const Layer* layer) {
    return checkbox_variant_has(layer, "switch");
}

static void checkbox_dim_color(Color* color, int is_disabled) {
    if (!color || !is_disabled) {
        return;
    }
    int gray = (color->r * 30 + color->g * 59 + color->b * 11) / 100;
    color->r = (uint8_t)gray;
    color->g = (uint8_t)gray;
    color->b = (uint8_t)gray;
    color->a = (uint8_t)(color->a * 0.6);
}

static int checkbox_on_data_update(Layer* layer, cJSON* data) {
    if (!layer || !layer->component || !data) {
        return 0;
    }
    if (cJSON_IsBool(data)) {
        checkbox_component_set_checked((CheckboxComponent*)layer->component,
                                       cJSON_IsTrue(data));
        return 1;
    }
    return 0;
}

static void checkbox_apply_style(CheckboxComponent* component, Layer* layer, cJSON* style) {
    if (!component || !layer || !style || !cJSON_IsObject(style)) {
        return;
    }

    cJSON* bg_color = cJSON_GetObjectItem(style, "bgColor");
    if (bg_color && cJSON_IsString(bg_color)) {
        parse_color(bg_color->valuestring, &layer->bg_color);
    }

    cJSON* color = cJSON_GetObjectItem(style, "color");
    if (color && cJSON_IsString(color)) {
        parse_color(color->valuestring, &layer->color);
    }

    cJSON* border_color = cJSON_GetObjectItem(style, "borderColor");
    if (border_color && cJSON_IsString(border_color)) {
        parse_color(border_color->valuestring, &component->border_color);
    }

    cJSON* check_color = cJSON_GetObjectItem(style, "checkColor");
    if (check_color && cJSON_IsString(check_color)) {
        parse_color(check_color->valuestring, &component->check_color);
    }

    mark_layer_dirty(layer, DIRTY_COLOR);
}

static void checkbox_component_set_style(Layer* layer, cJSON* style) {
    if (!layer || !layer->component) return;
    checkbox_apply_style((CheckboxComponent*)layer->component, layer, style);
}

static void checkbox_apply_style_from_json(CheckboxComponent* component, cJSON* json_obj) {
    if (!component || !json_obj) {
        return;
    }

    cJSON* style = cJSON_GetObjectItem(json_obj, "style");
    if (!style || !cJSON_IsObject(style)) {
        return;
    }

    checkbox_apply_style(component, component->layer, style);
}

static void checkbox_layer_destroy(Layer* layer) {
    if (!layer || !layer->component) {
        return;
    }
    checkbox_component_destroy((CheckboxComponent*)layer->component);
    layer->component = NULL;
}

// 创建复选框组件
CheckboxComponent* checkbox_component_create(Layer* layer, int default_checked) {
    if (!layer) {
        return NULL;
    }

    Color saved_bg = layer->bg_color;
    Color saved_fg = layer->color;
    
    CheckboxComponent* component = (CheckboxComponent*)malloc(sizeof(CheckboxComponent));
    if (!component) {
        return NULL;
    }
    
    memset(component, 0, sizeof(CheckboxComponent));
    component->layer = layer;
    component->checked = default_checked;
    component->user_data = NULL;
    // 不再初始化component->label，因为我们将使用layer->label
    
    // 保留 JSON/style 已设置的颜色，否则使用默认值
    layer->bg_color = saved_bg.a > 0 ? saved_bg : (Color){255, 255, 255, 255};
    component->border_color = (Color){148, 163, 184, 255};
    component->check_color = (Color){59, 130, 246, 255};
    layer->color = saved_fg.a > 0 ? saved_fg : (Color){0, 0, 0, 255};
        
    // 设置组件指针和自定义渲染函数
    layer->component = component;
    layer->render = checkbox_component_render;
    
    // 绑定事件处理函数
    layer->handle_pointer_event = checkbox_component_handle_pointer_event;
    
    // 设置组件为可聚焦（默认情况下未禁用时可聚焦）
    layer->focusable = !HAS_STATE(layer, LAYER_STATE_DISABLED);
    layer->on_data_update = checkbox_on_data_update;
    layer->get_property = checkbox_component_get_property;
    layer->set_style = checkbox_component_set_style;
    layer->on_destroy = checkbox_layer_destroy;

    return component;
}

// 从JSON创建复选框组件
CheckboxComponent* checkbox_component_create_from_json(Layer* layer, cJSON* json_obj) {
    int default_checked = 0;
    if(cJSON_HasObjectItem(json_obj, "data")) {
        default_checked = cJSON_IsTrue(cJSON_GetObjectItem(json_obj, "data"));
    }
    cJSON* size = cJSON_GetObjectItem(json_obj, "size");
    if (size == NULL) {
      if (checkbox_is_switch(layer)) {
        layer->rect.w = 40;
        layer->rect.h = 22;
      } else {
        layer->rect.w = 20;
        layer->rect.h = 20;
      }
      layer->fixed_width = layer->rect.w;
      layer->fixed_height = layer->rect.h;
    }
    CheckboxComponent* component = checkbox_component_create(layer, default_checked);
    if (component) {
        checkbox_apply_style_from_json(component, json_obj);
    }
    return component;
}

// 销毁复选框组件
void checkbox_component_destroy(CheckboxComponent* component) {
    if (component) {
        // 不再需要释放component->label，因为我们使用的是layer->label
        free(component);
    }
}

// 设置复选框选中状态
void checkbox_component_set_checked(CheckboxComponent* component, int checked) {
    if (component) {
        component->checked = checked ? 1 : 0;
        // 同步状态到图层
        if (component->checked) {
            SET_STATE(component->layer, LAYER_STATE_ACTIVE);
        } else {
            CLEAR_STATE(component->layer, LAYER_STATE_ACTIVE);
        }
    }
}

// 获取复选框选中状态
int checkbox_component_is_checked(CheckboxComponent* component) {
    if (component) {
        return component->checked;
    }
    return 0;
}

cJSON* checkbox_component_get_property(Layer* layer, const char* property_name) {
    if (!layer || !property_name || !layer->component) {
        return NULL;
    }

    CheckboxComponent* component = (CheckboxComponent*)layer->component;

    if (strcmp(property_name, "data") == 0) {
        return cJSON_CreateBool(component->checked);
    }

    return NULL;
}

// 设置复选框颜色
void checkbox_component_set_colors(CheckboxComponent* component, Color bg_color, Color border_color, Color check_color) {
    if (component) {
        // 使用layer->bg_color代替component->bg_color
        if (component->layer) {
            component->layer->bg_color = bg_color;
        }
        component->border_color = border_color;
        component->check_color = check_color;
    }
}

// 设置用户数据
void checkbox_component_set_user_data(CheckboxComponent* component, void* data) {
    if (component) {
        component->user_data = data;
    }
}

// 设置标签文本和颜色
void checkbox_component_set_label(CheckboxComponent* component, const char* label, Color color) {
    if (!component) {
        return;
    }
    
    // 直接使用layer->label
    if (component->layer) {
        layer_set_label(component->layer, label);
        component->layer->color = color;
    }
}

// 设置禁用状态
void checkbox_component_set_disabled(CheckboxComponent* component, int disabled) {
    if (!component || !component->layer) {
        return;
    }
    
    if (disabled) {
        SET_STATE(component->layer, LAYER_STATE_DISABLED);
        // 禁用时不能获得焦点
        component->layer->focusable = 0;
    } else {
        CLEAR_STATE(component->layer, LAYER_STATE_DISABLED);
        // 启用时恢复焦点能力
        component->layer->focusable = 1;
    }
}

// 检查是否被禁用
int checkbox_component_is_disabled(CheckboxComponent* component) {
    if (!component || !component->layer) {
        return 0;
    }
    
    return HAS_STATE(component->layer, LAYER_STATE_DISABLED);
}

// 处理鼠标事件
int checkbox_component_handle_pointer_event(Layer* layer, PointerEvent* event) {
    if (!layer || !event || !layer->component) {
        return 0;
    }

    // 如果组件被禁用，不响应点击事件
    if (HAS_STATE(layer, LAYER_STATE_DISABLED)) {
        return 0;
    }

    CheckboxComponent* component = (CheckboxComponent*)layer->component;

    // 检查鼠标是否在复选框范围内
    int is_inside = (event->x >= layer->rect.x &&
                     event->x < layer->rect.x + layer->rect.w &&
                     event->y >= layer->rect.y &&
                     event->y < layer->rect.y + layer->rect.h);

    // 处理鼠标点击事件
    if (event->button == BUTTON_LEFT && event->phase == POINTER_DOWN && is_inside) {
        // 切换选中状态
        component->checked = !component->checked;

        // 标记图层状态变化，确保重绘，但保留禁用状态
        if (component->checked) {
            layer->state |= LAYER_STATE_ACTIVE; // 只设置激活位，保留其他位
        } else {
            layer->state &= ~LAYER_STATE_ACTIVE; // 只清除激活位，保留其他位
        }

        mark_layer_dirty(layer, DIRTY_COLOR);

        // 如果有点击事件回调，调用它
        if (layer->event && layer->event->click) {
            EVENT_INVOKE(layer->event->click, layer);
        }
    }
    return 0;
}

// 绘制勾选标记（单线，避免双线加粗导致弯曲/十字）
static void checkbox_draw_tick(const Layer* layer, Color tick) {
    int ox = layer->rect.x;
    int oy = layer->rect.y;
    int w = layer->rect.w;
    int h = layer->rect.h;
    int x1 = ox + (w * 5 + 9) / 20;
    int y1 = oy + (h * 10 + 9) / 20;
    int x2 = ox + (w * 8 + 9) / 20;
    int y2 = oy + (h * 14 + 9) / 20;
    int x3 = ox + (w * 16 + 9) / 20;
    int y3 = oy + (h * 6 + 9) / 20;

    backend_render_line(x1, y1, x2, y2, tick);
    backend_render_line(x2, y2, x3, y3, tick);
}

static void checkbox_render_switch(Layer* layer, CheckboxComponent* component, int is_disabled) {
    Color track = component->checked ? component->check_color : component->border_color;
    checkbox_dim_color(&track, is_disabled);

    int radius = layer->rect.h / 2;
    if (radius < 1) {
        radius = 1;
    }
    backend_render_rounded_rect(&layer->rect, track, radius);

    int pad = layer->rect.h > 16 ? 2 : 1;
    int thumb_d = layer->rect.h - pad * 2;
    if (thumb_d < 4) {
        thumb_d = layer->rect.h > 4 ? layer->rect.h - 2 : layer->rect.h;
        pad = (layer->rect.h - thumb_d) / 2;
    }
    int thumb_x = component->checked
                      ? layer->rect.x + layer->rect.w - pad - thumb_d
                      : layer->rect.x + pad;
    int thumb_y = layer->rect.y + pad;
    Rect thumb = { thumb_x, thumb_y, thumb_d, thumb_d };
    Color thumb_color = { 255, 255, 255, is_disabled ? 200 : 255 };
    backend_render_rounded_rect(&thumb, thumb_color, thumb_d / 2);
}

static void checkbox_render_box(Layer* layer, CheckboxComponent* component, int is_disabled) {
    if (component->checked) {
        Color fill = component->check_color;
        checkbox_dim_color(&fill, is_disabled);
        backend_render_fill_rect_color(
            &layer->rect, fill.r, fill.g, fill.b, fill.a);

        Color tick = {255, 255, 255, is_disabled ? 200 : 255};
        checkbox_draw_tick(layer, tick);
    } else {
        Color bg_color = layer->bg_color;
        Color border_color = component->border_color;
        if (is_disabled) {
            bg_color.a = (uint8_t)(bg_color.a * 0.6);
            checkbox_dim_color(&border_color, 1);
        }
        backend_render_fill_rect_color(
            &layer->rect, bg_color.r, bg_color.g, bg_color.b, bg_color.a);
        backend_render_rect_color(
            &layer->rect, border_color.r, border_color.g, border_color.b, border_color.a);
    }
}

// 渲染复选框（variant 含 switch 时绘制为开关）
void checkbox_component_render(Layer* layer) {
    if (!layer || !layer->component) {
        return;
    }
    
    CheckboxComponent* component = (CheckboxComponent*)layer->component;
    int is_disabled = HAS_STATE(layer, LAYER_STATE_DISABLED);

    if (checkbox_is_switch(layer)) {
        checkbox_render_switch(layer, component, is_disabled);
    } else {
        checkbox_render_box(layer, component, is_disabled);
    }
    
    // 绘制标签文本，使用layer->label和layer->color
    const char* label_text = layer_get_label(layer);
    if (label_text[0] != '\0' && layer->font && layer->font->default_font) {
        int label_x = layer->rect.x + layer->rect.w + 5;
        
        Color text_color = layer->color;
        checkbox_dim_color(&text_color, is_disabled);
        
        Texture* text_texture = backend_render_texture(layer->font->default_font, label_text, text_color);
        if (text_texture) {
            int text_width, text_height;
            backend_query_texture(text_texture, NULL, NULL, &text_width, &text_height);
            
            int label_y = layer->rect.y + (layer->rect.h - text_height/yui_density) / 2;
            
            Rect dst_rect = {
                .x = label_x,
                .y = label_y,
                .w = text_width/ yui_density,
                .h = text_height/ yui_density
            };
            
            backend_render_text_copy(text_texture, NULL, &dst_rect);
            backend_render_text_destroy(text_texture);
        }
    }
}