#include "backend.h"
#include "backend_common.h"
#include "component_registry.h"
#include "event.h"
#include "render.h"
#include "perf/perf.h"
#include "popup_manager.h"
#include "log.h"
#include "../../lib/lvgl/lv_port.h"

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

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
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

#define LVGL_MAX_TOUCHES 10
#define LVGL_SWIPE_THRESHOLD_PX 32

typedef struct {
    int fingerCount;
    int lastX[LVGL_MAX_TOUCHES];
    int lastY[LVGL_MAX_TOUCHES];
    Uint32 startTime;
    Uint32 lastTapTime;
    int tapCount;
    int longPressDetected;
} LvglTouchState;

static LvglTouchState g_touch_state;
static int g_pointer_drag_active = 0;
static int g_pointer_start_x = 0;
static int g_pointer_start_y = 0;
static int g_pointer_last_x = 0;
static int g_pointer_last_y = 0;
static int g_touch_swipe_start_x = 0;
static int g_touch_swipe_start_y = 0;

#ifdef __EMSCRIPTEN__
EM_JS(int, lvgl_emscripten_render_text_rgba, (const char* utf8, int font_px, int r, int g, int b, int a, int* out_w, int* out_h), {
    var text = UTF8ToString(utf8);
    if (!text) {
        return 0;
    }
    var family = 'Roboto, "Segoe UI", Arial, sans-serif';
    var px = font_px > 0 ? font_px : 16;
    var canvas = document.createElement('canvas');
    var ctx = canvas.getContext('2d');
    ctx.font = px + 'px ' + family;
    var metrics = ctx.measureText(text);
    var w = Math.ceil(metrics.width) + 4;
    var h = Math.ceil(px * 1.4);
    canvas.width = w;
    canvas.height = h;
    ctx.font = px + 'px ' + family;
    ctx.textBaseline = 'middle';
    ctx.textAlign = 'left';
    ctx.fillStyle = 'rgba(' + r + ',' + g + ',' + b + ',' + (a / 255) + ')';
    ctx.fillText(text, 2, h / 2);
    var img = ctx.getImageData(0, 0, w, h);
    var size = w * h * 4;
    var ptr = _malloc(size);
    if (!ptr) {
        return 0;
    }
    HEAPU8.set(img.data, ptr);
    setValue(out_w, w, 'i32');
    setValue(out_h, h, 'i32');
    return ptr;
});

static SDL_Surface* lvgl_emscripten_text_surface(const char* text, Color color, int target_px)
{
    int out_w = 0;
    int out_h = 0;
    int px;
    int ptr;
    SDL_Surface* wrapped;
    SDL_Surface* owned;

    if (!text || !text[0]) {
        return NULL;
    }

    if (color.a == 0) {
        color.a = 255;
    }

    px = (int)(target_px * yui_density + 0.5f);
    if (px < 12) {
        px = 12;
    }

    ptr = lvgl_emscripten_render_text_rgba(text, px, color.r, color.g, color.b, color.a,
                                          &out_w, &out_h);
    if (!ptr || out_w <= 0 || out_h <= 0) {
        if (ptr) {
            free((void*)(intptr_t)ptr);
        }
        return NULL;
    }

    wrapped = SDL_CreateRGBSurfaceWithFormatFrom(
        (void*)(intptr_t)ptr, out_w, out_h, 32, out_w * 4, SDL_PIXELFORMAT_RGBA32);
    if (!wrapped) {
        free((void*)(intptr_t)ptr);
        return NULL;
    }

    owned = SDL_ConvertSurfaceFormat(wrapped, SDL_PIXELFORMAT_ARGB8888, 0);
    SDL_FreeSurface(wrapped);
    free((void*)(intptr_t)ptr);
    if (!owned) {
        return NULL;
    }

    SDL_SetSurfaceBlendMode(owned, SDL_BLENDMODE_BLEND);
    return owned;
}
#endif

static void lvgl_pointer_to_logical(int* x, int* y)
{
    if (yui_density > 1.0f) {
        *x = (int)((float)(*x) / yui_density);
        *y = (int)((float)(*y) / yui_density);
    }
}

static void lvgl_finger_to_logical(const SDL_TouchFingerEvent* finger, int* x, int* y)
{
    int win_w = lv_port_get_width();
    int win_h = lv_port_get_height();
    if (win_w <= 0 || win_h <= 0) {
        *x = 0;
        *y = 0;
        return;
    }
    *x = (int)(finger->x * win_w);
    *y = (int)(finger->y * win_h);
}

