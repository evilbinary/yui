#include "button_component.h"
#include "../render.h"
#include "../backend.h"
#include "../layer_update.h"
#include "../util.h"
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"

#define BUTTON_TAP_SLOP 10

static int button_point_inside(const Layer* layer, int x, int y) {
    return layer &&
           x >= layer->rect.x &&
           x < layer->rect.x + layer->rect.w &&
           y >= layer->rect.y &&
           y < layer->rect.y + layer->rect.h;
}

static int button_exceeded_slop(const ButtonComponent* component, int x, int y) {
    if (!component) return 0;
    int dx = x - component->press_x;
    int dy = y - component->press_y;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    return dx > BUTTON_TAP_SLOP || dy > BUTTON_TAP_SLOP;
}

static void button_begin_pointer(ButtonComponent* component, Layer* layer, int x, int y) {
    component->press_x = x;
    component->press_y = y;
    component->pointer_active = 1;
    component->press_armed = 1;
    component->drag_cancelled = 0;
    SET_STATE(layer, LAYER_STATE_PRESSED);
}

static void button_abort_drag(ButtonComponent* component, Layer* layer) {
    component->pointer_active = 0;
    component->drag_cancelled = 1;
    CLEAR_STATE(layer, LAYER_STATE_PRESSED);
}

static void button_cancel_pointer(ButtonComponent* component, Layer* layer) {
    component->pointer_active = 0;
    component->press_armed = 0;
    component->drag_cancelled = 0;
    CLEAR_STATE(layer, LAYER_STATE_PRESSED);
}

static void button_fire_click(Layer* layer) {
    if (layer && layer->event && layer->event->click) {
        layer->event->click((void*)layer);
    }
}

// 创建按钮组件（内部通用初始化）
ButtonComponent* button_component_create(Layer* layer) {
    return button_component_create_from_json(layer, NULL);
}

static void button_layer_destroy(Layer* layer) {
    if (!layer || !layer->component) return;
    button_component_destroy((ButtonComponent*)layer->component);
    layer->component = NULL;
}

static Color button_shade_color(Color base, int delta) {
    Color c = base;
    int r = (int)base.r + delta;
    int g = (int)base.g + delta;
    int b = (int)base.b + delta;
    if (r < 0) r = 0; else if (r > 255) r = 255;
    if (g < 0) g = 0; else if (g > 255) g = 255;
    if (b < 0) b = 0; else if (b > 255) b = 255;
    c.r = (unsigned char)r;
    c.g = (unsigned char)g;
    c.b = (unsigned char)b;
    return c;
}

static void button_sync_state_colors(ButtonComponent* component, Color base) {
    if (!component) return;
    component->colors[LAYER_STATE_NORMAL] = base;
    component->colors[LAYER_STATE_HOVER] = button_shade_color(base, 18);
    component->colors[LAYER_STATE_PRESSED] = button_shade_color(base, -28);
    component->colors[LAYER_STATE_FOCUSED] = button_shade_color(base, 12);
    component->colors[LAYER_STATE_DISABLED] = (Color){base.r, base.g, base.b, 150};
}

