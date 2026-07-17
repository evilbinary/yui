#include "backend.h"
#include "backend_common.h"
#include "component_registry.h"
#include "event.h"
#include "render.h"
#include "perf/perf.h"
#include "popup_manager.h"
#include "log.h"
#include "../../lib/lvgl/lv_port.h"
#include "../../lib/lvgl/lvgl.h"

#ifdef YUI_HAS_LVGLMODULE
#include "../../lib/lvglmodule/lvgl_component.h"
#endif

#if defined(YUI_LVGL_PORT_SDL)
#ifdef D_SDL
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#else
#include <SDL.h>
#include <SDL_ttf.h>
#endif
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

float yui_density = 1.0f;

float backend_get_density(void) {
    return yui_density > 0.0f ? yui_density : 1.0f;
}

void backend_set_density(float density) {
    if (density > 0.0f) {
        yui_density = density;
    }
}

Layer* g_ui_root = NULL;
static int g_running = 0;
#if defined(YUI_LVGL_PORT_SDL)
static int g_ttf_ready = 0;
#endif

static UpdateCallback g_update_callbacks[16];
static int g_update_callback_count = 0;
static ResizeCallback g_resize_callback = NULL;

#if defined(YUI_LVGL_PORT_SDL)
static void lvgl_yui_sdl_event_hook(const SDL_Event* event, void* user_data)
{
    Layer* root = (Layer*)user_data;
    if (!root || !event) {
        return;
    }

    if (event->type == SDL_KEYDOWN) {
        KeyEvent key_event;
        key_event.type = KEY_EVENT_DOWN;
        key_event.data.key.key_code = event->key.keysym.sym;
        key_event.data.key.mod = event->key.keysym.mod;
        key_event.data.key.repeat = event->key.repeat;
        handle_key_event(root, &key_event);
    } else if (event->type == SDL_TEXTEDITING) {
        KeyEvent key_event;
        key_event.type = KEY_EVENT_TEXT_EDITING;
        strncpy(key_event.data.text.text, event->edit.text, sizeof(key_event.data.text.text) - 1);
        key_event.data.text.text[sizeof(key_event.data.text.text) - 1] = '\0';
        key_event.data.text.start = event->edit.start;
        key_event.data.text.length = event->edit.length;
        handle_key_event(root, &key_event);
    } else if (event->type == SDL_TEXTINPUT) {
        KeyEvent key_event;
        key_event.type = KEY_EVENT_TEXT_INPUT;
        strncpy(key_event.data.text.text, event->text.text, sizeof(key_event.data.text.text) - 1);
        key_event.data.text.text[sizeof(key_event.data.text.text) - 1] = '\0';
        handle_key_event(root, &key_event);
    } else if (event->type == SDL_MOUSEBUTTONDOWN || event->type == SDL_MOUSEBUTTONUP ||
        event->type == SDL_MOUSEMOTION) {
        int mouse_x;
        int mouse_y;

        if (event->type == SDL_MOUSEMOTION) {
            mouse_x = event->motion.x;
            mouse_y = event->motion.y;
            handle_scrollbar_drag_event(root, mouse_x, mouse_y, event->type);
        } else {
            MouseEvent mouse_event;
            int event_state;

            mouse_x = event->button.x;
            mouse_y = event->button.y;
            event_state = (event->type == SDL_MOUSEBUTTONDOWN) ? SDL_PRESSED : SDL_RELEASED;
            handle_scrollbar_drag_event(root, mouse_x, mouse_y, event->type);

            mouse_event.x = mouse_x;
            mouse_event.y = mouse_y;
            mouse_event.button = event->button.button;
            mouse_event.state = event_state;
            mouse_event.clicks = event->button.clicks;
            mouse_event.timestamp = SDL_GetTicks();
            handle_mouse_event(root, &mouse_event);
        }
    } else if (event->type == SDL_MOUSEWHEEL) {
        int mouse_x = 0;
        int mouse_y = 0;
        SDL_GetMouseState(&mouse_x, &mouse_y);
        handle_scroll_event(root, mouse_x, mouse_y, event->wheel.x, -event->wheel.y);
    }
}
#endif