static int lvgl_layer_scrollbar_dragging(const Layer* layer)
{
    if (!layer) {
        return 0;
    }
    if ((layer->scrollbar_v && layer->scrollbar_v->is_dragging) ||
        (layer->scrollbar_h && layer->scrollbar_h->is_dragging) ||
        (layer->scrollbar && layer->scrollbar->is_dragging)) {
        return 1;
    }
    return 0;
}

static int lvgl_any_scrollbar_dragging(const Layer* root)
{
    int i;

    if (!root) {
        return 0;
    }
    if (lvgl_layer_scrollbar_dragging(root)) {
        return 1;
    }
    for (i = 0; i < root->child_count; i++) {
        if (root->children[i] && lvgl_any_scrollbar_dragging(root->children[i])) {
            return 1;
        }
    }
    if (root->sub && lvgl_any_scrollbar_dragging(root->sub)) {
        return 1;
    }
    return 0;
}

static void lvgl_deliver_pointer_event(Layer* root, PointerEvent* pe)
{
    SDL_Point pos;
    int deliver;

    if (!root || !pe) {
        return;
    }

    pos.x = pe->x;
    pos.y = pe->y;
    deliver = (pe->phase == POINTER_MOVE || pe->phase == POINTER_UP) ||
              SDL_PointInRect(&pos, &root->rect);
    if (deliver) {
        handle_pointer_event(root, pe);
    }
}

static void lvgl_deliver_mouse_pointer(Layer* root, int x, int y, PointerPhase phase,
                                       Uint8 button, int dx, int dy)
{
    PointerEvent pe;
    memset(&pe, 0, sizeof(pe));
    pe.device = POINTER_DEVICE_MOUSE;
    pe.phase = phase;
    pe.x = x;
    pe.y = y;
    pe.button = button;
    pe.delta_x = dx;
    pe.delta_y = dy;
    lvgl_deliver_pointer_event(root, &pe);
}

static void lvgl_dispatch_touch(Layer* root, PointerPhase phase, int x, int y,
                                int dx, int dy, int pointer_id, int finger_count,
                                float scale, float rotation)
{
    PointerEvent pe;
    memset(&pe, 0, sizeof(pe));
    pe.device = POINTER_DEVICE_TOUCH;
    pe.phase = phase;
    pe.x = x;
    pe.y = y;
    pe.delta_x = dx;
    pe.delta_y = dy;
    pe.pointer_id = pointer_id;
    pe.finger_count = finger_count;
    pe.ext.gesture.scale = scale;
    pe.ext.gesture.rotation = rotation;
    handle_pointer_event(root, &pe);
}