static void button_component_apply_theme_style(Layer* layer, cJSON* style) {
    if (!layer || !layer->component || !style || !cJSON_IsObject(style)) {
        return;
    }

    ButtonComponent* component = (ButtonComponent*)layer->component;
    cJSON* bg = cJSON_GetObjectItem(style, "bgColor");
    if (bg && cJSON_IsString(bg) && bg->valuestring) {
        if (strcmp(bg->valuestring, "transparent") == 0) {
            component->bg_transparent = 1;
            layer->bg_color = (Color){0, 0, 0, 0};
        } else {
            Color c;
            parse_color(bg->valuestring, &c);
            component->bg_transparent = 0;
            layer->bg_color = c;
            button_sync_state_colors(component, c);
        }
        mark_layer_dirty(layer, DIRTY_COLOR);
    }

    cJSON* color = cJSON_GetObjectItem(style, "color");
    if (color && cJSON_IsString(color) && color->valuestring) {
        parse_color(color->valuestring, &layer->color);
        mark_layer_dirty(layer, DIRTY_COLOR | DIRTY_TEXT);
    }

    cJSON* hover = cJSON_GetObjectItem(style, "hoverColor");
    if (hover && cJSON_IsString(hover) && hover->valuestring) {
        parse_color(hover->valuestring, &component->hover_text_color);
        mark_layer_dirty(layer, DIRTY_COLOR);
    }

    cJSON* radius = cJSON_GetObjectItem(style, "borderRadius");
    if (!radius) {
        radius = cJSON_GetObjectItem(style, "radius");
    }
    if (radius && cJSON_IsNumber(radius)) {
        layer->radius = radius->valueint;
        mark_layer_dirty(layer, DIRTY_STYLE);
    }

    cJSON* font_size = cJSON_GetObjectItem(style, "fontSize");
    if (font_size && cJSON_IsNumber(font_size)) {
        if (!layer->font) {
            layer->font = (Font*)malloc(sizeof(Font));
            memset(layer->font, 0, sizeof(Font));
            strcpy(layer->font->path, "Roboto-Regular.ttf");
            strcpy(layer->font->weight, "normal");
        }
        if (layer->font->size != font_size->valueint) {
            layer->font->size = font_size->valueint;
            layer->font->default_font = NULL;
            mark_layer_dirty(layer, DIRTY_TEXT | DIRTY_LAYOUT);
        }
    }

    if (cJSON_HasObjectItem(style, "shadow")) {
        parse_layer_shadow(cJSON_GetObjectItem(style, "shadow"), &layer->shadow);
        mark_layer_dirty(layer, DIRTY_STYLE | DIRTY_COLOR);
    }
    if (cJSON_HasObjectItem(style, "bgGradient")) {
        parse_layer_gradient(cJSON_GetObjectItem(style, "bgGradient"), &layer->bg_gradient);
        mark_layer_dirty(layer, DIRTY_STYLE | DIRTY_COLOR);
    }
    if (cJSON_HasObjectItem(style, "border")) {
        parse_layer_border(cJSON_GetObjectItem(style, "border"), &layer->border);
        mark_layer_dirty(layer, DIRTY_STYLE | DIRTY_COLOR);
    }
    if (cJSON_HasObjectItem(style, "borderWidth") || cJSON_HasObjectItem(style, "borderSize") ||
        cJSON_HasObjectItem(style, "border-width")) {
        cJSON* bw = cJSON_GetObjectItem(style, "borderWidth");
        if (!bw) bw = cJSON_GetObjectItem(style, "borderSize");
        if (!bw) bw = cJSON_GetObjectItem(style, "border-width");
        parse_layer_border_width(bw, &layer->border);
        mark_layer_dirty(layer, DIRTY_STYLE | DIRTY_COLOR);
    }
    if (cJSON_HasObjectItem(style, "borderStyle") || cJSON_HasObjectItem(style, "border-style")) {
        cJSON* bs = cJSON_GetObjectItem(style, "borderStyle");
        if (!bs) bs = cJSON_GetObjectItem(style, "border-style");
        parse_layer_border_style(bs, &layer->border);
        mark_layer_dirty(layer, DIRTY_STYLE | DIRTY_COLOR);
    }
    if (cJSON_HasObjectItem(style, "borderColor") || cJSON_HasObjectItem(style, "border-color")) {
        cJSON* bc = cJSON_GetObjectItem(style, "borderColor");
        if (!bc) bc = cJSON_GetObjectItem(style, "border-color");
        parse_layer_border_color(bc, &layer->border);
        mark_layer_dirty(layer, DIRTY_STYLE | DIRTY_COLOR);
    }
}

