#include "label_component.h"
#include "../render.h"
#include "../backend.h"
#include "../popup_manager.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define TOOLTIP_DELAY_MS 400

static Layer* g_tooltip_layer = NULL;

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
        .w = tw / scale,
        .h = th / scale
    };
    backend_render_text_copy(tex, NULL, &text_rect);
    backend_render_text_destroy(tex);
}

static void show_tooltip(LabelComponent* comp, int mouse_x, int mouse_y) {
    if (!comp || !comp->has_overflow) return;
    if (g_tooltip_layer) return;

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

    if (tw / scale <= layer->rect.w) return;

    // 创建 tooltip layer
    g_tooltip_layer = malloc(sizeof(Layer));
    memset(g_tooltip_layer, 0, sizeof(Layer));
    g_tooltip_layer->type = LABEL;
    g_tooltip_layer->visible = 1;
    g_tooltip_layer->font = layer->font;

    int pad = 6;
    int sw = 800, sh = 600;
    backend_get_windowsize(&sw, &sh);

    g_tooltip_layer->rect.x = mouse_x + 12;
    g_tooltip_layer->rect.y = mouse_y + 12;
    g_tooltip_layer->rect.w = tw / scale + pad * 2;
    g_tooltip_layer->rect.h = th / scale + pad * 2;

    if (g_tooltip_layer->rect.x + g_tooltip_layer->rect.w > sw)
        g_tooltip_layer->rect.x = mouse_x - g_tooltip_layer->rect.w - 4;
    if (g_tooltip_layer->rect.y + g_tooltip_layer->rect.h > sh)
        g_tooltip_layer->rect.y = mouse_y - g_tooltip_layer->rect.h - 4;

    g_tooltip_layer->text = strdup(full_text);
    g_tooltip_layer->render = tooltip_layer_render;

    PopupLayer* popup = popup_layer_create(g_tooltip_layer, POPUP_TYPE_TOOLTIP, 100);
    if (popup) {
        popup_manager_add(popup);
    }
}

static void hide_tooltip(void) {
    if (g_tooltip_layer) {
        popup_manager_remove(g_tooltip_layer);
        if (g_tooltip_layer->text) free(g_tooltip_layer->text);
        free(g_tooltip_layer);
        g_tooltip_layer = NULL;
    }
}

// 创建标签组件
LabelComponent* label_component_create(Layer* layer) {
    if (!layer) return NULL;

    LabelComponent* component = malloc(sizeof(LabelComponent));
    if (!component) return NULL;

    memset(component, 0, sizeof(LabelComponent));
    component->layer = layer;
    component->text_alignment = LAYOUT_CENTER;
    component->auto_size = 0;

    layer->component = component;
    layer->render = label_component_render;
    layer->handle_mouse_event = label_component_handle_mouse_event;

    return component;
}