static void lvgl_emit_swipe_event(Layer* root, int x, int y, int dx, int dy)
{
    int adx = dx < 0 ? -dx : dx;
    int ady = dy < 0 ? -dy : dy;

    if (adx < LVGL_SWIPE_THRESHOLD_PX || adx < ady) {
        return;
    }

    lvgl_dispatch_touch(root, POINTER_SWIPE, x, y, dx, dy, 0, 1, 0.0f, 0.0f);
}

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
    } else if (event->type == SDL_KEYUP) {
        KeyEvent key_event;
        key_event.type = KEY_EVENT_UP;
        key_event.data.key.key_code = event->key.keysym.sym;
        key_event.data.key.mod = event->key.keysym.mod;
        key_event.data.key.repeat = 0;
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
        PointerPhase phase = POINTER_MOVE;
        Uint8 button = SDL_BUTTON_LEFT;
        int clicks = 0;

        if (event->type == SDL_MOUSEBUTTONDOWN || event->type == SDL_MOUSEBUTTONUP) {
            mouse_x = event->button.x;
            mouse_y = event->button.y;
            button = event->button.button;
            clicks = event->button.clicks;
        } else {
            mouse_x = event->motion.x;
            mouse_y = event->motion.y;
        }
        lvgl_pointer_to_logical(&mouse_x, &mouse_y);

        {
            int delta_x = 0;
            int delta_y = 0;
            if (event->type == SDL_MOUSEBUTTONDOWN) {
                phase = (clicks >= 2) ? POINTER_DOUBLE_TAP : POINTER_DOWN;
                if (button == SDL_BUTTON_LEFT && phase == POINTER_DOWN) {
                    g_pointer_drag_active = 1;
                    g_pointer_start_x = mouse_x;
                    g_pointer_start_y = mouse_y;
                    g_pointer_last_x = mouse_x;
                    g_pointer_last_y = mouse_y;
                }
            } else if (event->type == SDL_MOUSEBUTTONUP) {
                phase = POINTER_UP;
                if (button == SDL_BUTTON_LEFT && g_pointer_drag_active) {
                    lvgl_emit_swipe_event(root, mouse_x, mouse_y,
                                          mouse_x - g_pointer_start_x,
                                          mouse_y - g_pointer_start_y);
                    g_pointer_drag_active = 0;
                }
            } else {
                if (g_pointer_drag_active) {
                    delta_x = mouse_x - g_pointer_last_x;
                    delta_y = mouse_y - g_pointer_last_y;
                    g_pointer_last_x = mouse_x;
                    g_pointer_last_y = mouse_y;
                }
            }

            lvgl_deliver_mouse_pointer(root, mouse_x, mouse_y, phase, button,
                                       delta_x, delta_y);
        }
    } else if (event->type == SDL_MOUSEWHEEL) {
        int mouse_x = 0;
        int mouse_y = 0;
        PointerEvent pe;
        SDL_GetMouseState(&mouse_x, &mouse_y);
        lvgl_pointer_to_logical(&mouse_x, &mouse_y);
        memset(&pe, 0, sizeof(pe));
        pe.device = POINTER_DEVICE_MOUSE;
        pe.phase = POINTER_WHEEL;
        pe.x = mouse_x;
        pe.y = mouse_y;
        pe.button = SDL_BUTTON_LEFT;
        pe.delta_x = event->wheel.x;
        pe.delta_y = -event->wheel.y;
        handle_pointer_event(root, &pe);
    } else if (event->type == SDL_FINGERDOWN) {
        int x;
        int y;

        lvgl_finger_to_logical(&event->tfinger, &x, &y);
        g_touch_state.fingerCount++;
        if (g_touch_state.fingerCount == 1) {
            g_touch_state.startTime = SDL_GetTicks();
            g_touch_state.longPressDetected = 0;
            g_touch_swipe_start_x = x;
            g_touch_swipe_start_y = y;
        }

        {
            int touchId = (int)(event->tfinger.fingerId % LVGL_MAX_TOUCHES);
            g_touch_state.lastX[touchId] = x;
            g_touch_state.lastY[touchId] = y;
            if (g_touch_state.fingerCount == 1) {
                lvgl_deliver_mouse_pointer(root, x, y, POINTER_DOWN, SDL_BUTTON_LEFT, 0, 0);
            }
            lvgl_dispatch_touch(root, POINTER_DOWN, x, y, 0, 0,
                                touchId, g_touch_state.fingerCount, 0.0f, 0.0f);
        }
    } else if (event->type == SDL_FINGERMOTION) {
        int x;
        int y;
        int touchId;
        int deltaX;
        int deltaY;

        lvgl_finger_to_logical(&event->tfinger, &x, &y);
        touchId = (int)(event->tfinger.fingerId % LVGL_MAX_TOUCHES);
        deltaX = x - g_touch_state.lastX[touchId];
        deltaY = y - g_touch_state.lastY[touchId];
        g_touch_state.lastX[touchId] = x;
        g_touch_state.lastY[touchId] = y;

        if (g_touch_state.fingerCount == 1) {
            lvgl_deliver_mouse_pointer(root, x, y, POINTER_MOVE, SDL_BUTTON_LEFT, 0, 0);
        }
        if (!lvgl_any_scrollbar_dragging(root)) {
            lvgl_dispatch_touch(root, POINTER_MOVE, x, y, deltaX, deltaY,
                                touchId, g_touch_state.fingerCount, 0.0f, 0.0f);
        }
    } else if (event->type == SDL_FINGERUP) {
        int x;
        int y;

        if (g_touch_state.fingerCount > 0) {
            g_touch_state.fingerCount--;
        }

        lvgl_finger_to_logical(&event->tfinger, &x, &y);

        {
            int touchId = (int)(event->tfinger.fingerId % LVGL_MAX_TOUCHES);
            lvgl_dispatch_touch(root, POINTER_UP, x, y, 0, 0,
                                touchId, g_touch_state.fingerCount, 0.0f, 0.0f);
        }

        if (g_touch_state.fingerCount == 0) {
            lvgl_emit_swipe_event(root, x, y,
                                  x - g_touch_swipe_start_x,
                                  y - g_touch_swipe_start_y);
            lvgl_deliver_mouse_pointer(root, x, y, POINTER_UP, SDL_BUTTON_LEFT, 0, 0);
        }
    } else if (event->type == SDL_MULTIGESTURE) {
        int winW = lv_port_get_width();
        int winH = lv_port_get_height();
        int cx, cy;
        int fingers;

        if (winW <= 0 || winH <= 0) {
            return;
        }
        cx = (int)(event->mgesture.x * winW);
        cy = (int)(event->mgesture.y * winH);
        fingers = (int)event->mgesture.numFingers;
        lvgl_pointer_to_logical(&cx, &cy);
        if (event->mgesture.dDist != 0.0f) {
            lvgl_dispatch_touch(root, POINTER_PINCH, cx, cy, 0, 0, -1, fingers,
                                1.0f + event->mgesture.dDist, 0.0f);
        }
        if (event->mgesture.dTheta != 0.0f) {
            lvgl_dispatch_touch(root, POINTER_ROTATE, cx, cy, 0, 0, -1, fingers,
                                1.0f, event->mgesture.dTheta);
        }
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
#ifdef __EMSCRIPTEN__
    FILE* f;
    long file_size;
    unsigned char* font_data;
    SDL_RWops* rw;
    TTF_Font* font;

    if (!path || !path[0] || size <= 0) {
        return NULL;
    }

    f = fopen(path, "rb");
    if (!f) {
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (file_size <= 0) {
        fclose(f);
        return NULL;
    }

    font_data = (unsigned char*)malloc((size_t)file_size);
    if (!font_data) {
        fclose(f);
        return NULL;
    }

    if (fread(font_data, 1, (size_t)file_size, f) != (size_t)file_size) {
        free(font_data);
        fclose(f);
        return NULL;
    }
    fclose(f);

    rw = SDL_RWFromConstMem(font_data, (int)file_size);
    if (!rw) {
        free(font_data);
        return NULL;
    }

    font = TTF_OpenFontRW(rw, 1, (int)(size * yui_density));
    return font;
#else
    if (!font_file_exists(path)) {
        return NULL;
    }
    return TTF_OpenFont(path, (int)(size * yui_density));
#endif
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

static void lvgl_blit_surface_to_fb(SDL_Surface* surface, const SDL_Rect* srcrect,
                                      const SDL_Rect* dstrect)
{
    uint32_t* fb;
    SDL_Surface* src_surface = surface;
    SDL_Surface* converted = NULL;
    int src_x;
    int src_y;
    int src_w;
    int src_h;
    int y;
    int x;

    fb = framebuffer();
    if (!fb || !surface || !dstrect) {
        return;
    }

    if (surface->format->format != SDL_PIXELFORMAT_ARGB8888) {
        converted = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_ARGB8888, 0);
        if (!converted) {
            return;
        }
        src_surface = converted;
    }

    src_x = srcrect ? srcrect->x : 0;
    src_y = srcrect ? srcrect->y : 0;
    src_w = srcrect ? srcrect->w : src_surface->w;
    src_h = srcrect ? srcrect->h : src_surface->h;

    {
        int dst_w = dstrect->w > 0 ? dstrect->w : src_w;
        int dst_h = dstrect->h > 0 ? dstrect->h : src_h;

        for (y = 0; y < dst_h; y++) {
            int sy = src_h > 0 ? (y * src_h) / dst_h : 0;
            int dy = dstrect->y + y;
            uint8_t* row;

            if (dy < 0 || dy >= fb_height()) {
                continue;
            }

            row = (uint8_t*)src_surface->pixels + (src_y + sy) * src_surface->pitch + src_x * 4;
            for (x = 0; x < dst_w; x++) {
                int sx = src_w > 0 ? (x * src_w) / dst_w : 0;
                int dx = dstrect->x + x;
                uint8_t* px;
                uint32_t src_pixel;
                uint32_t* dst_px;

                if (dx < 0 || dx >= fb_width()) {
                    continue;
                }

                px = row + sx * 4;
                src_pixel = ((uint32_t)px[3] << 24) | ((uint32_t)px[2] << 16)
                          | ((uint32_t)px[1] << 8) | (uint32_t)px[0];
                dst_px = fb + dy * fb_stride() + dx;
                *dst_px = fb_blend_pixel(*dst_px, src_pixel);
            }
        }
    }

    if (converted) {
        SDL_FreeSurface(converted);
    }
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
    {
        SDL_Renderer* renderer = lv_port_get_renderer();
        int render_w = 0;
        int render_h = 0;
        int logic_w = lv_port_get_width();
        if (renderer && logic_w > 0 &&
            SDL_GetRendererOutputSize(renderer, &render_w, &render_h) == 0) {
            backend_set_density((float)render_w / (float)logic_w);
        }
    }
#endif

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

static int g_lvgl_exit_code = 0;
static int g_lvgl_auto_frames = -1;

void backend_set_auto_frames(int frames) { g_lvgl_auto_frames = frames; }
void backend_request_quit(int exit_code)
{
    g_lvgl_exit_code = exit_code;
    g_running = 0;
}
int backend_get_exit_code(void) { return g_lvgl_exit_code; }
int backend_should_quit(void) { return !g_running; }
void backend_set_headless(int on) { (void)on; }
int backend_is_headless(void) { return 0; }

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

int backend_measure_text_width(DFont* font, const char* text)
{
#if defined(YUI_LVGL_PORT_SDL)
    int w = 0;
    int h = 0;

    if (!font || !text || !text[0]) {
        return 0;
    }
    if (TTF_SizeUTF8((TTF_Font*)font, text, &w, &h) == 0 && w > 0) {
        return (int)(((float)w / yui_density) + 0.5f);
    }
#endif
    (void)font;
    (void)text;
    return 0;
}

Texture* backend_render_texture(DFont* font, const char* text, Color color)
{
#if defined(YUI_LVGL_PORT_SDL)
    SDL_Surface* surface;
    int font_size_px = 16;

    if (!font || !text || text[0] == '\0') {
        return NULL;
    }

    font_size_px = TTF_FontHeight((TTF_Font*)font);
    if (font_size_px <= 0) {
        font_size_px = 16;
    }

#ifdef __EMSCRIPTEN__
    {
        float density = backend_get_density();
        if (density > 1.0f) {
            font_size_px = (int)(font_size_px / density + 0.5f);
        }
        if (font_size_px <= 0) {
            font_size_px = 16;
        }
    }
    surface = lvgl_emscripten_text_surface(text, color, font_size_px);
#else
    surface = TTF_RenderUTF8_Blended((TTF_Font*)font, text, color);
    if (!surface) {
        LOGE("backend_lvgl", "TTF_RenderUTF8_Blended failed: %s", TTF_GetError());
        return NULL;
    }
    {
        SDL_Surface* converted = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_ARGB8888, 0);
        SDL_FreeSurface(surface);
        surface = converted;
    }
#endif
    return (Texture*)surface;
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

void backend_get_pointer_state(int* x, int* y)
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
        SDL_FreeSurface((SDL_Surface*)texture);
    }
#else
    (void)texture;
#endif
}