ButtonComponent* button_component_create_from_json(Layer* layer, cJSON* json_obj) {
    if (!layer) {
        return NULL;
    }
    
    ButtonComponent* component = (ButtonComponent*)malloc(sizeof(ButtonComponent));
    if (!component) {
        return NULL;
    }
    
    memset(component, 0, sizeof(ButtonComponent));
    component->layer = layer;
    component->user_data = NULL;
    component->icon_path = NULL;
    component->icon_text = NULL;
    component->icon_tex = NULL;
    component->icon_size = 0;
    
    // 设置默认颜色
    component->colors[LAYER_STATE_NORMAL] = (Color){100, 149, 237, 255};    // 蓝色
    component->colors[LAYER_STATE_HOVER] = (Color){135, 206, 250, 255};     // 亮蓝色
    component->colors[LAYER_STATE_PRESSED] = (Color){70, 130, 180, 255};    // 深蓝色
    component->colors[LAYER_STATE_DISABLED] = (Color){200, 200, 200, 150};  // 灰色半透明

    // 从 JSON 加载图标配置
    if (json_obj) {
        cJSON* icon_json = cJSON_GetObjectItem(json_obj, "icon");
        if (icon_json && icon_json->valuestring) {
            const char* icon_val = icon_json->valuestring;
            // 含路径分隔符或扩展名的视为图片/SVG 文件，否则为文本图标（emoji 等）
            if (strchr(icon_val, '.') || strchr(icon_val, '/') || strchr(icon_val, '\\')) {
                component->icon_path = strdup(icon_val);
            } else {
                component->icon_text = strdup(icon_val);
            }
        }

        // 检测 bgColor 是否显式设为 transparent；可选 hoverColor
        cJSON* style = cJSON_GetObjectItem(json_obj, "style");
        if (style) {
            cJSON* bg = cJSON_GetObjectItem(style, "bgColor");
            if (bg && bg->valuestring && strcmp(bg->valuestring, "transparent") == 0) {
                component->bg_transparent = 1;
            }
            cJSON* hover = cJSON_GetObjectItem(style, "hoverColor");
            if (hover && hover->valuestring) {
                parse_color(hover->valuestring, &component->hover_text_color);
            }
        }

        cJSON* iconSize = cJSON_GetObjectItem(json_obj, "iconSize");
        if (iconSize) {
            component->icon_size = iconSize->valueint;
        }
    }

    // 设置组件指针和自定义渲染函数
    layer->component = component;
    layer->render = button_component_render;
    
    // 绑定事件处理函数
    layer->handle_pointer_event = button_component_handle_pointer_event;

    // 绑定键盘事件处理函数
    layer->handle_key_event = button_component_handle_key_event;

    layer->set_style = button_component_apply_theme_style;
    
    // 设置组件为可聚焦
    layer->focusable = 1;
    layer->on_destroy = button_layer_destroy;
    
    return component;
}

// 销毁按钮组件
void button_component_destroy(ButtonComponent* component) {
    if (component) {
        if (component->icon_text) free(component->icon_text);
        if (component->icon_path) free(component->icon_path);
        if (component->icon_tex) backend_render_text_destroy(component->icon_tex);
        free(component);
    }
}

// 设置按钮文本
void button_component_set_text(ButtonComponent* component, const char* text) {
    if (!component || !text) {
        return;
    }
    
    layer_set_text(component->layer, text);
}

// 设置图标路径
void button_component_set_icon_path(ButtonComponent* component, const char* path) {
    if (!component || !path) return;
    if (component->icon_path) free(component->icon_path);
    if (component->icon_text) { free(component->icon_text); component->icon_text = NULL; }
    if (component->icon_tex) { backend_render_text_destroy(component->icon_tex); component->icon_tex = NULL; }
    component->icon_path = strdup(path);
}

// 设置图标文本
void button_component_set_icon_text(ButtonComponent* component, const char* text) {
    if (!component || !text) return;
    if (component->icon_text) free(component->icon_text);
    if (component->icon_path) { free(component->icon_path); component->icon_path = NULL; }
    if (component->icon_tex) { backend_render_text_destroy(component->icon_tex); component->icon_tex = NULL; }
    component->icon_text = strdup(text);
}

