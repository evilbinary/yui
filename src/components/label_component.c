#include "label_component.h"
#include "../render.h"
#include "../backend.h"
#include "../popup_manager.h"
#include "../util.h"
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define TOOLTIP_DELAY_MS 400

// 渲染 tooltip 弹层
static void tooltip_layer_render(Layer* layer) {
    if (!layer) return;
    Color bg = {50, 50, 60, 230};
    Color text_color = {220, 220, 220, 255};

    if (bg.a > 0) {
        backend_render_fill_rect(&layer->rect, bg);
    }

    Texture* tex = render_text(layer, layer->text, text_color);
    if (!tex) return;

    int tw, th;
    backend_query_texture(tex, NULL, NULL, &tw, &th);
    Rect text_rect = {
        .x = layer->rect.x + 6,
        .y = layer->rect.y + 4,
        .w = tw / yui_density,
        .h = th / yui_density
    };
    backend_render_text_copy(tex, NULL, &text_rect);
    backend_render_text_destroy(tex);
}

static void show_tooltip(LabelComponent* comp, int mouse_x, int mouse_y) {
    if (!comp || !comp->has_overflow) return;
    if (comp->tooltip_popup) return;

    Layer* layer = comp->layer;
    if (!layer->font || !layer->font->default_font) return;

    const char* full_text = layer_get_text(layer);
    if (full_text[0] == '\0') return;

    // 测量文本宽度
    Texture* tex = backend_render_texture(layer->font->default_font, full_text, (Color){255,255,255,255});
    if (!tex) return;
    int tw, th;
    backend_query_texture(tex, NULL, NULL, &tw, &th);
    backend_render_text_destroy(tex);

    int tooltip_avail_w = layer->rect.w - (layer->rect.w > 10 ? 10 : 0);
    if (tw / yui_density <= tooltip_avail_w) return;

    // 创建 tooltip layer
    Layer* tl = malloc(sizeof(Layer));
    memset(tl, 0, sizeof(Layer));
    tl->type = LABEL;
    tl->visible = 1;
    tl->font = layer->font;

    int pad = 6;
    int sw = 800, sh = 600;
    backend_get_windowsize(&sw, &sh);

    tl->rect.x = mouse_x + 12;
    tl->rect.y = mouse_y + 12;
    tl->rect.w = tw / yui_density + pad * 2;
    tl->rect.h = th / yui_density + pad * 2;

    if (tl->rect.x + tl->rect.w > sw)
        tl->rect.x = mouse_x - tl->rect.w - 4;
    if (tl->rect.y + tl->rect.h > sh)
        tl->rect.y = mouse_y - tl->rect.h - 4;

    tl->text = strdup(full_text);
    tl->render = tooltip_layer_render;

    PopupLayer* popup = popup_layer_create(tl, POPUP_TYPE_TOOLTIP, 100);
    if (popup && popup_manager_add(popup)) {
        comp->tooltip_popup = tl;
    } else {
        if (tl->text) free(tl->text);
        free(tl);
    }
}

static void hide_tooltip(LabelComponent* comp) {
    if (comp->tooltip_popup) {
        Layer* tl = (Layer*)comp->tooltip_popup;
        comp->tooltip_popup = NULL;
        popup_manager_remove(tl);
        if (tl->text) free(tl->text);
        free(tl);
    }
}

// 创建标签组件
LabelComponent* label_component_create(Layer* layer) {
    return label_component_create_from_json(layer, NULL);
}

static void label_layer_destroy(Layer* layer) {
    if (!layer || !layer->component) return;
    label_component_destroy((LabelComponent*)layer->component);
    layer->component = NULL;
}

