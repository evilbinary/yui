#include "backend.h"
#include "backend_common.h"
#include "component_registry.h"
#include "event.h"
#include "render.h"
#include "popup_manager.h"
#include "log.h"
#include "../../lib/lvgl/port_sdl/lv_port.h"

#ifdef YUI_HAS_LVGLMODULE
#include "../../lib/lvglmodule/lvgl_component.h"
#endif

#ifdef D_SDL
#include <SDL2/SDL.h>
#else
#include <SDL.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

float scale = 1.0f;
static Layer* g_ui_root = NULL;
static int g_running = 0;

static UpdateCallback g_update_callbacks[16];
static int g_update_callback_count = 0;
static ResizeCallback g_resize_callback = NULL;

static uint32_t* framebuffer(void)
{
    return (uint32_t*)lv_port_get_draw_buf();
}

static int fb_width(void)
{
    return lv_port_get_width();
}

static int fb_height(void)
{
    return lv_port_get_height();
}

static int fb_stride(void)
{
    return lv_port_get_stride();
}

static void fb_fill_rect(const Rect* rect, Color color)
{
    uint32_t* fb = framebuffer();
    if (!fb || !rect) {
        return;
    }

    int x0 = rect->x;
    int y0 = rect->y;
    int x1 = rect->x + rect->w;
    int y1 = rect->y + rect->h;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > fb_width()) x1 = fb_width();
    if (y1 > fb_height()) y1 = fb_height();

    uint32_t pixel = backend_color_to_pixel(color, 32);
    for (int y = y0; y < y1; y++) {
        uint32_t* row = fb + y * fb_stride() + x0;
        for (int x = x0; x < x1; x++) {
            *row++ = pixel;
        }
    }
}

int backend_init(void)
{
    yui_component_registry_init();
    yui_components_register_builtin();

    if (lv_port_init() != 0) {
        LOGE("backend_lvgl", "lv_port_init failed");
        return -1;
    }
#ifdef YUI_HAS_LVGLMODULE
    lvglmodule_register_all();
#endif

    return 0;
}

void backend_quit(void)
{
    lv_port_deinit();
}

void backend_set_ui_root(Layer* root)
{
    g_ui_root = root;
}

Texture* backend_load_texture(char* path)
{
    (void)path;
    return NULL;
}

Texture* backend_load_texture_from_base64(const char* base64_data, size_t data_len)
{
    (void)base64_data;
    (void)data_len;
    return NULL;
}

Texture* backend_render_texture(DFont* font, const char* text, Color color)
{
    (void)font;
    (void)text;
    (void)color;
    return NULL;
}

void backend_render_fill_rect(Rect* rect, Color color)
{
    fb_fill_rect(rect, color);
}

void backend_render_rect(Rect* rect, Color color)
{
    backend_render_fill_rect(rect, color);
}

void backend_render_rect_color(Rect* rect, unsigned char r, unsigned char g,
                               unsigned char b, unsigned char a)
{
    Color color = {(Uint8)r, (Uint8)g, (Uint8)b, (Uint8)a};
    backend_render_fill_rect(rect, color);
}

void backend_render_fill_rect_color(Rect* rect, unsigned char r, unsigned char g,
                                    unsigned char b, unsigned char a)
{
    backend_render_rect_color(rect, r, g, b, a);
}

void backend_get_windowsize(int* width, int* height)
{
    if (width) *width = fb_width();
    if (height) *height = fb_height();
}

void backend_set_windowsize(int width, int height)
{
    (void)width;
    (void)height;
}

void backend_set_window_size(char* title)
{
    (void)title;
}

void backend_set_resizable(int resizable)
{
    (void)resizable;
}

void backend_set_minimum_windowsize(int width, int height)
{
    (void)width;
    (void)height;
}

void backend_render_present(void)
{
    lv_port_flush();
}

void backend_delay(int delay)
{
    SDL_Delay((Uint32)delay);
}

Uint32 backend_get_ticks(void)
{
    return SDL_GetTicks();
}

void backend_get_mouse_state(int* x, int* y)
{
    if (x) *x = 0;
    if (y) *y = 0;
}