// 设置按钮颜色
void button_component_set_color(ButtonComponent* component, LayerState state, Color color) {
    if (!component || state < 0 || state >= 4) {
        return;
    }
    
    component->colors[state] = color;
}

// 设置用户数据
void button_component_set_user_data(ButtonComponent* component, void* data) {
    if (!component) {
        return;
    }
    
    component->user_data = data;
}

// 处理键盘事件
int button_component_handle_key_event(Layer* layer, KeyEvent* event) {
    ButtonComponent* component = (ButtonComponent*)layer->component;
    if (!component || HAS_STATE(layer, LAYER_STATE_DISABLED)) {
        return 0;
    }

    // 处理回车键或空格键按下事件
    if (event->type == KEY_EVENT_DOWN && (event->data.key.key_code == 13 || event->data.key.key_code == 32)) {
        // 设置 PRESSED 状态
        SET_STATE(layer, LAYER_STATE_PRESSED);
    }

    // 处理按键释放事件，触发点击并恢复按钮状态
    if (event->type == KEY_EVENT_UP && (event->data.key.key_code == 13 || event->data.key.key_code == 32)) {
        // 触发点击事件（如果存在）
        if (HAS_STATE(layer, LAYER_STATE_PRESSED)) {
            CLEAR_STATE(layer, LAYER_STATE_PRESSED);
            // 在按键释放时触发点击事件
            if (layer->event && layer->event->click) {
                EVENT_INVOKE(layer->event->click, layer);
            }
        }
    }
    return 0;
}

// 处理指针事件（鼠标与触摸）
int button_component_handle_pointer_event(Layer* layer, PointerEvent* event) {
    ButtonComponent* component = (ButtonComponent*)layer->component;
    if (!component || !event || HAS_STATE(layer, LAYER_STATE_DISABLED)) {
        return 0;
    }

    int is_inside = button_point_inside(layer, event->x, event->y);
    int was_hover = HAS_STATE(layer, LAYER_STATE_HOVER);

    if (is_inside) {
        SET_STATE(layer, LAYER_STATE_HOVER);
    } else {
        CLEAR_STATE(layer, LAYER_STATE_HOVER);
    }
    if (was_hover != HAS_STATE(layer, LAYER_STATE_HOVER)) {
        mark_layer_dirty(layer, DIRTY_TEXT | DIRTY_COLOR);
    }

    if (event->phase == POINTER_DOWN) {
        if (is_inside) {
            button_begin_pointer(component, layer, event->x, event->y);
        }
    } else if (event->phase == POINTER_MOVE) {
        /* 拖出按钮区域或超过 slop：取消点击（避免松手落在另一按钮上误开） */
        if (component->pointer_active &&
            (!is_inside || button_exceeded_slop(component, event->x, event->y))) {
            button_abort_drag(component, layer);
        }
    } else if (event->phase == POINTER_CANCEL) {
        int armed = component->press_armed;
        button_cancel_pointer(component, layer);
        return armed ? 1 : 0;
    } else if (event->phase == POINTER_UP) {
        int armed = component->press_armed;
        if (armed && !component->drag_cancelled && is_inside) {
            button_fire_click(layer);
        }
        button_cancel_pointer(component, layer);
        return armed ? 1 : 0;
    }
    return 0;
}