LabelComponent* label_component_create_from_json(Layer* layer, cJSON* json_obj) {
    if (!layer) return NULL;

    LabelComponent* component = malloc(sizeof(LabelComponent));
    if (!component) return NULL;

    memset(component, 0, sizeof(LabelComponent));
    component->layer = layer;
    component->text_alignment = LAYOUT_CENTER;
    component->auto_size = 0;
    component->icon_align = ICON_ALIGN_LEFT;
    component->icon_size = 0;
    component->icon_gap = 4;

    if (json_obj) {
        cJSON* text_align = cJSON_GetObjectItem(json_obj, "textAlign");
        if (text_align && cJSON_IsString(text_align)) {
            if (strcmp(text_align->valuestring, "left") == 0) {
                component->text_alignment = LAYOUT_LEFT;
            } else if (strcmp(text_align->valuestring, "right") == 0) {
                component->text_alignment = LAYOUT_RIGHT;
            } else if (strcmp(text_align->valuestring, "center") == 0) {
                component->text_alignment = LAYOUT_CENTER;
            }
        }

        cJSON* icon = cJSON_GetObjectItem(json_obj, "icon");
        if (icon && cJSON_IsString(icon) && icon->valuestring[0]) {
            component->icon_text = strdup(icon->valuestring);
        }

        cJSON* icon_align = cJSON_GetObjectItem(json_obj, "iconAlign");
        if (icon_align && cJSON_IsString(icon_align)) {
            if (strcmp(icon_align->valuestring, "right") == 0) {
                component->icon_align = ICON_ALIGN_RIGHT;
            } else if (strcmp(icon_align->valuestring, "top") == 0) {
                component->icon_align = ICON_ALIGN_TOP;
            } else if (strcmp(icon_align->valuestring, "bottom") == 0) {
                component->icon_align = ICON_ALIGN_BOTTOM;
            } else if (strcmp(icon_align->valuestring, "center") == 0) {
                component->icon_align = ICON_ALIGN_CENTER;
            }
        }

        cJSON* icon_size = cJSON_GetObjectItem(json_obj, "iconSize");
        if (icon_size && cJSON_IsNumber(icon_size)) {
            component->icon_size = icon_size->valueint;
        }

        cJSON* icon_gap = cJSON_GetObjectItem(json_obj, "iconGap");
        if (icon_gap && cJSON_IsNumber(icon_gap)) {
            component->icon_gap = icon_gap->valueint;
        }
    }

    layer->component = component;
    layer->render = label_component_render;
    layer->handle_pointer_event = label_component_handle_pointer_event;
    layer->on_destroy = label_layer_destroy;

    return component;
}

// 销毁标签组件
void label_component_destroy(LabelComponent* component) {
    if (component) {
        hide_tooltip(component);
        if (component->icon_text) free(component->icon_text);
        free(component);
    }
}

// 设置标签文本
void label_component_set_text(LabelComponent* component, const char* text) {
    if (!component || !text) return;
    layer_set_text(component->layer, text);

    if (component->auto_size && component->layer->font && component->layer->font->default_font) {
        int text_width, text_height;
        Texture* text_texture = backend_render_texture(component->layer->font->default_font, text, (Color){0, 0, 0, 255});
        if (text_texture) {
            backend_query_texture(text_texture, NULL, NULL, &text_width, &text_height);
            backend_render_text_destroy(text_texture);
            component->layer->rect.w = text_width / yui_density + 10;
            component->layer->rect.h = text_height / yui_density + 10;
        }
    }
}

// 设置文本对齐方式
void label_component_set_text_alignment(LabelComponent* component, int alignment) {
    if (!component) return;
    component->text_alignment = alignment;
}

// 设置自动调整大小
void label_component_set_auto_size(LabelComponent* component, int auto_size) {
    if (!component) return;
    component->auto_size = auto_size;

    const char* existing_text = layer_get_text(component->layer);
    if (auto_size && existing_text[0] != '\0' &&
        component->layer->font && component->layer->font->default_font) {
        label_component_set_text(component, existing_text);
    }
}

static int label_max_int(int a, int b) { return a > b ? a : b; }