#if defined(YUI_LVGL_PORT_SDL)
static uint32_t fb_blend_pixel(uint32_t dst, uint32_t src)
{
    uint8_t sa = (uint8_t)(src >> 24);
    if (sa == 0) {
        return dst;
    }
    if (sa == 255) {
        return src;
    }

    uint8_t sr = (uint8_t)((src >> 16) & 0xFF);
    uint8_t sg = (uint8_t)((src >> 8) & 0xFF);
    uint8_t sb = (uint8_t)(src & 0xFF);

    uint8_t dr = (uint8_t)((dst >> 16) & 0xFF);
    uint8_t dg = (uint8_t)((dst >> 8) & 0xFF);
    uint8_t db = (uint8_t)(dst & 0xFF);

    uint8_t inv = (uint8_t)(255 - sa);
    uint8_t r = (uint8_t)((sr * sa + dr * inv) / 255);
    uint8_t g = (uint8_t)((sg * sa + dg * inv) / 255);
    uint8_t b = (uint8_t)((sb * sa + db * inv) / 255);
    return ((uint32_t)255 << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

static int font_file_exists(const char* path)
{
    FILE* f;
    if (!path || !path[0]) {
        return 0;
    }
    f = fopen(path, "rb");
    if (!f) {
        return 0;
    }
    fclose(f);
    return 1;
}

static TTF_Font* open_font_at(const char* path, int size)
{
    if (!font_file_exists(path)) {
        return NULL;
    }
    return TTF_OpenFont(path, (int)(size * yui_density));
}

static void build_weighted_font_path(const char* font_path, const char* weight,
                                     char* out, size_t out_size)
{
    const char* suffix = NULL;

    if (!font_path || !out || out_size == 0) {
        return;
    }

    if (weight && strcmp(weight, "bold") == 0) {
        suffix = "-Bold";
    } else if (weight && strcmp(weight, "light") == 0) {
        suffix = "-Light";
    }

    if (!suffix || strstr(font_path, "Bold") || strstr(font_path, "bold")
        || strstr(font_path, "Light") || strstr(font_path, "light")) {
        snprintf(out, out_size, "%s", font_path);
        return;
    }

    {
        const char* ext = strrchr(font_path, '.');
        if (ext) {
            int base_len = (int)(ext - font_path);
            snprintf(out, out_size, "%.*s%s%s", base_len, font_path, suffix, ext);
        } else {
            snprintf(out, out_size, "%s%s", font_path, suffix);
        }
    }
}
#endif

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

#if defined(YUI_LVGL_PORT_SDL)
    if (TTF_Init() == -1) {
        LOGE("backend_lvgl", "TTF_Init failed: %s", TTF_GetError());
        return -1;
    }
    g_ttf_ready = 1;
#endif

#ifdef YUI_HAS_LVGLMODULE
    lvglmodule_register_all();
#endif

    return 0;
}

void backend_quit(void)
{
#if defined(YUI_LVGL_PORT_SDL)
    if (g_ttf_ready) {
        TTF_Quit();
        g_ttf_ready = 0;
    }
#endif
    lv_port_deinit();
}

void backend_set_ui_root(Layer* root)
{
    g_ui_root = root;
#if defined(YUI_LVGL_PORT_SDL)
    lv_port_set_sdl_event_hook(lvgl_yui_sdl_event_hook, g_ui_root);
#endif
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
#if defined(YUI_LVGL_PORT_SDL)
    SDL_Renderer* renderer;
    SDL_Surface* surface;
    SDL_Surface* converted;
    SDL_Texture* texture;

    if (!font || !text || text[0] == '\0') {
        return NULL;
    }

    renderer = lv_port_get_renderer();
    if (!renderer) {
        return NULL;
    }

    surface = TTF_RenderUTF8_Blended(font, text, color);
    if (!surface) {
        LOGE("backend_lvgl", "TTF_RenderUTF8_Blended failed: %s", TTF_GetError());
        return NULL;
    }

    converted = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_ARGB8888, 0);
    SDL_FreeSurface(surface);
    if (!converted) {
        return NULL;
    }

    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                SDL_TEXTUREACCESS_STREAMING,
                                converted->w, converted->h);
    if (!texture) {
        SDL_FreeSurface(converted);
        return NULL;
    }

    if (SDL_UpdateTexture(texture, NULL, converted->pixels, converted->pitch) != 0) {
        SDL_DestroyTexture(texture);
        SDL_FreeSurface(converted);
        return NULL;
    }

    SDL_FreeSurface(converted);
    return texture;
#else
    (void)font;
    (void)text;
    (void)color;
    return NULL;
#endif
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
    if (width > 0 && height > 0) {
        lv_port_resize(width, height);
    }
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
#if defined(YUI_LVGL_PORT_SDL)
    SDL_Delay((Uint32)delay);
#else
    (void)delay;
#endif
}

Uint32 backend_get_ticks(void)
{
#if defined(YUI_LVGL_PORT_SDL)
    return SDL_GetTicks();
#else
    return (Uint32)lv_tick_get();
#endif
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
    return backend_load_font_with_weight(font_path, size, "normal");
}