// 渲染按钮组件
void button_component_render(Layer* layer) {
    if (!layer || !layer->component) {
        return;
    }
    
    ButtonComponent* component = (ButtonComponent*)layer->component;
    
    // 背景色选择
    Color bg_color;
    int has_bg = 1;
    if (component->bg_transparent) {
        /* 透明按钮：hover/press 只叠半透明底，不画边框 */
        has_bg = 0;
        if (HAS_STATE(layer, LAYER_STATE_PRESSED)) {
            if (component->hover_text_color.a > 0) {
                bg_color = component->hover_text_color;
                bg_color.a = 64;
            } else {
                bg_color = (Color){255, 255, 255, 56};
            }
        } else if (HAS_STATE(layer, LAYER_STATE_HOVER) || HAS_STATE(layer, LAYER_STATE_FOCUSED)) {
            if (component->hover_text_color.a > 0) {
                bg_color = component->hover_text_color;
                bg_color.a = 40;
            } else {
                bg_color = (Color){255, 255, 255, 36};
            }
        } else {
            bg_color = (Color){0, 0, 0, 0};
        }
    } else if (layer->bg_color.a > 0) {
        /* 主题/样式写入 layer->bg_color 后，hover/press 用同步过的状态色 */
        if (HAS_STATE(layer, LAYER_STATE_PRESSED)) {
            bg_color = component->colors[LAYER_STATE_PRESSED];
        } else if (HAS_STATE(layer, LAYER_STATE_HOVER)) {
            bg_color = component->colors[LAYER_STATE_HOVER];
        } else if (HAS_STATE(layer, LAYER_STATE_FOCUSED)) {
            bg_color = component->colors[LAYER_STATE_FOCUSED];
        } else if (HAS_STATE(layer, LAYER_STATE_DISABLED)) {
            bg_color = component->colors[LAYER_STATE_DISABLED];
        } else {
            bg_color = layer->bg_color;
        }
    } else {
        // 根据状态使用组件默认色
        if (HAS_STATE(layer, LAYER_STATE_PRESSED)) {
            bg_color = component->colors[LAYER_STATE_PRESSED];
        } else if (HAS_STATE(layer, LAYER_STATE_HOVER)) {
            bg_color = component->colors[LAYER_STATE_HOVER];
        } else if (HAS_STATE(layer, LAYER_STATE_FOCUSED)) {
            bg_color = component->colors[LAYER_STATE_FOCUSED];
        } else if (HAS_STATE(layer, LAYER_STATE_DISABLED)) {
            bg_color = component->colors[LAYER_STATE_DISABLED];
        } else {
            bg_color = component->colors[LAYER_STATE_NORMAL];
        }
    }

    // 绘制阴影 + 背景（支持 bgGradient / shadow）
    if (component->bg_transparent) {
        if (bg_color.a > 0) {
            if (layer->radius > 0) {
                backend_render_rounded_rect(&layer->rect, bg_color, layer->radius);
            } else {
                backend_render_fill_rect(&layer->rect, bg_color);
            }
        }
    } else if (bg_color.a > 0 || layer->bg_gradient.enabled || layer->shadow.enabled ||
               layer_border_visible(&layer->border)) {
        /* Soft UI / border-0：默认无硬边；显式 borderWidth/border 时由 render_layer_background 画 */
        (void)has_bg;
        render_layer_background(layer, layer->bg_gradient.enabled ? NULL : &bg_color);

        if (layer->backdrop_filter) {
            backend_render_backdrop_filter(&layer->rect, layer->blur_radius, layer->saturation, layer->brightness);
        }
    }
    
    // 渲染图标与文本
    const char* layer_text = layer_get_text(layer);
    int has_text = layer_text && layer_text[0] != '\0';
    int has_icon = component->icon_path || component->icon_text;
    if (!has_text && !has_icon) {
        return;
    }

    Color text_color = layer->color;
    if (HAS_STATE(layer, LAYER_STATE_DISABLED)) {
        text_color = (Color){255, 255, 255, 150};
    } else if (HAS_STATE(layer, LAYER_STATE_HOVER) && component->hover_text_color.a > 0) {
        text_color = component->hover_text_color;
    }

    int text_y_center = layer->rect.y + layer->rect.h / 2;
    const int pad_h = 6;
    const int pad_right = 6;
    const int icon_text_gap = 4;

    // --- 先测量文本尺寸，图标+文字并存时优先保证文字按 fontSize 渲染 ---
    Texture* text_texture = NULL;
    int draw_w = 0, draw_h = 0;
    if (has_text) {
        text_texture = render_text(layer, layer_text, text_color);
        if (text_texture) {
            int text_width, text_height;
            backend_query_texture(text_texture, NULL, NULL, &text_width, &text_height);
            draw_w = text_width / yui_density;
            draw_h = text_height / yui_density;
        }
    }

    // --- 渲染图标（优先 SVG/图片，回退到文本图标） ---
    int icon_w = 0, icon_h = 0;
    Texture* icon_tex = NULL;
    int icon_tex_owned = 0;
    if (component->icon_path && !component->icon_tex) {
        component->icon_tex = backend_load_texture(component->icon_path);
    }
    if (component->icon_tex) {
        icon_tex = component->icon_tex;
    } else if (component->icon_text) {
        icon_tex = render_text(layer, component->icon_text, text_color);
        icon_tex_owned = 1;
    }

    if (icon_tex) {
        int iw, ih;
        backend_query_texture(icon_tex, NULL, NULL, &iw, &ih);
        int icon_max = component->icon_size > 0 ? component->icon_size : layer->rect.h - 8;
        icon_w = iw / yui_density;
        icon_h = ih / yui_density;

        if (has_text && draw_w > 0) {
            // 为文字预留空间，剩余宽度才给图标，避免挤压文字字号
            int icon_space = layer->rect.w - pad_h - pad_right - icon_text_gap - draw_w;
            if (icon_space < 8) icon_space = 8;
            if (icon_max > icon_space) icon_max = icon_space;
        }

        if (icon_w > icon_max || icon_h > icon_max) {
            float ratio = (float)icon_w / icon_h;
            if (ratio > 1.0f) {
                icon_w = icon_max;
                icon_h = (int)(icon_max / ratio);
            } else {
                icon_h = icon_max;
                icon_w = (int)(icon_max * ratio);
            }
        }
        int icon_x = has_text ? layer->rect.x + pad_h : layer->rect.x + (layer->rect.w - icon_w) / 2;
        int icon_y = text_y_center - icon_h / 2;
        Rect icon_rect = {icon_x, icon_y, icon_w, icon_h};
        if (component->icon_path) {
            backend_render_texture_tinted(icon_tex, NULL, &icon_rect, text_color);
        } else {
            backend_render_text_copy(icon_tex, NULL, &icon_rect);
        }
        if (icon_tex_owned) backend_render_text_destroy(icon_tex);
    }

    // --- 渲染文本 ---
    if (text_texture) {
        int avail_w = layer->rect.w - (icon_w > 0 ? pad_h + icon_w + icon_text_gap + pad_right : pad_h * 2);
        int avail_h = layer->rect.h - 8;
        if (avail_w < 1) avail_w = 1;
        if (avail_h < 1) avail_h = 1;
        if (draw_w > avail_w || draw_h > avail_h) {
            float ratio = (float)draw_w / draw_h;
            if (draw_w * avail_h > avail_w * draw_h) {
                draw_w = avail_w;
                draw_h = (int)(avail_w / ratio);
            } else {
                draw_h = avail_h;
                draw_w = (int)(avail_h * ratio);
            }
            if (draw_w < 1) draw_w = 1;
            if (draw_h < 1) draw_h = 1;
        }

        int text_x, text_y;
        if (icon_w > 0) {
            text_x = layer->rect.x + pad_h + icon_w + icon_text_gap;
        } else {
            text_x = layer->rect.x + (layer->rect.w - draw_w) / 2;
        }
        text_y = layer->rect.y + (layer->rect.h - draw_h) / 2;

        Rect text_rect = {text_x, text_y, draw_w, draw_h};
        backend_render_text_copy(text_texture, NULL, &text_rect);
        backend_render_text_destroy(text_texture);
    }
}