// 渲染标签组件
void label_component_render(Layer* layer) {
    if (!layer || !layer->component) return;

    LabelComponent* component = (LabelComponent*)layer->component;

    // 绘制背景
    if (layer->bg_color.a > 0) {
        if (layer->radius > 0) {
            backend_render_rounded_rect(&layer->rect, layer->bg_color, layer->radius);
        } else {
            backend_render_fill_rect(&layer->rect, layer->bg_color);
        }
    }

    const char* original_text = layer_get_text(layer);
    int has_text = original_text && original_text[0] != '\0';
    int has_icon = component->icon_text && component->icon_text[0] != '\0';
    if (!has_text && !has_icon) return;

    Color text_color = layer->color;
    int density = yui_density;
    int pad_top = layer_padding_get(layer, 0);
    int pad_right = layer_padding_get(layer, 1);
    int pad_bottom = layer_padding_get(layer, 2);
    int pad_left = layer_padding_get(layer, 3);
    if (pad_top == 0 && pad_bottom == 0 && pad_left == 0 && pad_right == 0) {
        pad_top = 4; pad_bottom = 4; pad_left = 6; pad_right = 6;
    }

    // --- 测量图标 ---
    Texture* icon_tex = NULL;
    int icon_w = 0, icon_h = 0;
    if (has_icon) {
        icon_tex = render_text(layer, component->icon_text, text_color);
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
    }

    // --- 仅有图标 ---
    if (!has_text && has_icon) {
        int x = layer->rect.x + (layer->rect.w - icon_w) / 2;
        int y = layer->rect.y + (layer->rect.h - icon_h) / 2;
        Rect r = {x, y, icon_w, icon_h};
        backend_render_text_copy(icon_tex, NULL, &r);
        backend_render_text_destroy(icon_tex);
        return;
    }

    // --- 仅有文本（原逻辑不变） ---
    if (!has_icon) {
        component->has_overflow = 0;
        const char* display_text = original_text;

        if (layer->font && layer->font->default_font) {
            Texture* tex = backend_render_texture(layer->font->default_font, original_text, (Color){0,0,0,255});
            if (tex) {
                int tw;
                backend_query_texture(tex, NULL, NULL, &tw, NULL);
                backend_render_text_destroy(tex);

                int avail_w = layer->rect.w - (layer->rect.w > 10 ? 10 : 0);
                if (tw / density > avail_w) {
                    component->has_overflow = 1;
                    int byte_len = (int)strlen(original_text);
                    char* truncated = malloc((size_t)byte_len + 4);
                    while (byte_len > 0) {
                        int safe_len = utf8_safe_prefix_bytes(original_text, byte_len);
                        if (safe_len <= 0) break;
                        memcpy(truncated, original_text, (size_t)safe_len);
                        truncated[safe_len] = '\0';
                        strcat(truncated, "...");
                        Texture* short_tex = backend_render_texture(layer->font->default_font, truncated, (Color){0,0,0,255});
                        if (short_tex) {
                            int stw;
                            backend_query_texture(short_tex, NULL, NULL, &stw, NULL);
                            backend_render_text_destroy(short_tex);
                            if (stw / density <= avail_w) break;
                        }
                        byte_len = utf8_prev_prefix_bytes(original_text, safe_len);
                    }
                    if (byte_len > 0) {
                        Texture* final_tex = render_text(layer, truncated, text_color);
                        if (final_tex) {
                            int ftw, fth;
                            backend_query_texture(final_tex, NULL, NULL, &ftw, &fth);
                            Rect text_rect;
                            text_rect.h = fth / density;
                            text_rect.w = ftw / density;
                            text_rect.y = layer->rect.y + (layer->rect.h - text_rect.h) / 2;
                            switch (component->text_alignment) {
                                case LAYOUT_LEFT:
                                case LAYOUT_ALIGN_LEFT:
                                    text_rect.x = layer->rect.x + 5; break;
                                case LAYOUT_RIGHT:
                                case LAYOUT_ALIGN_RIGHT:
                                    text_rect.x = layer->rect.x + layer->rect.w - text_rect.w - 5; break;
                                default:
                                    text_rect.x = layer->rect.x + (layer->rect.w - text_rect.w) / 2; break;
                            }
                            if (text_rect.x < layer->rect.x) text_rect.x = layer->rect.x;
                            if (text_rect.y < layer->rect.y) text_rect.y = layer->rect.y;
                            backend_render_text_copy(final_tex, NULL, &text_rect);
                            backend_render_text_destroy(final_tex);
                        }
                        display_text = NULL;
                    }
                    free(truncated);
                }
            }
        }

        if (display_text) {
            Texture* text_texture = render_text(layer, display_text, text_color);
            if (text_texture) {
                int text_width, text_height;
                backend_query_texture(text_texture, NULL, NULL, &text_width, &text_height);
                Rect text_rect;
                int nat_w = text_width / density;
                int nat_h = text_height / density;
                text_rect.w = nat_w;
                text_rect.h = nat_h;
                text_rect.y = layer->rect.y + (layer->rect.h - text_rect.h) / 2;
                switch (component->text_alignment) {
                    case LAYOUT_LEFT:
                    case LAYOUT_ALIGN_LEFT:
                        text_rect.x = layer->rect.x + 5; break;
                    case LAYOUT_RIGHT:
                    case LAYOUT_ALIGN_RIGHT:
                        text_rect.x = layer->rect.x + layer->rect.w - text_rect.w - 5; break;
                    default:
                        text_rect.x = layer->rect.x + (layer->rect.w - text_rect.w) / 2; break;
                }
                if (text_rect.x < layer->rect.x) text_rect.x = layer->rect.x;
                if (text_rect.y < layer->rect.y) text_rect.y = layer->rect.y;
                if (layer->rect.w > 0 && text_rect.w > layer->rect.w) {
                    float s = (float)layer->rect.w / (float)text_rect.w;
                    text_rect.w = layer->rect.w;
                    text_rect.h = (int)(text_rect.h * s);
                    if (text_rect.h < 1) text_rect.h = 1;
                }
                if (layer->rect.h > 0 && text_rect.h > layer->rect.h) {
                    float s = (float)layer->rect.h / (float)text_rect.h;
                    text_rect.h = layer->rect.h;
                    text_rect.w = (int)(text_rect.w * s);
                    if (text_rect.w < 1) text_rect.w = 1;
                    text_rect.y = layer->rect.y;
                }
                backend_render_text_copy(text_texture, NULL, &text_rect);
                backend_render_text_destroy(text_texture);
            }
        }

        if (component->hovering && component->has_overflow) {
            Uint32 now = backend_get_ticks();
            if (now - component->hover_start >= TOOLTIP_DELAY_MS) {
                int mx, my;
                backend_get_pointer_state(&mx, &my);
                show_tooltip(component, mx, my);
            }
        }
        return;
    }

    // --- 图标 + 文本（使用 layer 原生 padding，支持 ... 截断） ---
    int gap = component->icon_gap;
    int content_w = layer->rect.w - pad_left - pad_right;
    if (content_w < 1) content_w = 1;
    int content_h = layer->rect.h - pad_top - pad_bottom;
    if (content_h < 1) content_h = 1;

    // 截断用辅助函数：测量带 "..." 的文本宽度密度像素
    int avail_text_w = 0;

    // 准备渲染文本（可能带 "..." 截断）
    Texture* text_tex = NULL;
    int draw_w = 0, draw_h = 0;

    // 先测量原始文本，确定是否需要截断（水平模式）
    if (component->icon_align == ICON_ALIGN_LEFT)
        avail_text_w = content_w - icon_w - gap;
    else if (component->icon_align == ICON_ALIGN_RIGHT)
        avail_text_w = content_w - icon_w - gap;
    else
        avail_text_w = content_w;

    if (avail_text_w < 8) avail_text_w = 8;

    {
        int need_trunc = 0;
        if (layer->font && layer->font->default_font) {
            Texture* mt = backend_render_texture(layer->font->default_font, original_text, (Color){0,0,0,255});
            if (mt) {
                int mw;
                backend_query_texture(mt, NULL, NULL, &mw, NULL);
                backend_render_text_destroy(mt);
                if (mw / density > avail_text_w) {
                    need_trunc = 1;
                }
            }
        }

        if (need_trunc) {
            int byte_len = (int)strlen(original_text);
            char* truncated = malloc((size_t)byte_len + 4);
            while (byte_len > 0) {
                int safe_len = utf8_safe_prefix_bytes(original_text, byte_len);
                if (safe_len <= 0) break;
                memcpy(truncated, original_text, (size_t)safe_len);
                truncated[safe_len] = '\0';
                strcat(truncated, "...");
                Texture* st = backend_render_texture(layer->font->default_font, truncated, (Color){0,0,0,255});
                if (st) {
                    int sw;
                    backend_query_texture(st, NULL, NULL, &sw, NULL);
                    backend_render_text_destroy(st);
                    if (sw / density <= avail_text_w) break;
                }
                byte_len = utf8_prev_prefix_bytes(original_text, safe_len);
            }
            if (byte_len > 0) {
                text_tex = render_text(layer, truncated, text_color);
                if (text_tex) {
                    int tw, th;
                    backend_query_texture(text_tex, NULL, NULL, &tw, &th);
                    draw_w = tw / density;
                    draw_h = th / density;
                    if (draw_w < 1) draw_w = 1;
                    if (draw_h < 1) draw_h = 1;
                }
            }
            free(truncated);
        } else {
            text_tex = render_text(layer, original_text, text_color);
            if (text_tex) {
                int tw, th;
                backend_query_texture(text_tex, NULL, NULL, &tw, &th);
                draw_w = tw / density;
                draw_h = th / density;
                if (draw_w < 1) draw_w = 1;
                if (draw_h < 1) draw_h = 1;
            }
        }
    }

    switch (component->icon_align) {
        case ICON_ALIGN_LEFT: {
            int block_w = icon_w + gap + draw_w;
            int block_h = label_max_int(icon_h, draw_h);
            int start_x = layer->rect.x + pad_left + (content_w - block_w) / 2;
            int start_y = layer->rect.y + pad_top + (content_h - block_h) / 2;
            Rect ir = {start_x, start_y + (block_h - icon_h) / 2, icon_w, icon_h};
            backend_render_text_copy(icon_tex, NULL, &ir);
            if (text_tex) {
                Rect tr = {start_x + icon_w + gap, start_y + (block_h - draw_h) / 2, draw_w, draw_h};
                backend_render_text_copy(text_tex, NULL, &tr);
            }
            break;
        }
        case ICON_ALIGN_RIGHT: {
            int block_w = draw_w + gap + icon_w;
            int block_h = label_max_int(icon_h, draw_h);
            int start_x = layer->rect.x + pad_left + (content_w - block_w) / 2;
            int start_y = layer->rect.y + pad_top + (content_h - block_h) / 2;
            if (text_tex) {
                Rect tr = {start_x, start_y + (block_h - draw_h) / 2, draw_w, draw_h};
                backend_render_text_copy(text_tex, NULL, &tr);
            }
            Rect ir = {start_x + draw_w + gap, start_y + (block_h - icon_h) / 2, icon_w, icon_h};
            backend_render_text_copy(icon_tex, NULL, &ir);
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
            int total_h = use_icon_h + gap + draw_h;
            if (total_h > content_h && draw_h > 0) {
                float s = (float)(content_h - use_icon_h - gap) / draw_h;
                if (s < 0.3f) s = 0.3f;
                draw_w = (int)(draw_w * s);
                draw_h = (int)(draw_h * s);
                if (draw_w < 1) draw_w = 1;
                if (draw_h < 1) draw_h = 1;
            }
            total_h = use_icon_h + gap + draw_h;
            int start_y = layer->rect.y + pad_top + (content_h - total_h) / 2;
            Rect ir = {layer->rect.x + (layer->rect.w - use_icon_w) / 2, start_y, use_icon_w, use_icon_h};
            backend_render_text_copy(icon_tex, NULL, &ir);
            if (text_tex) {
                Rect tr = {layer->rect.x + (layer->rect.w - draw_w) / 2, start_y + use_icon_h + gap, draw_w, draw_h};
                backend_render_text_copy(text_tex, NULL, &tr);
            }
            break;
        }
        case ICON_ALIGN_BOTTOM: {
            int total_h = draw_h + gap + icon_h;
            if (total_h > content_h && draw_h > 0) {
                float s = (float)(content_h - icon_h - gap) / draw_h;
                if (s < 0.3f) s = 0.3f;
                draw_w = (int)(draw_w * s);
                draw_h = (int)(draw_h * s);
                if (draw_w < 1) draw_w = 1;
                if (draw_h < 1) draw_h = 1;
            }
            total_h = draw_h + gap + icon_h;
            int start_y = layer->rect.y + pad_top + (content_h - total_h) / 2;
            if (text_tex) {
                Rect tr = {layer->rect.x + (layer->rect.w - draw_w) / 2, start_y, draw_w, draw_h};
                backend_render_text_copy(text_tex, NULL, &tr);
            }
            Rect ir = {layer->rect.x + (layer->rect.w - icon_w) / 2, start_y + draw_h + gap, icon_w, icon_h};
            backend_render_text_copy(icon_tex, NULL, &ir);
            break;
        }
    }

    if (text_tex) backend_render_text_destroy(text_tex);
    backend_render_text_destroy(icon_tex);
}

// 鼠标事件处理
int label_component_handle_pointer_event(Layer* layer, PointerEvent* event) {
    if (!layer || !layer->component) return 0;
    LabelComponent* comp = (LabelComponent*)layer->component;

    int inside = (event->x >= layer->rect.x && event->x <= layer->rect.x + layer->rect.w &&
                  event->y >= layer->rect.y && event->y <= layer->rect.y + layer->rect.h);

    if (event->phase == POINTER_MOVE) {
        if (inside && !comp->hovering) {
            // 鼠标进入
            comp->hovering = 1;
            comp->hover_start = backend_get_ticks();
        } else if (!inside && comp->hovering) {
            // 鼠标离开
            comp->hovering = 0;
            hide_tooltip(comp);
        }
    } else if (event->phase == POINTER_DOWN || event->phase == POINTER_UP) {
        if (!inside && comp->hovering) {
            comp->hovering = 0;
            hide_tooltip(comp);
        }
    }

    // 分发点击事件
    if (layer->event && layer->event->click) {
        if (event->phase == POINTER_UP && event->button == SDL_BUTTON_LEFT && inside) {
            EVENT_INVOKE(layer->event->click, layer);
        }
    }
    return 0;
}