DFont* backend_load_font_with_weight(char* font_path, int size, const char* weight)
{
#if defined(YUI_LVGL_PORT_SDL)
    char weighted_path[MAX_PATH];
    TTF_Font* font = NULL;
    const char* candidates[12];
    int count = 0;
    int i;

    if (!g_ttf_ready || !font_path || size <= 0) {
        return NULL;
    }

    build_weighted_font_path(font_path, weight, weighted_path, sizeof(weighted_path));

    candidates[count++] = weighted_path;
    candidates[count++] = font_path;

    if (!weight || strcmp(weight, "normal") == 0) {
        candidates[count++] = "app/assets/Roboto-Regular.ttf";
        candidates[count++] = "assets/Roboto-Regular.ttf";
    } else if (strcmp(weight, "bold") == 0) {
        candidates[count++] = "app/assets/Roboto-Bold.ttf";
        candidates[count++] = "assets/Roboto-Bold.ttf";
    } else if (strcmp(weight, "light") == 0) {
        candidates[count++] = "app/assets/Roboto-Light.ttf";
        candidates[count++] = "assets/Roboto-Light.ttf";
    }

#if defined(_WIN32)
    candidates[count++] = "C:/Windows/Fonts/segoeui.ttf";
    candidates[count++] = "C:/Windows/Fonts/arial.ttf";
#elif defined(__APPLE__)
    candidates[count++] = "/System/Library/Fonts/Supplemental/Arial.ttf";
    candidates[count++] = "/System/Library/Fonts/Helvetica.ttc";
#else
    candidates[count++] = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";
#endif

    for (i = 0; i < count && !font; i++) {
        font = open_font_at(candidates[i], size);
        if (font) {
            LOGD("backend_lvgl", "loaded font: %s (size %d)", candidates[i], size);
        }
    }

    if (!font) {
        LOGE("backend_lvgl", "failed to load font: %s (size %d, weight %s)",
             font_path, size, weight ? weight : "normal");
    } else {
        TTF_SetFontHinting(font, TTF_HINTING_LIGHT);
    }

    return font;
#else
    (void)font_path;
    (void)size;
    (void)weight;
    return NULL;
#endif
}

void backend_render_text_destroy(Texture* texture)
{
#if defined(YUI_LVGL_PORT_SDL)
    if (texture) {
        SDL_DestroyTexture(texture);
    }
#else
    (void)texture;
#endif
}

void backend_render_text_copy(Texture* texture, const Rect* srcrect, const Rect* dstrect)
{
#if defined(YUI_LVGL_PORT_SDL)
    uint32_t* fb;
    int tex_w = 0;
    int tex_h = 0;
    SDL_Rect src;
    SDL_Rect dst;
    void* pixels = NULL;
    int pitch = 0;
    int y;
    int x;

    fb = framebuffer();
    if (!fb || !texture || !dstrect) {
        return;
    }

    if (SDL_QueryTexture(texture, NULL, NULL, &tex_w, &tex_h) != 0) {
        return;
    }

    src.x = 0;
    src.y = 0;
    src.w = tex_w;
    src.h = tex_h;
    if (srcrect) {
        src = *srcrect;
    }
    dst = *dstrect;

    if (SDL_LockTexture(texture, &src, &pixels, &pitch) != 0) {
        return;
    }

    for (y = 0; y < src.h; y++) {
        int dy = dst.y + y;
        if (dy < 0 || dy >= fb_height()) {
            continue;
        }
        for (x = 0; x < src.w; x++) {
            int dx = dst.x + x;
            uint8_t* px;
            uint32_t src_pixel;
            uint32_t* dst_px;

            if (dx < 0 || dx >= fb_width()) {
                continue;
            }

            px = (uint8_t*)pixels + y * pitch + x * 4;
            src_pixel = ((uint32_t)px[3] << 24) | ((uint32_t)px[2] << 16)
                      | ((uint32_t)px[1] << 8) | (uint32_t)px[0];
            dst_px = fb + dy * fb_stride() + dx;
            *dst_px = fb_blend_pixel(*dst_px, src_pixel);
        }
    }

    SDL_UnlockTexture(texture);
#else
    (void)texture;
    (void)srcrect;
    (void)dstrect;
#endif
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
int backend_screenshot(const char* path) { (void)path; return -1; }

int backend_query_texture(Texture* texture, Uint32* format, int* access, int* w, int* h)
{
#if defined(YUI_LVGL_PORT_SDL)
    if (!texture) {
        return -1;
    }
    return SDL_QueryTexture(texture, format, access, w, h);
#else
    (void)texture;
    (void)format;
    (void)access;
    if (w) *w = 0;
    if (h) *h = 0;
    return -1;
#endif
}

void backend_run(Layer* ui_root)
{
    g_ui_root = ui_root;
    g_running = 1;

#if defined(YUI_LVGL_PORT_SDL)
    lv_port_set_sdl_event_hook(lvgl_yui_sdl_event_hook, g_ui_root);
#endif

    while (g_running) {
        lv_port_tick_inc();
        lv_port_input_poll();
        if (lv_port_should_quit()) {
            g_running = 0;
            break;
        }

        for (int i = 0; i < g_update_callback_count; i++) {
            if (g_update_callbacks[i]) {
                g_update_callbacks[i]();
            }
        }

        backend_render_clear_color(30, 30, 30, 255);
        if (g_ui_root) {
            perf_frame_begin();
            perf_render_tree_begin();
            render_layer(g_ui_root);
            perf_render_tree_end();
            render_inspect_overlay(g_ui_root);
            perf_draw_overlay(g_ui_root);
            popup_manager_render();
            perf_frame_end();
        }

#if LV_USE_PERF_MONITOR || LV_USE_MEM_MONITOR
        {
            lv_disp_t* disp = lv_disp_get_default();
            if (disp) {
                lv_obj_invalidate(lv_disp_get_layer_sys(disp));
            }
        }
#endif

        lv_timer_handler();
        lv_port_flush();
#if defined(YUI_LVGL_PORT_SDL)
        SDL_Delay(16);
#endif
    }
}
