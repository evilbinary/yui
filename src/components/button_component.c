#include "button_component.h"
#include "../render.h"
#include "../backend.h"
#include "../layer_update.h"
#include "../util.h"
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"

#define BUTTON_TAP_SLOP 10

static int button_max_int(int a, int b) { return a > b ? a : b; }

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

        component->icon_gap = 4;
        cJSON* iconGap = cJSON_GetObjectItem(json_obj, "iconGap");
        if (iconGap) {
            component->icon_gap = iconGap->valueint;
        }

        component->icon_align = ICON_ALIGN_LEFT;
        cJSON* iconAlign = cJSON_GetObjectItem(json_obj, "iconAlign");
        if (iconAlign && cJSON_IsString(iconAlign)) {
            if (strcmp(iconAlign->valuestring, "right") == 0) {
                component->icon_align = ICON_ALIGN_RIGHT;
            } else if (strcmp(iconAlign->valuestring, "top") == 0) {
                component->icon_align = ICON_ALIGN_TOP;
            } else if (strcmp(iconAlign->valuestring, "bottom") == 0) {
                component->icon_align = ICON_ALIGN_BOTTOM;
            } else if (strcmp(iconAlign->valuestring, "center") == 0) {
                component->icon_align = ICON_ALIGN_CENTER;
            }
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

    int pad_top = layer_padding_get(layer, 0);
    int pad_right = layer_padding_get(layer, 1);
    int pad_bottom = layer_padding_get(layer, 2);
    int pad_left = layer_padding_get(layer, 3);
    if (pad_top == 0 && pad_bottom == 0 && pad_left == 0 && pad_right == 0) {
        pad_top = 4; pad_bottom = 4; pad_left = 6; pad_right = 6;
    }
    int density = yui_density;

    // --- 测量文本 ---
    Texture* text_tex = NULL;
    int text_w = 0, text_h = 0;
    if (has_text) {
        text_tex = render_text(layer, layer_text, text_color);
        if (text_tex) {
            int tw, th;
            backend_query_texture(text_tex, NULL, NULL, &tw, &th);
            text_w = tw / density;
            text_h = th / density;
            if (text_w < 1) text_w = 1;
            if (text_h < 1) text_h = 1;
        }
    }

    // --- 渲染图标 ---
    Texture* icon_tex = NULL;
    int icon_w = 0, icon_h = 0;
    int icon_tex_owned = 0;
    int icon_is_path = 0;
    if (component->icon_path && !component->icon_tex) {
        component->icon_tex = backend_load_texture(component->icon_path);
    }
    if (component->icon_tex) {
        icon_tex = component->icon_tex;
        icon_is_path = 1;
    } else if (component->icon_text) {
        icon_tex = render_text(layer, component->icon_text, text_color);
        icon_tex_owned = 1;
    }

    if (icon_tex) {
        int iw, ih;
        backend_query_texture(icon_tex, NULL, NULL, &iw, &ih);
        icon_w = iw / density;
        icon_h = ih / density;
        int icon_max = component->icon_size > 0 ? component->icon_size
            : (has_text ? (icon_h > 0 ? icon_h : 16) : (layer->rect.h - pad_top - pad_bottom));
        if (icon_max < 4) icon_max = 4;
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
        if (icon_w < 1) icon_w = 1;
        if (icon_h < 1) icon_h = 1;
    }

    // --- 仅有图标 ---
    if (!has_text && has_icon) {
        int x = layer->rect.x + (layer->rect.w - icon_w) / 2;
        int y = layer->rect.y + (layer->rect.h - icon_h) / 2;
        Rect r = {x, y, icon_w, icon_h};
        if (icon_is_path) {
            backend_render_texture_tinted(icon_tex, NULL, &r, text_color);
        } else {
            backend_render_text_copy(icon_tex, NULL, &r);
        }
        if (icon_tex_owned) backend_render_text_destroy(icon_tex);
        return;
    }

    // --- 仅有文本（保持原位置逻辑） ---
    if (!has_icon) {
        if (text_tex) {
            int avail_w = layer->rect.w - pad_left - pad_right;
            int avail_h = layer->rect.h - pad_top - pad_bottom;
            if (avail_w < 1) avail_w = 1;
            if (avail_h < 1) avail_h = 1;
            if (text_w > avail_w || text_h > avail_h) {
                float ratio = (float)text_w / text_h;
                if (text_w * avail_h > avail_w * text_h) {
                    text_w = avail_w;
                    text_h = (int)(avail_w / ratio);
                } else {
                    text_h = avail_h;
                    text_w = (int)(avail_h * ratio);
                }
            }
            int text_x = layer->rect.x + (layer->rect.w - text_w) / 2;
            int text_y = layer->rect.y + (layer->rect.h - text_h) / 2;
            Rect tr = {text_x, text_y, text_w, text_h};
            backend_render_text_copy(text_tex, NULL, &tr);
            backend_render_text_destroy(text_tex);
        }
        return;
    }

    // --- 图标 + 文本（使用 layer 原生 padding，支持 ... 截断） ---
    int gap = component->icon_gap;
    int content_w = layer->rect.w - pad_left - pad_right;
    if (content_w < 1) content_w = 1;
    int content_h = layer->rect.h - pad_top - pad_bottom;
    if (content_h < 1) content_h = 1;

    // 确定可用文本宽度（水平模式），并对 text 做 "..." 截断
    int avail_text_w = 0;
    if (component->icon_align == ICON_ALIGN_LEFT || component->icon_align == ICON_ALIGN_RIGHT)
        avail_text_w = content_w - icon_w - gap;
    else
        avail_text_w = content_w;
    if (avail_text_w < 8) avail_text_w = 8;

    if (text_tex) backend_render_text_destroy(text_tex);
    text_tex = NULL;
    text_w = 0; text_h = 0;

    {
        int need_trunc = 0;
        if (layer->font && layer->font->default_font) {
            Texture* mt = backend_render_texture(layer->font->default_font, layer_text, (Color){0,0,0,255});
            if (mt) {
                int mw;
                backend_query_texture(mt, NULL, NULL, &mw, NULL);
                backend_render_text_destroy(mt);
                if (mw / density > avail_text_w) need_trunc = 1;
            }
        }

        if (need_trunc) {
            int byte_len = (int)strlen(layer_text);
            char* truncated = malloc((size_t)byte_len + 4);
            while (byte_len > 0) {
                int safe_len = utf8_safe_prefix_bytes(layer_text, byte_len);
                if (safe_len <= 0) break;
                memcpy(truncated, layer_text, (size_t)safe_len);
                truncated[safe_len] = '\0';
                strcat(truncated, "...");
                Texture* st = backend_render_texture(layer->font->default_font, truncated, (Color){0,0,0,255});
                if (st) {
                    int sw;
                    backend_query_texture(st, NULL, NULL, &sw, NULL);
                    backend_render_text_destroy(st);
                    if (sw / density <= avail_text_w) break;
                }
                byte_len = utf8_prev_prefix_bytes(layer_text, safe_len);
            }
            if (byte_len > 0) {
                text_tex = render_text(layer, truncated, text_color);
                if (text_tex) {
                    int tw, th;
                    backend_query_texture(text_tex, NULL, NULL, &tw, &th);
                    text_w = tw / density;
                    text_h = th / density;
                    if (text_w < 1) text_w = 1;
                    if (text_h < 1) text_h = 1;
                }
            }
            free(truncated);
        } else {
            text_tex = render_text(layer, layer_text, text_color);
            if (text_tex) {
                int tw, th;
                backend_query_texture(text_tex, NULL, NULL, &tw, &th);
                text_w = tw / density;
                text_h = th / density;
                if (text_w < 1) text_w = 1;
                if (text_h < 1) text_h = 1;
            }
        }
    }

    switch (component->icon_align) {
        case ICON_ALIGN_LEFT: {
            int block_w = icon_w + gap + text_w;
            int block_h = button_max_int(icon_h, text_h);
            int start_x = layer->rect.x + pad_left + (content_w - block_w) / 2;
            int start_y = layer->rect.y + pad_top + (content_h - block_h) / 2;
            Rect ir = {start_x, start_y + (block_h - icon_h) / 2, icon_w, icon_h};
            Rect tr = {start_x + icon_w + gap, start_y + (block_h - text_h) / 2, text_w, text_h};
            if (icon_is_path) backend_render_texture_tinted(icon_tex, NULL, &ir, text_color);
            else backend_render_text_copy(icon_tex, NULL, &ir);
            if (text_tex) { backend_render_text_copy(text_tex, NULL, &tr); backend_render_text_destroy(text_tex); }
            break;
        }
        case ICON_ALIGN_RIGHT: {
            int block_w = text_w + gap + icon_w;
            int block_h = button_max_int(icon_h, text_h);
            int start_x = layer->rect.x + pad_left + (content_w - block_w) / 2;
            int start_y = layer->rect.y + pad_top + (content_h - block_h) / 2;
            Rect tr = {start_x, start_y + (block_h - text_h) / 2, text_w, text_h};
            Rect ir = {start_x + text_w + gap, start_y + (block_h - icon_h) / 2, icon_w, icon_h};
            if (text_tex) { backend_render_text_copy(text_tex, NULL, &tr); backend_render_text_destroy(text_tex); }
            if (icon_is_path) backend_render_texture_tinted(icon_tex, NULL, &ir, text_color);
            else backend_render_text_copy(icon_tex, NULL, &ir);
            break;
        }
        case ICON_ALIGN_TOP:
        case ICON_ALIGN_CENTER: {
            int use_icon_h = icon_h;
            int use_icon_w = icon_w;
            if (component->icon_align == ICON_ALIGN_CENTER && component->icon_size > 0) {
                use_icon_h = component->icon_size;
                float ratio = (float)icon_w / icon_h;
                use_icon_w = (int)(use_icon_h * ratio);
                if (use_icon_w < 1) use_icon_w = 1;
            }
            int total_h = use_icon_h + gap + text_h;
            if (total_h > content_h && text_h > 0) {
                float s = (float)(content_h - use_icon_h - gap) / text_h;
                if (s < 0.3f) s = 0.3f;
                text_w = (int)(text_w * s);
                text_h = (int)(text_h * s);
                if (text_w < 1) text_w = 1;
                if (text_h < 1) text_h = 1;
            }
            total_h = use_icon_h + gap + text_h;
            int start_y = layer->rect.y + pad_top + (content_h - total_h) / 2;
            Rect ir = {layer->rect.x + (layer->rect.w - use_icon_w) / 2, start_y, use_icon_w, use_icon_h};
            Rect tr = {layer->rect.x + (layer->rect.w - text_w) / 2, start_y + use_icon_h + gap, text_w, text_h};
            if (icon_is_path) backend_render_texture_tinted(icon_tex, NULL, &ir, text_color);
            else backend_render_text_copy(icon_tex, NULL, &ir);
            if (text_tex) { backend_render_text_copy(text_tex, NULL, &tr); backend_render_text_destroy(text_tex); }
            break;
        }
        case ICON_ALIGN_BOTTOM: {
            int total_h = text_h + gap + icon_h;
            if (total_h > content_h && text_h > 0) {
                float s = (float)(content_h - icon_h - gap) / text_h;
                if (s < 0.3f) s = 0.3f;
                text_w = (int)(text_w * s);
                text_h = (int)(text_h * s);
                if (text_w < 1) text_w = 1;
                if (text_h < 1) text_h = 1;
            }
            total_h = text_h + gap + icon_h;
            int start_y = layer->rect.y + pad_top + (content_h - total_h) / 2;
            Rect tr = {layer->rect.x + (layer->rect.w - text_w) / 2, start_y, text_w, text_h};
            if (text_tex) { backend_render_text_copy(text_tex, NULL, &tr); backend_render_text_destroy(text_tex); }
            Rect ir = {layer->rect.x + (layer->rect.w - icon_w) / 2, start_y + text_h + gap, icon_w, icon_h};
            if (icon_is_path) backend_render_texture_tinted(icon_tex, NULL, &ir, text_color);
            else backend_render_text_copy(icon_tex, NULL, &ir);
            break;
        }
    }

    if (icon_tex_owned) backend_render_text_destroy(icon_tex);
}