// 销毁标签组件
void label_component_destroy(LabelComponent* component) {
    if (component) {
        hide_tooltip();
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
            component->layer->rect.w = text_width / scale + 10;
            component->layer->rect.h = text_height / scale + 10;
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

    // 获取文本
    const char* original_text = layer_get_text(layer);
    if (original_text[0] == '\0') return;

    component->has_overflow = 0;
    const char* display_text = original_text;

    // 测量文本宽度，过长则截断加 "..."
    if (layer->font && layer->font->default_font) {
        Texture* tex = backend_render_texture(layer->font->default_font, original_text, (Color){0,0,0,255});
        if (tex) {
            int tw;
            backend_query_texture(tex, NULL, NULL, &tw, NULL);
            backend_render_text_destroy(tex);

            int avail_w = layer->rect.w - (layer->rect.w > 10 ? 10 : 0);
            if (tw / scale > avail_w) {
                component->has_overflow = 1;
                // 截断文本: 从完整长度逐步缩短直到加上 "..." 能放进
                int len = strlen(original_text);
                char* truncated = malloc(len + 4);
                while (len > 0) {
                    memcpy(truncated, original_text, len);
                    truncated[len] = '\0';
                    strcat(truncated, "...");
                    Texture* short_tex = backend_render_texture(layer->font->default_font, truncated, (Color){0,0,0,255});
                    if (short_tex) {
                        int stw;
                        backend_query_texture(short_tex, NULL, NULL, &stw, NULL);
                        backend_render_text_destroy(short_tex);
                        if (stw / scale <= avail_w) break;
                    }
                    len--;
                }
                if (len > 0) {
                    Color text_color = layer->color;
                    Texture* final_tex = render_text(layer, truncated, text_color);
                    if (final_tex) {
                        int ftw, fth;
                        backend_query_texture(final_tex, NULL, NULL, &ftw, &fth);
                        Rect text_rect;
                        text_rect.h = fth / scale;
                        text_rect.w = ftw / scale;
                        text_rect.y = layer->rect.y + (layer->rect.h - text_rect.h) / 2;

                        switch (component->text_alignment) {
                            case LAYOUT_LEFT:
                            case LAYOUT_ALIGN_LEFT:
                                text_rect.x = layer->rect.x + 5;
                                break;
                            case LAYOUT_RIGHT:
                            case LAYOUT_ALIGN_RIGHT:
                                text_rect.x = layer->rect.x + layer->rect.w - text_rect.w - 5;
                                break;
                            default:
                                text_rect.x = layer->rect.x + (layer->rect.w - text_rect.w) / 2;
                                break;
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
        // 正常渲染文本（无需截断）
        Color text_color = layer->color;
        Texture* text_texture = render_text(layer, display_text, text_color);
        if (text_texture) {
            int text_width, text_height;
            backend_query_texture(text_texture, NULL, NULL, &text_width, &text_height);

            Rect text_rect;
            text_rect.h = text_height / scale;
            text_rect.w = text_width / scale;
            text_rect.y = layer->rect.y + (layer->rect.h - text_rect.h) / 2;

            switch (component->text_alignment) {
                case LAYOUT_LEFT:
                case LAYOUT_ALIGN_LEFT:
                    text_rect.x = layer->rect.x + 5;
                    break;
                case LAYOUT_RIGHT:
                case LAYOUT_ALIGN_RIGHT:
                    text_rect.x = layer->rect.x + layer->rect.w - text_rect.w - 5;
                    break;
                default:
                    text_rect.x = layer->rect.x + (layer->rect.w - text_rect.w) / 2;
                    break;
            }

            if (text_rect.x < layer->rect.x) text_rect.x = layer->rect.x;
            if (text_rect.x + text_rect.w > layer->rect.x + layer->rect.w)
                text_rect.w = layer->rect.x + layer->rect.w - text_rect.x;
            if (text_rect.y < layer->rect.y) text_rect.y = layer->rect.y;
            if (text_rect.y + text_rect.h > layer->rect.y + layer->rect.h)
                text_rect.h = layer->rect.y + layer->rect.h - text_rect.y;

            backend_render_text_copy(text_texture, NULL, &text_rect);
            backend_render_text_destroy(text_texture);
        }
    }

    // 检查是否要显示 tooltip
    if (component->hovering && component->has_overflow) {
        Uint32 now = backend_get_ticks();
        if (now - component->hover_start >= TOOLTIP_DELAY_MS) {
            int mx, my;
            backend_get_mouse_state(&mx, &my);
            show_tooltip(component, mx, my);
        }
    }
}

// 鼠标事件处理
void label_component_handle_mouse_event(Layer* layer, MouseEvent* event) {
    if (!layer || !layer->component) return;
    LabelComponent* comp = (LabelComponent*)layer->component;

    int inside = (event->x >= layer->rect.x && event->x <= layer->rect.x + layer->rect.w &&
                  event->y >= layer->rect.y && event->y <= layer->rect.y + layer->rect.h);

    if (event->state == SDL_MOUSEMOTION) {
        if (inside && !comp->hovering) {
            // 鼠标进入
            comp->hovering = 1;
            comp->hover_start = backend_get_ticks();
        } else if (!inside && comp->hovering) {
            // 鼠标离开
            comp->hovering = 0;
            hide_tooltip();
        }
    } else if (event->state == SDL_PRESSED || event->state == SDL_RELEASED) {
        if (!inside && comp->hovering) {
            comp->hovering = 0;
            hide_tooltip();
        }
    }
}
