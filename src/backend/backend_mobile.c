#include "backend.h"
#include "component_registry.h"
#include "event.h"
#include "popup_manager.h"
#include "render.h"
#include "perf/perf.h"
#include "ytype.h"

#include <stdlib.h>
#include <string.h>

#define MOBILE_DEFAULT_W 360
#define MOBILE_DEFAULT_H 640
#define MAX_UPDATE_CALLBACKS 16
#define MAX_TOUCHES 10

float scale = 1.0f;

static Layer* g_ui_root = NULL;
static void* g_native_surface = NULL;
static int g_window_w = MOBILE_DEFAULT_W;
static int g_window_h = MOBILE_DEFAULT_H;
static UpdateCallback g_update_callbacks[MAX_UPDATE_CALLBACKS];
static int g_update_callback_count = 0;
static ResizeCallback g_resize_callback = NULL;

typedef struct {
    int active;
    int x;
    int y;
} MobileTouchState;

static MobileTouchState g_touches[MAX_TOUCHES];

void backend_mobile_set_native_surface(void* native_surface) {
    g_native_surface = native_surface;
}

void backend_mobile_on_touch(int pointer_id, int x, int y, int phase) {
    TouchEvent event;
    MobileTouchState* touch;
    int id = pointer_id;

    if (id < 0 || id >= MAX_TOUCHES || !g_ui_root) {
        return;
    }

    touch = &g_touches[id];
    memset(&event, 0, sizeof(event));
    event.x = x;
    event.y = y;

    if (phase == 0) {
        event.type = TOUCH_TYPE_START;
        touch->active = 1;
        touch->x = x;
        touch->y = y;
    } else if (phase == 1) {
        if (!touch->active) {
            return;
        }
        event.type = TOUCH_TYPE_MOVE;
        event.deltaX = x - touch->x;
        event.deltaY = y - touch->y;
        touch->x = x;
        touch->y = y;
    } else {
        if (!touch->active) {
            return;
        }
        event.type = TOUCH_TYPE_END;
        touch->active = 0;
    }

    handle_touch_event(g_ui_root, &event);
}

int backend_init(void) {
    yui_component_registry_init();
    yui_components_register_builtin();
    (void)g_native_surface;
    return 0;
}

void backend_quit(void) {
    g_ui_root = NULL;
    g_native_surface = NULL;
    g_update_callback_count = 0;
    g_resize_callback = NULL;
    memset(g_touches, 0, sizeof(g_touches));
}

Texture* backend_load_texture(char* path) {
    (void)path;
    return NULL;
}

Texture* backend_load_texture_from_base64(const char* base64_data, size_t data_len) {
    (void)base64_data;
    (void)data_len;
    return NULL;
}

Texture* backend_render_texture(DFont* font, const char* text, Color color) {
    (void)font;
    (void)text;
    (void)color;
    return NULL;
}

void backend_render_fill_rect(Rect* rect, Color color) {
    backend_render_fill_rect_color(rect, color.r, color.g, color.b, color.a);
}

void backend_render_fill_rect_color(Rect* rect, unsigned char r, unsigned char g,
                                    unsigned char b, unsigned char a) {
    (void)rect;
    (void)r;
    (void)g;
    (void)b;
    (void)a;
    /* TODO: Skia fillRect */
}

void backend_render_rect(Rect* rect, Color color) {
    backend_render_rect_color(rect, color.r, color.g, color.b, color.a);
}

void backend_render_rect_color(Rect* rect, unsigned char r, unsigned char g,
                               unsigned char b, unsigned char a) {
    (void)rect;
    (void)r;
    (void)g;
    (void)b;
    (void)a;
}

void backend_render_rounded_rect(Rect* rect, Color color, int radius) {
    backend_render_rounded_rect_color(rect, color.r, color.g, color.b, color.a, radius);
}

void backend_render_rounded_rect_color(Rect* rect, unsigned char r, unsigned char g,
                                       unsigned char b, unsigned char a, int radius) {
    (void)rect;
    (void)r;
    (void)g;
    (void)b;
    (void)a;
    (void)radius;
}

void backend_render_rounded_rect_with_border(Rect* rect, Color bg_color, int radius,
                                             int border_width, Color border_color) {
    (void)rect;
    (void)bg_color;
    (void)radius;
    (void)border_width;
    (void)border_color;
}

void backend_render_line(int x1, int y1, int x2, int y2, Color color) {
    (void)x1;
    (void)y1;
    (void)x2;
    (void)y2;
    (void)color;
}

void backend_render_bezier_cubic(int x0, int y0, int cx1, int cy1, int cx2, int cy2,
                                 int x1, int y1, Color color, int width) {
    (void)x0;
    (void)y0;
    (void)cx1;
    (void)cy1;
    (void)cx2;
    (void)cy2;
    (void)x1;
    (void)y1;
    (void)color;
    (void)width;
}

void backend_render_arc(int center_x, int center_y, int radius, float start_angle,
                        float end_angle, Color color, int line_width) {
    (void)center_x;
    (void)center_y;
    (void)radius;
    (void)start_angle;
    (void)end_angle;
    (void)color;
    (void)line_width;
}