void backend_render_clear_color(unsigned char r, unsigned char g,
                                unsigned char b, unsigned char a)
{
    Rect full = {0, 0, fb_width(), fb_height()};
    Color color = {r, g, b, a};
    fb_fill_rect(&full, color);
}

DFont* backend_load_font(char* font_path, int size)
{
    (void)font_path;
    (void)size;
    return NULL;
}

DFont* backend_load_font_with_weight(char* font_path, int size, const char* weight)
{
    (void)font_path;
    (void)size;
    (void)weight;
    return NULL;
}

void backend_render_text_destroy(Texture* texture)
{
    (void)texture;
}

void backend_render_text_copy(Texture* texture, const Rect* srcrect, const Rect* dstrect)
{
    (void)texture;
    (void)srcrect;
    (void)dstrect;
}

void backend_render_get_clip_rect(Rect* prev_clip)
{
    if (prev_clip) {
        prev_clip->x = 0;
        prev_clip->y = 0;
        prev_clip->w = fb_width();
        prev_clip->h = fb_height();
    }
}

void backend_render_set_clip_rect(Rect* clip)
{
    backend_clip_push(clip);
}

void backend_register_update_callback(UpdateCallback callback)
{
    if (!callback || g_update_callback_count >= 16) {
        return;
    }
    g_update_callbacks[g_update_callback_count++] = callback;
}

void backend_set_resize_callback(ResizeCallback callback)
{
    g_resize_callback = callback;
}

void backend_render_rounded_rect(Rect* rect, Color color, int radius)
{
    (void)radius;
    backend_render_fill_rect(rect, color);
}

void backend_render_rounded_rect_color(Rect* rect, unsigned char r, unsigned char g,
                                       unsigned char b, unsigned char a, int radius)
{
    (void)radius;
    backend_render_rect_color(rect, r, g, b, a);
}

void backend_render_rounded_rect_with_border(Rect* rect, Color bg_color, int radius,
                                             int border_width, Color border_color)
{
    (void)radius;
    (void)border_width;
    backend_render_fill_rect(rect, bg_color);
    (void)border_color;
}

void backend_render_line(int x1, int y1, int x2, int y2, Color color)
{
    (void)x1;
    (void)y1;
    (void)x2;
    (void)y2;
    (void)color;
}

void backend_render_arc(int center_x, int center_y, int radius, float start_angle,
                        float end_angle, Color color, int line_width)
{
    (void)center_x;
    (void)center_y;
    (void)radius;
    (void)start_angle;
    (void)end_angle;
    (void)color;
    (void)line_width;
}

void backend_render_backdrop_filter(Rect* rect, int blur_radius, float saturation,
                                    float brightness)
{
    (void)rect;
    (void)blur_radius;
    (void)saturation;
    (void)brightness;
}

char* backend_get_clipboard_text(void)
{
    return NULL;
}

void backend_set_clipboard_text(const char* text)
{
    (void)text;
}

void backend_start_text_input(void) {}
void backend_stop_text_input(void) {}
void backend_set_text_input_rect(Rect* rect) { (void)rect; }
void backend_set_titlebar_color(Color bg, Color text) { (void)bg; (void)text; }
void backend_set_window_icon(const char* path) { (void)path; }
void backend_set_font_fallback_path(const char* path) { (void)path; }
int backend_has_font_fallback(void) { return 0; }
int backend_screenshot(const char* path) { (void)path; return -1; }

int backend_query_texture(Texture* texture, Uint32* format, int* access, int* w, int* h)
{
    (void)texture;
    (void)format;
    (void)access;
    if (w) *w = 0;
    if (h) *h = 0;
    return -1;
}

void backend_run(Layer* ui_root)
{
    g_ui_root = ui_root;
    g_running = 1;

    while (g_running) {
        lv_port_tick_inc();
        lv_port_input_poll();

        for (int i = 0; i < g_update_callback_count; i++) {
            if (g_update_callbacks[i]) {
                g_update_callbacks[i]();
            }
        }

        backend_render_clear_color(30, 30, 30, 255);
        if (g_ui_root) {
            render_layer(g_ui_root);
            popup_manager_render();
        }

        lv_timer_handler();
        lv_port_flush();
        SDL_Delay(16);
    }
}