void backend_render_text_copy(Texture* texture, const Rect* srcrect, const Rect* dstrect)
{
#if defined(YUI_LVGL_PORT_SDL)
    SDL_Rect src;
    SDL_Rect dst;
    SDL_Surface* surface = (SDL_Surface*)texture;

    if (!surface || !dstrect) {
        return;
    }

    src.x = 0;
    src.y = 0;
    src.w = surface->w;
    src.h = surface->h;
    if (srcrect) {
        src = *srcrect;
    }
    dst = *dstrect;
    lvgl_blit_surface_to_fb(surface, &src, &dst);
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
    SDL_Surface* surface = (SDL_Surface*)texture;

    if (!surface) {
        return -1;
    }
    if (format) {
        *format = surface->format ? surface->format->format : 0;
    }
    if (access) {
        *access = 0;
    }
    if (w) {
        *w = surface->w;
    }
    if (h) {
        *h = surface->h;
    }
    return 0;
#else
    (void)texture;
    (void)format;
    (void)access;
    if (w) *w = 0;
    if (h) *h = 0;
    return -1;
#endif
}

static void backend_lvgl_frame(void);

void backend_run(Layer* ui_root)
{
    g_ui_root = ui_root;
    g_running = 1;

#if defined(YUI_LVGL_PORT_SDL)
    lv_port_set_sdl_event_hook(lvgl_yui_sdl_event_hook, g_ui_root);
#endif

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(backend_lvgl_frame, 0, 1);
#else
    while (g_running) {
        backend_lvgl_frame();
        if (g_lvgl_auto_frames >= 0) {
            if (g_lvgl_auto_frames == 0) {
                g_running = 0;
            } else {
                g_lvgl_auto_frames--;
            }
        }
    }
#endif
}

static void backend_lvgl_frame(void)
{
    if (!g_running) {
#ifdef __EMSCRIPTEN__
        emscripten_cancel_main_loop();
#endif
        return;
    }

    lv_port_tick_inc();
    lv_port_input_poll();
    if (lv_port_should_quit()) {
        g_running = 0;
#ifdef __EMSCRIPTEN__
        emscripten_cancel_main_loop();
#endif
        return;
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
#if defined(YUI_LVGL_PORT_SDL) && !defined(__EMSCRIPTEN__)
    SDL_Delay(16);
#endif
}