void backend_render_backdrop_filter(Rect* rect, int blur_radius, float saturation,
                                    float brightness) {
    (void)rect;
    (void)blur_radius;
    (void)saturation;
    (void)brightness;
}

void backend_get_windowsize(int* width, int* height) {
    if (width) {
        *width = g_window_w;
    }
    if (height) {
        *height = g_window_h;
    }
}

void backend_set_windowsize(int width, int height) {
    if (width > 0) {
        g_window_w = width;
    }
    if (height > 0) {
        g_window_h = height;
    }
    if (g_ui_root && g_resize_callback) {
        g_resize_callback(g_ui_root, g_window_w, g_window_h);
    }
}

void backend_set_window_size(char* title) {
    (void)title;
}

void backend_set_resizable(int resizable) {
    (void)resizable;
}

void backend_set_minimum_windowsize(int width, int height) {
    (void)width;
    (void)height;
}

void backend_render_present(void) {
    /* TODO: Skia flush + swap / Metal present */
}

void backend_delay(int delay) {
    (void)delay;
}

Uint32 backend_get_ticks(void) {
    return 0;
}

void backend_get_mouse_state(int* x, int* y) {
    if (x) {
        *x = 0;
    }
    if (y) {
        *y = 0;
    }
}

void backend_render_clear_color(unsigned char r, unsigned char g, unsigned char b,
                                unsigned char a) {
    (void)r;
    (void)g;
    (void)b;
    (void)a;
}

DFont* backend_load_font(char* font_path, int size) {
    return backend_load_font_with_weight(font_path, size, "normal");
}

DFont* backend_load_font_with_weight(char* font_path, int size, const char* weight) {
    DFont* font;
    (void)font_path;
    (void)weight;
    font = (DFont*)calloc(1, sizeof(DFont));
    if (font) {
        font->size = size;
    }
    return font;
}

void backend_render_text_destroy(Texture* texture) {
    if (texture) {
        free(texture);
    }
}

void backend_render_text_copy(Texture* texture, const Rect* srcrect, const Rect* dstrect) {
    (void)texture;
    (void)srcrect;
    (void)dstrect;
}

void backend_render_get_clip_rect(Rect* prev_clip) {
    if (prev_clip) {
        prev_clip->x = 0;
        prev_clip->y = 0;
        prev_clip->w = g_window_w;
        prev_clip->h = g_window_h;
    }
}

void backend_render_set_clip_rect(Rect* clip) {
    (void)clip;
}

void backend_register_update_callback(UpdateCallback callback) {
    if (callback && g_update_callback_count < MAX_UPDATE_CALLBACKS) {
        g_update_callbacks[g_update_callback_count++] = callback;
    }
}

void backend_set_resize_callback(ResizeCallback callback) {
    g_resize_callback = callback;
}

void backend_tick(Layer* ui_root) {
    int i;

    if (!ui_root) {
        return;
    }

    g_ui_root = ui_root;

    for (i = 0; i < g_update_callback_count; i++) {
        if (g_update_callbacks[i]) {
            g_update_callbacks[i]();
        }
    }

    backend_render_clear_color(30, 30, 30, 255);
    perf_frame_begin();
    perf_render_tree_begin();
    render_layer(ui_root);
    perf_render_tree_end();
    render_inspect_overlay(ui_root);
    perf_draw_overlay(ui_root);
    popup_manager_render();
    perf_frame_end();
    backend_render_present();
}

void backend_run(Layer* ui_root) {
    g_ui_root = ui_root;
    backend_tick(ui_root);
}

int backend_query_texture(Texture* texture, Uint32* format, int* access, int* w, int* h) {
    if (!texture) {
        return -1;
    }
    if (format) {
        *format = 0;
    }
    if (access) {
        *access = 0;
    }
    if (w) {
        *w = texture->w;
    }
    if (h) {
        *h = texture->h;
    }
    return 0;
}

char* backend_get_clipboard_text(void) {
    return strdup("");
}

void backend_set_clipboard_text(const char* text) {
    (void)text;
}

void backend_start_text_input(void) {}
void backend_stop_text_input(void) {}
void backend_set_text_input_rect(Rect* rect) { (void)rect; }

void backend_set_titlebar_color(Color bg, Color text) {
    (void)bg;
    (void)text;
}

void backend_set_window_icon(const char* path) {
    (void)path;
}

void backend_set_font_fallback_path(const char* path) {
    (void)path;
}

int backend_has_font_fallback(void) {
    return 0;
}

void backend_texture_cache_invalidate(void) {}
void backend_texture_cache_pin(DFont* font, const char* text, Color color) {
    (void)font;
    (void)text;
    (void)color;
}
void backend_texture_cache_warmup(DFont* font, const char** texts, int count, Color color) {
    (void)font;
    (void)texts;
    (void)count;
    (void)color;
}

int backend_screenshot(const char* path) {
    (void)path;
    return -1;
}

void backend_set_ui_root(Layer* root) {
    g_ui_root = root;
}
