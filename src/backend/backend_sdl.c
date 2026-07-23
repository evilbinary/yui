#include "backend.h"
#include "component_registry.h"
#include "event.h"
#include "render.h"
#include "perf/perf.h"
#include "ytype.h"
#include "util.h"
#include "popup_manager.h"
#include "screenshot.h"
#include "log.h"
#include <stdbool.h>  // 添加支持bool类型
#include <math.h>     // 添加数学函数支持
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#if defined(_WIN32) && !defined(__EMSCRIPTEN__)
#define YUI_WIN32_NATIVE 1
#include <windows.h>
#include <SDL_syswm.h>
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")
#endif

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#define WINDOW_WIDTH 1000
#define MAX_TOUCHES 10
#define MAX_UPDATE_CALLBACKS 16
#define MAX_TEXTURE_CACHE_ENTRIES 200

// ====================== 全局渲染器 ======================
SDL_Renderer* renderer = NULL;
float yui_density=1.0f;
SDL_Window* window=NULL;
DFont* default_font=NULL;
Layer* g_ui_root = NULL;
int g_running=0;

static int g_auto_frames = -1; /* -1 = run forever */
static int g_request_quit = 0;
static int g_exit_code = 0;
static int g_headless = -1; /* -1 = unset (read YUI_HEADLESS), 0/1 = explicit */

void backend_set_headless(int on)
{
    g_headless = on ? 1 : 0;
}

int backend_is_headless(void)
{
    const char* env;

    if (g_headless >= 0) {
        return g_headless;
    }
    env = getenv("YUI_HEADLESS");
    if (env && env[0] != '\0' && strcmp(env, "0") != 0) {
        return 1;
    }
    return 0;
}

void backend_set_auto_frames(int frames)
{
    g_auto_frames = frames;
    g_request_quit = 0;
    g_exit_code = 0;
}

void backend_request_quit(int exit_code)
{
    g_exit_code = exit_code;
    g_request_quit = 1;
    g_running = 0;
}

int backend_get_exit_code(void)
{
    return g_exit_code;
}

int backend_should_quit(void)
{
    return g_request_quit;
}

// 主循环更新回调管理
static UpdateCallback update_callbacks[MAX_UPDATE_CALLBACKS] = {NULL};
static int update_callback_count = 0;
static ResizeCallback resize_callback = NULL;

// 字体缓存结构
typedef struct {
    char font_path[MAX_PATH];
    int size;
    char weight[32];  // "normal", "bold", "light"
    TTF_Font* font;
    Uint32 last_used;
} FontCacheEntry;

#define MAX_FONT_CACHE_ENTRIES 150
FontCacheEntry font_cache[MAX_FONT_CACHE_ENTRIES] = {0};
int font_cache_initialized = 0;

// 纹理缓存结构
typedef struct {
    uint64_t key_hash;
    DFont* font;
    int font_size;
    int scale_milli;
    uint16_t text_len;
    char text[256];
    Color color;
    SDL_Texture* texture;
    int width;
    int height;
    Uint32 last_used;
    uint8_t pinned;
} TextureCacheEntry;

TextureCacheEntry texture_cache[MAX_TEXTURE_CACHE_ENTRIES] = {0};
int texture_cache_initialized = 0;
static int texture_cache_scale_milli = -1;

static uint64_t backend_texture_cache_fnv1a_u64(uint64_t h, uint64_t v) {
    h ^= v;
    h *= 1099511628211ULL;
    return h;
}

static uint64_t backend_texture_cache_fnv1a_bytes(uint64_t h, const unsigned char* data, size_t len) {
    size_t i;
    for (i = 0; i < len; i++) {
        h = backend_texture_cache_fnv1a_u64(h, data[i]);
    }
    return h;
}

static int backend_texture_cache_scale_key(void) {
    return (int)(yui_density * 1000.0f + 0.5f);
}

static uint64_t backend_texture_cache_make_hash(DFont* font, int font_size, Color color, const char* text) {
    uint64_t h = 14695981039346656037ULL;
    uintptr_t font_addr = (uintptr_t)font;

    h = backend_texture_cache_fnv1a_u64(h, (uint64_t)font_addr);
    h = backend_texture_cache_fnv1a_u64(h, (uint64_t)(uint32_t)font_size);
    h = backend_texture_cache_fnv1a_u64(h, (uint64_t)(uint32_t)backend_texture_cache_scale_key());
    h = backend_texture_cache_fnv1a_u64(h, color.r);
    h = backend_texture_cache_fnv1a_u64(h, color.g);
    h = backend_texture_cache_fnv1a_u64(h, color.b);
    h = backend_texture_cache_fnv1a_u64(h, color.a);
    if (text) {
        h = backend_texture_cache_fnv1a_bytes(h, (const unsigned char*)text, strlen(text));
    }
    return h;
}

static int backend_texture_cache_entry_matches(const TextureCacheEntry* entry, uint64_t key_hash,
                                               DFont* font, int font_size, Color color, const char* text) {
    if (!entry->texture || entry->key_hash != key_hash || entry->font != font || entry->font_size != font_size) {
        return 0;
    }
    if (entry->scale_milli != backend_texture_cache_scale_key()) {
        return 0;
    }
    if (entry->color.r != color.r || entry->color.g != color.g ||
        entry->color.b != color.b || entry->color.a != color.a) {
        return 0;
    }
    {
        size_t len = text ? strlen(text) : 0;
        if (entry->text_len != len) {
            return 0;
        }
        if (len >= sizeof(entry->text)) {
            return strncmp(entry->text, text, sizeof(entry->text) - 1) == 0;
        }
        return strcmp(entry->text, text) == 0;
    }
}

static int backend_text_cache_volatile(const char* text) {
    int len;
    int volatile_chars = 0;
    int i;

    if (!text) {
        return 0;
    }
    len = (int)strlen(text);
    if (len < 4) {
        return 0;
    }
    for (i = 0; text[i]; i++) {
        char c = text[i];
        if ((c >= '0' && c <= '9') || c == ':' || c == '.' || c == ',' || c == '-' || c == '%') {
            volatile_chars++;
        }
    }
    return volatile_chars * 2 >= len;
}

static void backend_texture_cache_sync_scale(void) {
    int now = backend_texture_cache_scale_key();

    if (!texture_cache_initialized) {
        texture_cache_scale_milli = now;
        return;
    }
    if (texture_cache_scale_milli == now) {
        return;
    }
    texture_cache_scale_milli = now;
    backend_texture_cache_invalidate();
}

static int backend_texture_cache_find_index(uint64_t key_hash, DFont* font, int font_size,
                                            Color color, const char* text) {
    int i;

    for (i = 0; i < MAX_TEXTURE_CACHE_ENTRIES; i++) {
        if (!texture_cache[i].texture || texture_cache[i].key_hash != key_hash) {
            continue;
        }
        if (backend_texture_cache_entry_matches(&texture_cache[i], key_hash, font, font_size, color, text)) {
            return i;
        }
    }
    return -1;
}

static int backend_texture_cache_pick_evict_index(void) {
    int i;
    int cache_index = -1;
    Uint32 oldest_time = SDL_GetTicks();

    for (i = 0; i < MAX_TEXTURE_CACHE_ENTRIES; i++) {
        if (!texture_cache[i].texture) {
            return i;
        }
    }

    for (i = 0; i < MAX_TEXTURE_CACHE_ENTRIES; i++) {
        if (texture_cache[i].pinned) {
            continue;
        }
        if (texture_cache[i].last_used <= oldest_time) {
            oldest_time = texture_cache[i].last_used;
            cache_index = i;
        }
    }
    return cache_index;
}

static void backend_texture_cache_store_entry(int cache_index, uint64_t key_hash, DFont* font,
                                              int font_size, const char* text, Color color,
                                              SDL_Texture* texture, int width, int height, int pinned) {
    if (cache_index < 0 || cache_index >= MAX_TEXTURE_CACHE_ENTRIES) {
        return;
    }
    if (texture_cache[cache_index].texture && texture_cache[cache_index].texture != texture) {
        SDL_DestroyTexture(texture_cache[cache_index].texture);
    }

    texture_cache[cache_index].key_hash = key_hash;
    texture_cache[cache_index].font = font;
    texture_cache[cache_index].font_size = font_size;
    texture_cache[cache_index].scale_milli = backend_texture_cache_scale_key();
    texture_cache[cache_index].text_len = text ? (uint16_t)strlen(text) : 0;
    strncpy(texture_cache[cache_index].text, text, sizeof(texture_cache[cache_index].text) - 1);
    texture_cache[cache_index].text[sizeof(texture_cache[cache_index].text) - 1] = '\0';
    texture_cache[cache_index].color = color;
    texture_cache[cache_index].texture = texture;
    texture_cache[cache_index].width = width;
    texture_cache[cache_index].height = height;
    texture_cache[cache_index].last_used = SDL_GetTicks();
    texture_cache[cache_index].pinned = pinned ? 1 : 0;
}

static char g_font_fallback_path[MAX_PATH] = "";

void backend_set_font_fallback_path(const char* path) {
    if (!path) {
        g_font_fallback_path[0] = '\0';
        return;
    }
    strncpy(g_font_fallback_path, path, sizeof(g_font_fallback_path) - 1);
    g_font_fallback_path[sizeof(g_font_fallback_path) - 1] = '\0';
}

int backend_has_font_fallback(void) {
    return g_font_fallback_path[0] != '\0';
}

static int backend_get_font_size(DFont* font) {
    int i;

    if (!font) {
        return 16;
    }

    for (i = 0; i < MAX_FONT_CACHE_ENTRIES; i++) {
        if (font_cache[i].font == font) {
            return font_cache[i].size;
        }
    }

    return 16;
}

static DFont* backend_get_fallback_font_for(DFont* primary) {
    DFont* fallback;

    if (!backend_has_font_fallback() || !primary) {
        return NULL;
    }

#ifdef __EMSCRIPTEN__
    /* wasm: CBDT 彩色 emoji 字体无法用 SDL_ttf 可靠渲染，改由浏览器 Canvas 处理 */
    if (g_font_fallback_path[0] &&
        (strstr(g_font_fallback_path, "ColorEmoji") != NULL ||
         strstr(g_font_fallback_path, "NotoEmoji") != NULL)) {
        return NULL;
    }
#endif

#ifdef __EMSCRIPTEN__
    if (g_font_fallback_path[0]) {
        fallback = backend_load_font_with_weight(g_font_fallback_path,
                                                 backend_get_font_size(primary), "normal");
        if (fallback) {
            return fallback;
        }
    }
    fallback = backend_load_font_with_weight("app/assets/NotoEmoji-Regular.ttf",
                                             backend_get_font_size(primary), "normal");
    if (fallback) {
        return fallback;
    }
    fallback = backend_load_font_with_weight("app/assets/NotoSansSymbols2-Regular.ttf",
                                             backend_get_font_size(primary), "normal");
    if (fallback) {
        return fallback;
    }
#endif

    return backend_load_font_with_weight(g_font_fallback_path, backend_get_font_size(primary), "normal");
}

// 返回渲染应使用的单一字体；返回 NULL 表示文本需要混合排版。
// out_fallback 始终接收 fallback 字体（混合路径需要），调用方自行判 NULL。
static DFont* backend_resolve_render_font(DFont* primary, const char* text, DFont** out_fallback) {
    DFont* fallback;
    const char* cursor;
    int all_in_primary = 1;
    int all_in_fallback = 1;
    int any_in_primary = 0;

    if (out_fallback) {
        *out_fallback = NULL;
    }

    if (!primary || !text || !backend_has_font_fallback()) {
        return primary;
    }

    fallback = backend_get_fallback_font_for(primary);
    if (out_fallback) {
        *out_fallback = fallback;
    }
    if (!fallback || fallback == primary) {
        return primary;
    }

    cursor = text;
    while (*cursor) {
        Uint32 codepoint = 0;

        if (!utf8_decode_codepoint(&cursor, &codepoint)) {
            break;
        }
        if (TTF_GlyphIsProvided32(primary, codepoint)) {
            any_in_primary = 1;
        } else {
            all_in_primary = 0;
        }
        if (!TTF_GlyphIsProvided32(fallback, codepoint)) {
            all_in_fallback = 0;
        }
    }

    if (all_in_primary) {
        return primary;
    }
    // 仅当主字体完全缺字时才整串用 fallback（如纯 emoji）。
    // emoji 字体通常也含 ASCII，不能因 all_in_fallback 就把 "🔋 85%" 整串交给 fallback，否则数字会乱码。
    if (all_in_fallback && !any_in_primary) {
        return fallback;
    }
    return NULL;  // 混合：需要分段合成
}

static DFont* backend_pick_font_for_codepoint(DFont* primary, DFont* fallback, Uint32 codepoint) {
    if (primary && TTF_GlyphIsProvided32(primary, codepoint)) {
        return primary;
    }
    if (fallback && TTF_GlyphIsProvided32(fallback, codepoint)) {
        return fallback;
    }
    return NULL;
}

static int backend_text_fits_font(DFont* font, const char* text) {
    int w = 0;
    int h = 0;

    if (!font || !text || !text[0]) {
        return 0;
    }
    return TTF_SizeUTF8(font, text, &w, &h) == 0 && w > 0 && h > 0;
}

static SDL_Surface* backend_render_blended_utf8(DFont* font, const char* text, SDL_Color color) {
    if (!backend_text_fits_font(font, text)) {
        return NULL;
    }
    return TTF_RenderUTF8_Blended(font, text, color);
}

#ifdef __EMSCRIPTEN__

static int backend_is_emoji_codepoint(Uint32 codepoint) {
    return (codepoint >= 0x1F300 && codepoint <= 0x1FAFF)
        || (codepoint >= 0x2600 && codepoint <= 0x27BF)
        || (codepoint >= 0x2300 && codepoint <= 0x23FF)
        || (codepoint >= 0x2B50 && codepoint <= 0x2B55)
        || codepoint == 0x2764;
}

static int backend_is_emoji_run_codepoint(Uint32 codepoint) {
    return codepoint == 0x200D || codepoint == 0xFE0F || backend_is_emoji_codepoint(codepoint);
}

static int backend_text_contains_emoji(const char* text) {
    const char* cursor = text;

    if (!text) {
        return 0;
    }

    while (*cursor) {
        Uint32 codepoint = 0;

        if (!utf8_decode_codepoint(&cursor, &codepoint)) {
            break;
        }
        if (backend_is_emoji_codepoint(codepoint)) {
            return 1;
        }
    }
    return 0;
}

/* 用浏览器系统 emoji 字体渲染，返回 malloc 的 RGBA 像素（调用方 free） */
EM_JS(int, emscripten_render_emoji_rgba, (const char* utf8, int font_px, int* out_w, int* out_h), {
    var text = UTF8ToString(utf8);
    if (!text) {
        return 0;
    }
    if (!window.__yuiEmojiCanvas) {
        window.__yuiEmojiCanvas = document.createElement('canvas');
        window.__yuiEmojiCtx = window.__yuiEmojiCanvas.getContext('2d', { willReadFrequently: true });
    }
    var canvas = window.__yuiEmojiCanvas;
    var ctx = window.__yuiEmojiCtx;
    var px = Math.max(12, font_px | 0);
    var family = '"Segoe UI Emoji", "Apple Color Emoji", "Noto Color Emoji", sans-serif';
    ctx.font = px + 'px ' + family;
    var metrics = ctx.measureText(text);
    var w = Math.max(1, Math.ceil(metrics.width) + 4);
    var h = Math.max(1, px + 4);
    canvas.width = w;
    canvas.height = h;
    ctx.clearRect(0, 0, w, h);
    ctx.font = px + 'px ' + family;
    ctx.textBaseline = 'middle';
    ctx.textAlign = 'center';
    ctx.fillText(text, w / 2, h / 2);
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

static void backend_clean_surface_alpha_fringe(SDL_Surface* surface, Uint8 alpha_threshold) {
    int x;
    int y;

    if (!surface || !surface->pixels || !surface->format) {
        return;
    }

    if (SDL_MUSTLOCK(surface)) {
        SDL_LockSurface(surface);
    }

    for (y = 0; y < surface->h; y++) {
        Uint8* row = (Uint8*)surface->pixels + y * surface->pitch;
        for (x = 0; x < surface->w; x++) {
            Uint32* pixel = (Uint32*)(row + x * 4);
            Uint8 r = 0;
            Uint8 g = 0;
            Uint8 b = 0;
            Uint8 a = 0;

            SDL_GetRGBA(*pixel, surface->format, &r, &g, &b, &a);
            if (a < alpha_threshold) {
                *pixel = SDL_MapRGBA(surface->format, 0, 0, 0, 0);
            }
        }
    }

    if (SDL_MUSTLOCK(surface)) {
        SDL_UnlockSurface(surface);
    }
}

/* 用浏览器系统字体渲染彩色文字，返回 malloc 的 RGBA 像素（调用方 free） */
EM_JS(int, emscripten_render_text_rgba, (const char* utf8, int font_px, int r, int g, int b, int a, int* out_w, int* out_h), {
    var text = UTF8ToString(utf8);
    if (!text) {
        return 0;
    }
    if (!window.__yuiTextCanvas) {
        window.__yuiTextCanvas = document.createElement('canvas');
        window.__yuiTextCtx = window.__yuiTextCanvas.getContext('2d', { willReadFrequently: true });
    }
    var canvas = window.__yuiTextCanvas;
    var ctx = window.__yuiTextCtx;
    var px = Math.max(12, font_px | 0);
    var family = 'system-ui, -apple-system, "Segoe UI", "Microsoft YaHei", "PingFang SC", "Noto Sans SC", sans-serif';
    ctx.font = px + 'px ' + family;
    var metrics = ctx.measureText(text);
    var w = Math.max(1, Math.ceil(metrics.width) + 4);
    var h = Math.max(1, Math.ceil(px * 1.25) + 4);
    canvas.width = w;
    canvas.height = h;
    ctx.clearRect(0, 0, w, h);
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

static SDL_Surface* backend_render_emscripten_canvas_surface(const char* text, SDL_Color color, int target_px) {
    int out_w = 0;
    int out_h = 0;
    int px;
    int ptr;
    SDL_Surface* wrapped;
    SDL_Surface* owned;

    if (!text || !text[0]) {
        return NULL;
    }

    px = (int)(target_px * yui_density + 0.5f);
    if (px < 12) {
        px = 12;
    }

    ptr = emscripten_render_text_rgba(text, px, color.r, color.g, color.b, color.a, &out_w, &out_h);
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

    owned = SDL_ConvertSurfaceFormat(wrapped, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(wrapped);
    free((void*)(intptr_t)ptr);
    if (!owned) {
        return NULL;
    }

    backend_clean_surface_alpha_fringe(owned, 24);
    SDL_SetSurfaceBlendMode(owned, SDL_BLENDMODE_BLEND);
    return owned;
}

static SDL_Surface* backend_render_emscripten_emoji_surface(const char* text, int target_px) {
    int out_w = 0;
    int out_h = 0;
    int px;
    int ptr;
    SDL_Surface* wrapped;
    SDL_Surface* owned;

    if (!text || !text[0]) {
        return NULL;
    }

    px = (int)(target_px * yui_density + 0.5f);
    if (px < 12) {
        px = 12;
    }

    ptr = emscripten_render_emoji_rgba(text, px, &out_w, &out_h);
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

    owned = SDL_ConvertSurfaceFormat(wrapped, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(wrapped);
    free((void*)(intptr_t)ptr);
    if (!owned) {
        return NULL;
    }

    backend_clean_surface_alpha_fringe(owned, 24);
    SDL_SetSurfaceBlendMode(owned, SDL_BLENDMODE_BLEND);
    return owned;
}

static SDL_Surface* backend_render_emscripten_text_surface(DFont* font, const char* text, SDL_Color color, int target_px) {
    const char* cursor = text;
    const char* run_starts[128];
    const char* run_ends[128];
    int run_is_emoji[128];
    SDL_Surface* surfaces[128];
    int run_count = 0;
    int total_w = 0;
    int max_h = 0;
    int x = 0;
    char run_buf[256];
    SDL_Surface* final_surface = NULL;

    if (!text || !text[0]) {
        return NULL;
    }

    for (int i = 0; i < 128; i++) {
        surfaces[i] = NULL;
    }

    while (*cursor && run_count < 128) {
        const char* run_start = cursor;
        Uint32 codepoint = 0;
        int is_emoji;

        if (!utf8_decode_codepoint(&cursor, &codepoint)) {
            break;
        }
        is_emoji = backend_is_emoji_run_codepoint(codepoint);
        if (run_count > 0 && run_is_emoji[run_count - 1] == is_emoji) {
            run_ends[run_count - 1] = cursor;
        } else {
            run_starts[run_count] = run_start;
            run_ends[run_count] = cursor;
            run_is_emoji[run_count] = is_emoji;
            run_count++;
        }
    }

    for (int i = 0; i < run_count; i++) {
        int len = (int)(run_ends[i] - run_starts[i]);
        SDL_Surface* surf;

        if (len <= 0 || len >= (int)sizeof(run_buf)) {
            continue;
        }
        memcpy(run_buf, run_starts[i], (size_t)len);
        run_buf[len] = '\0';

        if (run_is_emoji[i]) {
            surf = backend_render_emscripten_emoji_surface(run_buf, target_px);
        } else {
            surf = backend_render_blended_utf8(font, run_buf, color);
            if (!surf) {
                surf = backend_render_emscripten_canvas_surface(run_buf, color, target_px);
            }
        }
        if (!surf) {
            continue;
        }
        surfaces[i] = surf;
        total_w += surf->w;
        if (surf->h > max_h) {
            max_h = surf->h;
        }
    }

    if (total_w <= 0 || max_h <= 0) {
        for (int i = 0; i < run_count; i++) {
            if (surfaces[i]) {
                SDL_FreeSurface(surfaces[i]);
            }
        }
        return NULL;
    }

    final_surface = SDL_CreateRGBSurfaceWithFormat(0, total_w, max_h, 32, SDL_PIXELFORMAT_RGBA32);
    if (!final_surface) {
        for (int i = 0; i < run_count; i++) {
            if (surfaces[i]) {
                SDL_FreeSurface(surfaces[i]);
            }
        }
        return NULL;
    }

    SDL_FillRect(final_surface, NULL, SDL_MapRGBA(final_surface->format, 0, 0, 0, 0));
    for (int i = 0; i < run_count; i++) {
        SDL_Surface* surf = surfaces[i];
        SDL_Rect dst;

        if (!surf) {
            continue;
        }
        dst.x = x;
        dst.y = (max_h - surf->h) / 2;
        dst.w = surf->w;
        dst.h = surf->h;
        SDL_BlitSurface(surf, NULL, final_surface, &dst);
        x += surf->w;
        SDL_FreeSurface(surf);
    }
    return final_surface;
}

#endif /* __EMSCRIPTEN__ */

void handle_event(Layer* root, SDL_Event* event);

static void backend_apply_display_scale(void);

static void backend_handle_window_resize(Layer* root) {
    if (!root || !window || !resize_callback) return;
    int w = 0, h = 0;
    SDL_GetWindowSize(window, &w, &h);
    if (w <= 0 || h <= 0) return;
    backend_apply_display_scale();
    resize_callback(root, w, h);
}

#ifdef __EMSCRIPTEN__
void backend_main_loop(void) {
    if (!g_ui_root || !g_running) {
        return;
    }

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            g_running = 0;
            emscripten_cancel_main_loop();
            return;
        }
        handle_event(g_ui_root, &event);
    }

    // 调用所有注册的更新回调
    for (int i = 0; i < update_callback_count; i++) {
        if (update_callbacks[i]) {
            update_callbacks[i]();
        }
    }

    SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
    SDL_RenderClear(renderer);

    perf_frame_begin();
    perf_render_tree_begin();
    render_layer(g_ui_root);
    perf_render_tree_end();
    render_inspect_overlay(g_ui_root);
    perf_draw_overlay(g_ui_root);

    // 渲染弹出层
    popup_manager_render();
    perf_frame_end();

    SDL_RenderPresent(renderer);
}
#endif

// ====================== 纹理缓存管理 ======================
void init_texture_cache() {
    int i;

    if (texture_cache_initialized) {
        return;
    }

    for (i = 0; i < MAX_TEXTURE_CACHE_ENTRIES; i++) {
        texture_cache[i].key_hash = 0;
        texture_cache[i].font = NULL;
        texture_cache[i].font_size = 0;
        texture_cache[i].scale_milli = 0;
        texture_cache[i].text_len = 0;
        texture_cache[i].text[0] = '\0';
        texture_cache[i].texture = NULL;
        texture_cache[i].width = 0;
        texture_cache[i].height = 0;
        texture_cache[i].last_used = 0;
        texture_cache[i].pinned = 0;
    }

    texture_cache_scale_milli = backend_texture_cache_scale_key();
    texture_cache_initialized = 1;
}

void cleanup_texture_cache() {
    int i;

    for (i = 0; i < MAX_TEXTURE_CACHE_ENTRIES; i++) {
        if (texture_cache[i].texture) {
            SDL_DestroyTexture(texture_cache[i].texture);
            texture_cache[i].texture = NULL;
        }
        texture_cache[i].key_hash = 0;
        texture_cache[i].font = NULL;
        texture_cache[i].font_size = 0;
        texture_cache[i].scale_milli = 0;
        texture_cache[i].text_len = 0;
        texture_cache[i].text[0] = '\0';
        texture_cache[i].width = 0;
        texture_cache[i].height = 0;
        texture_cache[i].pinned = 0;
    }
    texture_cache_scale_milli = -1;
}

void backend_texture_cache_invalidate(void) {
    int i;

    for (i = 0; i < MAX_TEXTURE_CACHE_ENTRIES; i++) {
        if (!texture_cache[i].texture || texture_cache[i].pinned) {
            continue;
        }
        SDL_DestroyTexture(texture_cache[i].texture);
        texture_cache[i].texture = NULL;
        texture_cache[i].key_hash = 0;
        texture_cache[i].font = NULL;
        texture_cache[i].text_len = 0;
        texture_cache[i].text[0] = '\0';
        texture_cache[i].width = 0;
        texture_cache[i].height = 0;
    }
}

void backend_texture_cache_pin(DFont* font, const char* text, Color color) {
    int font_size;
    uint64_t key_hash;
    int index;

    if (!font || !text || !text[0]) {
        return;
    }

    backend_texture_cache_sync_scale();
    font_size = backend_get_font_size(font);
    key_hash = backend_texture_cache_make_hash(font, font_size, color, text);
    index = backend_texture_cache_find_index(key_hash, font, font_size, color, text);
    if (index >= 0) {
        texture_cache[index].pinned = 1;
    }
}

void backend_texture_cache_warmup(DFont* font, const char** texts, int count, Color color) {
    int i;

    if (!font || !texts || count <= 0) {
        return;
    }

    for (i = 0; i < count; i++) {
        if (!texts[i] || !texts[i][0]) {
            continue;
        }
        backend_render_texture(font, texts[i], color);
        backend_texture_cache_pin(font, texts[i], color);
    }
}

SDL_Texture* find_texture_in_cache(DFont* font, const char* text, Color color, int font_size,
                                     int* width, int* height) {
    uint64_t key_hash;
    int index;

    if (!font || !text) {
        return NULL;
    }

    backend_texture_cache_sync_scale();
    if (font_size <= 0) {
        font_size = backend_get_font_size(font);
    }

    key_hash = backend_texture_cache_make_hash(font, font_size, color, text);
    index = backend_texture_cache_find_index(key_hash, font, font_size, color, text);
    if (index < 0) {
        return NULL;
    }

    texture_cache[index].last_used = SDL_GetTicks();
    if (width) {
        *width = texture_cache[index].width;
    }
    if (height) {
        *height = texture_cache[index].height;
    }
    return texture_cache[index].texture;
}

void add_texture_to_cache(DFont* font, const char* text, Color color, int font_size,
                          SDL_Texture* texture, int width, int height, int pinned) {
    uint64_t key_hash;
    int cache_index;

    if (!font || !text || !texture) {
        return;
    }

    backend_texture_cache_sync_scale();
    if (font_size <= 0) {
        font_size = backend_get_font_size(font);
    }
    if (!pinned && backend_text_cache_volatile(text)) {
        return;
    }

    key_hash = backend_texture_cache_make_hash(font, font_size, color, text);
    cache_index = backend_texture_cache_find_index(key_hash, font, font_size, color, text);
    if (cache_index >= 0) {
        backend_texture_cache_store_entry(cache_index, key_hash, font, font_size, text, color,
                                        texture, width, height, pinned || texture_cache[cache_index].pinned);
        return;
    }

    cache_index = backend_texture_cache_pick_evict_index();
    if (cache_index < 0) {
        SDL_DestroyTexture(texture);
        return;
    }

    backend_texture_cache_store_entry(cache_index, key_hash, font, font_size, text, color,
                                      texture, width, height, pinned);
}

// ====================== 字体缓存管理 ======================
void init_font_cache() {
    if (font_cache_initialized) return;
    
    for (int i = 0; i < MAX_FONT_CACHE_ENTRIES; i++) {
        font_cache[i].font = NULL;
        font_cache[i].font_path[0] = '\0';
        font_cache[i].size = 0;
        font_cache[i].weight[0] = '\0';
        font_cache[i].last_used = 0;
    }
    
    font_cache_initialized = 1;
}

void cleanup_font_cache() {
    printf("Cleaning up font cache...\n");
    for (int i = 0; i < MAX_FONT_CACHE_ENTRIES; i++) {
        if (font_cache[i].font) {
            TTF_CloseFont(font_cache[i].font);
            font_cache[i].font = NULL;
        }
        font_cache[i].font_path[0] = '\0';
        font_cache[i].size = 0;
        font_cache[i].weight[0] = '\0';
    }
    printf("Font cache cleanup completed\n");
}

// 在缓存中查找字体
TTF_Font* find_font_in_cache(const char* font_path, int size, const char* weight) {
    for (int i = 0; i < MAX_FONT_CACHE_ENTRIES; i++) {
        if (font_cache[i].font &&
            strcmp(font_cache[i].font_path, font_path) == 0 &&
            font_cache[i].size == size &&
            strcmp(font_cache[i].weight, weight) == 0) {
            font_cache[i].last_used = SDL_GetTicks();
            return font_cache[i].font;
        }
    }
    return NULL;
}

// 添加字体到缓存
void add_font_to_cache(const char* font_path, int size, const char* weight, TTF_Font* font) {
    // 查找空闲位置或最久未使用的位置
    int cache_index = -1;
    Uint32 oldest_time = SDL_GetTicks();
    
    // 首先查找空闲位置
    for (int i = 0; i < MAX_FONT_CACHE_ENTRIES; i++) {
        if (!font_cache[i].font) {
            cache_index = i;
            break;
        }
    }
    
    // 如果没有空闲位置，查找最久未使用的位置
    if (cache_index == -1) {
        for (int i = 0; i < MAX_FONT_CACHE_ENTRIES; i++) {
            if (font_cache[i].last_used < oldest_time) {
                oldest_time = font_cache[i].last_used;
                cache_index = i;
            }
        }
    }
    
    if (cache_index >= 0) {
        // 如果该位置已有字体，先关闭它
        if (font_cache[cache_index].font) {
            TTF_CloseFont(font_cache[cache_index].font);
        }
        
        // 添加新字体到缓存
        strncpy(font_cache[cache_index].font_path, font_path, MAX_PATH - 1);
        font_cache[cache_index].font_path[MAX_PATH - 1] = '\0';
        font_cache[cache_index].size = size;
        strncpy(font_cache[cache_index].weight, weight, 31);
        font_cache[cache_index].weight[31] = '\0';
        font_cache[cache_index].font = font;
        font_cache[cache_index].last_used = SDL_GetTicks();
    }
}

// 毛玻璃效果缓存结构
typedef struct {
    SDL_Texture* texture;
    int x, y, w, h;
    int blur_radius;
    float saturation, brightness;
    Uint32 last_used;
    int in_use;
} BlurCacheEntry;

#define MAX_BLUR_CACHE_ENTRIES 5
BlurCacheEntry blur_cache[MAX_BLUR_CACHE_ENTRIES] = {0};
int blur_cache_initialized = 0;

// 触屏事件状态
typedef struct {
    int fingerCount;          // 当前触摸点数量
    int lastX[MAX_TOUCHES];   // 上次触摸点X坐标
    int lastY[MAX_TOUCHES];   // 上次触摸点Y坐标
    Uint32 startTime;         // 触摸开始时间戳（用于长按检测）
    Uint32 lastTapTime;       // 上次点击时间戳（用于双击检测）
    int tapCount;             // 点击次数
    int longPressDetected;    // 是否已检测到长按
} TouchState;

#define MAX_TOUCHES 10
TouchState touchState = {0};

#define SWIPE_THRESHOLD_PX 32
static int pointer_drag_active = 0;
static int pointer_start_x = 0;
static int pointer_start_y = 0;
static int pointer_last_x = 0;
static int pointer_last_y = 0;
static int touch_swipe_start_x = 0;
static int touch_swipe_start_y = 0;
static Uint32 last_swipe_emit_ms = 0;

static void backend_emit_swipe_event(Layer* root, int x, int y, int dx, int dy) {
    int adx = dx < 0 ? -dx : dx;
    int ady = dy < 0 ? -dy : dy;
    Uint32 now;
    if (adx < SWIPE_THRESHOLD_PX) return;
    if (adx < ady) return;

    /* 鼠标/触控板可能对同一次拖动各发一次，短窗口内去重 */
    now = SDL_GetTicks();
    if (last_swipe_emit_ms != 0 && (now - last_swipe_emit_ms) < 80) {
        return;
    }
    last_swipe_emit_ms = now;

    PointerEvent pe;
    memset(&pe, 0, sizeof(pe));
    pe.device = POINTER_DEVICE_TOUCH;
    pe.phase = POINTER_SWIPE;
    pe.x = x;
    pe.y = y;
    pe.delta_x = dx;
    pe.delta_y = dy;
    pe.pointer_id = 0;
    pe.finger_count = 1;
    handle_pointer_event(root, &pe);
}

float getDisplayScale(SDL_Window* window) {
    int renderW, renderH;
    int windowW, windowH;

    SDL_GetWindowSize(window, &windowW, &windowH);
    if (windowW <= 0 || windowH <= 0) {
        return 1.0f;
    }
    SDL_GL_GetDrawableSize(window, &renderW, &renderH);
    return (float)renderW / (float)windowW;
}

float backend_get_density(void) {
    return yui_density > 0.0f ? yui_density : 1.0f;
}

void backend_set_density(float density) {
    if (density <= 0.0f) {
        return;
    }
    yui_density = density;
    if (renderer) {
        SDL_RenderSetScale(renderer, yui_density, yui_density);
    }
}

static void backend_apply_display_scale(void) {
    if (!window) {
        return;
    }
    backend_set_density(getDisplayScale(window));
}

#ifdef __EMSCRIPTEN__
/* Emscripten MOUSE 坐标在 backing-store 空间，需换算到逻辑坐标；FINGER 已是逻辑坐标 */
static void backend_pointer_to_logical(int *x, int *y) {
    if (yui_density > 1.0f) {
        *x = (int)((float)(*x) / yui_density);
        *y = (int)((float)(*y) / yui_density);
    }
}

static void backend_finger_to_logical(const SDL_TouchFingerEvent *finger, int *x, int *y) {
    int win_w = 0, win_h = 0;
    SDL_GetWindowSize(window, &win_w, &win_h);
    if (win_w <= 0 || win_h <= 0) {
        *x = 0;
        *y = 0;
        return;
    }
    *x = (int)(finger->x * win_w);
    *y = (int)(finger->y * win_h);
}
#endif

static void backend_deliver_pointer_event(Layer* root, PointerEvent* pe) {
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

static void backend_deliver_mouse_pointer(Layer* root, int x, int y, PointerPhase phase,
                                          Uint8 button, int dx, int dy) {
    PointerEvent pe;
    memset(&pe, 0, sizeof(pe));
    pe.device = POINTER_DEVICE_MOUSE;
    pe.phase = phase;
    pe.x = x;
    pe.y = y;
    pe.button = button;
    pe.delta_x = dx;
    pe.delta_y = dy;
    backend_deliver_pointer_event(root, &pe);
}

static void backend_dispatch_touch(Layer* root, PointerPhase phase, int x, int y,
                                   int dx, int dy, int pointer_id, int finger_count,
                                   float scale, float rotation) {
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

void draw_rounded_rect_with_border(SDL_Renderer* renderer, int x, int y, int w, int h, int radius, int border_width, SDL_Color bg_color, SDL_Color border_color);

void draw_rounded_rect(SDL_Renderer* renderer, int x, int y, int w, int h, int radius, SDL_Color color);

static void yui_aa_circle_cache_free(void);

int backend_init(){
    yui_component_registry_init();
    yui_components_register_builtin();

    // Windows: 抑制系统 DLL 调试输出（DWM 等），并禁止崩溃弹窗
    #ifdef YUI_WIN32_NATIVE
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    #endif

    // 鼠标 / 触摸分轨：内容拖滚动由各自路径处理，勿双向合成以免双滚
    SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");

    // 初始化SDL
    SDL_Init(SDL_INIT_VIDEO);

    // 启用 IME UI 显示（候选词窗口）
    SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");

    // 设置渲染质量为最佳（抗锯齿）
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "best");

    // Windows：默认"点击激活窗口"会吞掉首次点击，导致控件第一次点不中（拿不到焦点）
    // 开启 click-through 让首次点击也能送达应用侧
    SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");

    // Windows: 强制使用 OpenGL 渲染器以避免 Direct3D 的颜色问题
    #ifdef YUI_WIN32_NATIVE
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengl");
    printf("Windows detected: Forcing OpenGL renderer to avoid Direct3D color issues\n");
    #endif

#ifdef __EMSCRIPTEN__
    // Emscripten: 禁用帧率控制，由主循环管理
    SDL_SetHint(SDL_HINT_EMSCRIPTEN_ASYNCIFY, "0");
    SDL_SetHint(SDL_HINT_RENDER_VSYNC, "0");
#endif

    // Emscripten 不需要 OPENGL；开启 HIGH DPI 以高清渲染
    // Headless / auto-test: create hidden window (still renderable for screenshot)
    Uint32 window_flags = SDL_WINDOW_ALLOW_HIGHDPI;
    if (backend_is_headless()) {
        window_flags |= SDL_WINDOW_HIDDEN;
        printf("Backend: headless mode (window hidden)\n");
    } else {
        window_flags |= SDL_WINDOW_SHOWN;
    }
#ifndef __EMSCRIPTEN__
    window_flags |= SDL_WINDOW_OPENGL;
#endif
    
    window = SDL_CreateWindow("YUI",
                                        SDL_WINDOWPOS_CENTERED,
                                        SDL_WINDOWPOS_CENTERED,
                                        800, 600, window_flags);

    if (window && backend_is_headless()) {
        SDL_HideWindow(window);
    }

#ifdef __EMSCRIPTEN__
    // Emscripten 环境下不使用 PRESENTVSYNC，因为主循环控制帧率
    printf("Emscripten: Creating renderer without PRESENTVSYNC\n");
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
#else
    // 桌面环境下尝试使用垂直同步
    printf("Desktop: Creating renderer with PRESENTVSYNC\n");
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
#endif

    if (!renderer) {
        // 如果创建失败，重试不带任何标志
        printf("Warning: Failed to create renderer: %s\n", SDL_GetError());
        printf("Retrying with software renderer...\n");

        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }

    // 如果成功创建渲染器，禁用交换间隔（Emscripten 使用 rAF 主循环，勿调用 GL API）
    if (renderer) {
#ifndef __EMSCRIPTEN__
        SDL_GL_SetSwapInterval(0);
#endif
    }

    if (!renderer) {
        printf("Error: Failed to create any renderer: %s\n", SDL_GetError());
        return -1;
    }

    // 检查渲染器信息
    SDL_RendererInfo renderer_info;
    if (SDL_GetRendererInfo(renderer, &renderer_info) == 0) {
        printf("Renderer created successfully!\n");
        printf("  Renderer name: %s\n", renderer_info.name);
        printf("  Renderer flags: ");

        if (renderer_info.flags & SDL_RENDERER_ACCELERATED) {
            printf("ACCELERATED ");
        }
        if (renderer_info.flags & SDL_RENDERER_PRESENTVSYNC) {
            printf("PRESENTVSYNC ");
        }
        if (renderer_info.flags & SDL_RENDERER_SOFTWARE) {
            printf("SOFTWARE ");
        }
        if (renderer_info.flags & SDL_RENDERER_TARGETTEXTURE) {
            printf("TARGETTEXTURE ");
        }
        printf("\n");

        // 检查是否支持垂直同步
        if (renderer_info.flags & SDL_RENDERER_PRESENTVSYNC) {
            printf("  Vertical Sync: Enabled\n");
        } else {
            printf("  Vertical Sync: Disabled (not supported)\n");
        }
        
        // 打印支持的纹理格式
        printf("  Supported texture formats (%d): ", renderer_info.num_texture_formats);
        for (int i = 0; i < renderer_info.num_texture_formats; i++) {
            Uint32 format = renderer_info.texture_formats[i];
            printf("%s ", SDL_GetPixelFormatName(format));
        }
        printf("\n");
    }
    
    // 启用透明度混合
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    backend_apply_display_scale();
    
    // 初始化SDL_image库，支持多种图片格式
#ifndef __EMSCRIPTEN__
    {
        int imgFlags = IMG_INIT_PNG | IMG_INIT_JPG | IMG_INIT_TIF | IMG_INIT_WEBP;
        int imgReady = IMG_Init(imgFlags);
        int imgRequired = IMG_INIT_PNG | IMG_INIT_JPG;
        if ((imgReady & imgRequired) != imgRequired) {
            printf("SDL_image initialization failed: %s\n", IMG_GetError());
            return -1;
        }
    }
    {
        const char* imgVersion = IMG_Linked_Version() ? SDL_GetRevision() : "Unknown";
        printf("SDL_image version: %s\n", imgVersion);
    }
#else
    // Emscripten: IMG_Init 在无格式库时会误报失败，IMG_Load 仍可工作（见 emscripten-ports/SDL2_image#3）
    printf("SDL_image: skipped IMG_Init on Emscripten\n");
#endif
    
    // 初始化TTF
    if (TTF_Init() == -1) {
        printf("TTF initialization failed: %s\n", TTF_GetError());
        return -1;
    }
    
    // 初始化字体缓存
    init_font_cache();
    // 初始化触摸状态
    memset(&touchState, 0, sizeof(TouchState));

    return 0;

}


#ifdef YUI_WIN32_NATIVE

// WCA_USEDARKMODECOLORS 可能未在旧 SDK 中定义
#ifndef WCA_USEDARKMODECOLORS
#define WCA_USEDARKMODECOLORS 26
#endif

// WINDOWCOMPOSITIONATTRIBDATA 结构体定义（如果 SDK 已有则跳过）
#ifndef _WINDOWCOMPOSITIONATTRIBDATA_DEFINED_
#define _WINDOWCOMPOSITIONATTRIBDATA_DEFINED_
typedef struct _WINDOWCOMPOSITIONATTRIBDATA {
    DWORD dwAttrib;
    PVOID pvData;
    DWORD cbData;
} WINDOWCOMPOSITIONATTRIBDATA, *PWINDOWCOMPOSITIONATTRIBDATA;
#endif

// 保存标题栏颜色用于重新应用
static Color g_titlebar_bg = {0};
static Color g_titlebar_text = {0};
static int g_titlebar_colors_set = 0;  // 是否已设置颜色

// 应用暗色模式的核心逻辑
static void apply_titlebar_dark_mode(HWND hwnd) {
    BOOL dark = TRUE;

    // AllowDarkModeForWindow（uxtheme.dll ordinal 133）
    static BOOL (WINAPI *_AllowDarkModeForWindow)(HWND, BOOL) = NULL;
    if (!_AllowDarkModeForWindow) {
        HMODULE hUx = LoadLibraryA("uxtheme.dll");
        if (hUx) {
            *(FARPROC*)&_AllowDarkModeForWindow = GetProcAddress(hUx, (LPCSTR)(ULONG_PTR)(WORD)133);
        }
    }
    if (_AllowDarkModeForWindow) {
        _AllowDarkModeForWindow(hwnd, TRUE);
    }

    // SetWindowCompositionAttribute（Win10 18362+）
    static BOOL (WINAPI *_SetWindowCompositionAttribute)(HWND, WINDOWCOMPOSITIONATTRIBDATA*) = NULL;
    if (!_SetWindowCompositionAttribute) {
        HMODULE hmod = LoadLibraryA("user32.dll");
        if (hmod) {
            *(FARPROC*)&_SetWindowCompositionAttribute = GetProcAddress(hmod, "SetWindowCompositionAttribute");
        }
    }
    if (_SetWindowCompositionAttribute) {
        WINDOWCOMPOSITIONATTRIBDATA data = { WCA_USEDARKMODECOLORS, &dark, sizeof(dark) };
        _SetWindowCompositionAttribute(hwnd, &data);
    }

    // DwmSetWindowAttribute（Win10 20H1+ / Win11）
    DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark));

    // 自定义颜色（Win11 完整支持，Win10 静默忽略）
    if (g_titlebar_colors_set) {
        COLORREF bgColor = RGB(g_titlebar_bg.r, g_titlebar_bg.g, g_titlebar_bg.b);
        DwmSetWindowAttribute(hwnd, 35, &bgColor, sizeof(bgColor));
        DwmSetWindowAttribute(hwnd, 34, &bgColor, sizeof(bgColor));
        COLORREF textColor = RGB(g_titlebar_text.r, g_titlebar_text.g, g_titlebar_text.b);
        DwmSetWindowAttribute(hwnd, 36, &textColor, sizeof(textColor));
    }

    // SetProp（Win10 < 18362 回退）
    SetPropW(hwnd, L"UseImmersiveDarkModeColors", (HANDLE)(INT_PTR)dark);

    // 刷新 DWM 框架
    SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE);

    // 扩展 DWM 框架覆盖整个窗口（让按钮区域也使用暗色绘制）
    static BOOL (WINAPI *_DwmExtendFrameIntoClientArea)(HWND, const MARGINS*) = NULL;
    if (!_DwmExtendFrameIntoClientArea) {
        HMODULE hDwm = LoadLibraryA("dwmapi.dll");
        if (hDwm) {
            *(FARPROC*)&_DwmExtendFrameIntoClientArea = GetProcAddress(hDwm, "DwmExtendFrameIntoClientArea");
        }
    }
    if (_DwmExtendFrameIntoClientArea) {
        MARGINS margins = { -1, -1, -1, -1 };
        _DwmExtendFrameIntoClientArea(hwnd, &margins);
    }
}

// 窗口子类化过程 — 拦截 DWM 重置标题栏颜色的消息
static WNDPROC g_original_wndproc = NULL;

static LRESULT CALLBACK TitleBarSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_NCACTIVATE:
    case WM_SETTEXT:
        {
            LRESULT result = CallWindowProc(g_original_wndproc, hwnd, msg, wParam, lParam);
            apply_titlebar_dark_mode(hwnd);
            return result;
        }
    }
    return CallWindowProc(g_original_wndproc, hwnd, msg, wParam, lParam);
}

// 设置窗口标题栏暗色模式 + 自定义颜色（兼容 Win10 和 Win11）
// 使用完整的兼容链: SetWindowCompositionAttribute → SetProp → DwmSetWindowAttribute
void backend_set_titlebar_color(Color bg, Color text) {
    if (!window) return;

    // 保存颜色值（用于子类化过程重新应用）
    g_titlebar_bg = bg;
    g_titlebar_text = text;
    g_titlebar_colors_set = 1;

    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);

    if (SDL_GetWindowWMInfo(window, &wmInfo)) {
        HWND hwnd = wmInfo.info.win.window;

        // 首次调用时安装窗口子类化
        if (!g_original_wndproc) {
            g_original_wndproc = (WNDPROC)SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)TitleBarSubclassProc);
        }

        apply_titlebar_dark_mode(hwnd);
    }
}

#else /* !YUI_WIN32_NATIVE */

void backend_set_titlebar_color(Color bg, Color text) {
    (void)bg;
    (void)text;
}

#endif /* YUI_WIN32_NATIVE */

// 设置窗口图标
void backend_set_window_icon(const char* path) {
    if (!window || !path) return;
    SDL_Surface* icon = IMG_Load(path);
    if (icon) {
        SDL_SetWindowIcon(window, icon);
        SDL_FreeSurface(icon);
    } else {
        printf("Failed to load icon: %s\n", path);
    }
}



// 重置触摸状态
void resetTouchState() {
    touchState.fingerCount = 0;
    memset(touchState.lastX, 0, sizeof(touchState.lastX));
    memset(touchState.lastY, 0, sizeof(touchState.lastY));
    touchState.longPressDetected = 0;
}

// 检查点是否在图层内
int pointInLayer(SDL_Point* point, Layer* layer) {
    return SDL_PointInRect(point, &layer->rect);
}

void handle_event(Layer* root, SDL_Event* event) {
    // 处理键盘事件
    if (event->type == SDL_KEYDOWN) {
        // 创建键盘按键事件
        KeyEvent key_event;
        key_event.type = KEY_EVENT_DOWN;
        key_event.data.key.key_code = event->key.keysym.sym;
        key_event.data.key.mod = event->key.keysym.mod;
        key_event.data.key.repeat = event->key.repeat;
        
        handle_key_event(root, &key_event);
    } else if (event->type == SDL_TEXTEDITING) {
        // 处理 IME 文本编辑事件（显示候选词）
        KeyEvent key_event;
        key_event.type = KEY_EVENT_TEXT_EDITING;
        strncpy(key_event.data.text.text, event->edit.text, 31);
        key_event.data.text.text[31] = '\0';
        key_event.data.text.start = event->edit.start;
        key_event.data.text.length = event->edit.length;
        
        handle_key_event(root, &key_event);
    } else if (event->type == SDL_TEXTINPUT) {
        // 创建文本输入事件
        KeyEvent key_event;
        key_event.type = KEY_EVENT_TEXT_INPUT;
        strncpy(key_event.data.text.text, event->text.text, 31);
        key_event.data.text.text[31] = '\0';
        
        handle_key_event(root, &key_event);
    }
    
    // 处理鼠标事件
    if (event->type == SDL_MOUSEBUTTONDOWN || event->type == SDL_MOUSEBUTTONUP || event->type == SDL_MOUSEMOTION) {
        int mouse_x, mouse_y;
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
#ifdef __EMSCRIPTEN__
        backend_pointer_to_logical(&mouse_x, &mouse_y);
#endif

        {
            int delta_x = 0;
            int delta_y = 0;
            PointerPhase phase = POINTER_MOVE;
            if (event->type == SDL_MOUSEBUTTONDOWN) {
                phase = (clicks >= 2) ? POINTER_DOUBLE_TAP : POINTER_DOWN;
                if (button == SDL_BUTTON_LEFT && phase == POINTER_DOWN) {
                    pointer_drag_active = 1;
                    pointer_start_x = mouse_x;
                    pointer_start_y = mouse_y;
                    pointer_last_x = mouse_x;
                    pointer_last_y = mouse_y;
                }
            } else if (event->type == SDL_MOUSEBUTTONUP) {
                phase = POINTER_UP;
                if (button == SDL_BUTTON_LEFT && pointer_drag_active) {
                    backend_emit_swipe_event(root, mouse_x, mouse_y,
                        mouse_x - pointer_start_x, mouse_y - pointer_start_y);
                    pointer_drag_active = 0;
                }
            } else {
                if (pointer_drag_active) {
                    delta_x = mouse_x - pointer_last_x;
                    delta_y = mouse_y - pointer_last_y;
                    pointer_last_x = mouse_x;
                    pointer_last_y = mouse_y;
                }
            }

            backend_deliver_mouse_pointer(root, mouse_x, mouse_y, phase, button,
                                          delta_x, delta_y);
        }
    }
    // 滚轮并入 mouse 路径
    else if (event->type == SDL_MOUSEWHEEL) {
        int mouse_x = 0;
        int mouse_y = 0;
        PointerEvent pe;
        SDL_GetMouseState(&mouse_x, &mouse_y);
#ifdef __EMSCRIPTEN__
        backend_pointer_to_logical(&mouse_x, &mouse_y);
#endif
        memset(&pe, 0, sizeof(pe));
        pe.device = POINTER_DEVICE_MOUSE;
        pe.phase = POINTER_WHEEL;
        pe.x = mouse_x;
        pe.y = mouse_y;
        pe.button = SDL_BUTTON_LEFT;
        pe.delta_x = event->wheel.x;
        pe.delta_y = -event->wheel.y;
        handle_pointer_event(root, &pe);
    }
    else if (event->type == SDL_WINDOWEVENT && event->window.event == SDL_WINDOWEVENT_RESIZED) {
        backend_handle_window_resize(root);
    }
    // 触摸开始事件
    else if (event->type == SDL_FINGERDOWN) {
        int x, y;
        SDL_GetWindowSize(window, &x, &y);
        x = (int)(event->tfinger.x * x);
        y = (int)(event->tfinger.y * y);

        touchState.fingerCount++;
        if (touchState.fingerCount == 1) {
            touchState.startTime = SDL_GetTicks();
            touchState.longPressDetected = 0;
            touch_swipe_start_x = x;
            touch_swipe_start_y = y;
        }
        
        // 保存触摸位置
        int touchId = (int)(event->tfinger.fingerId % MAX_TOUCHES);
        touchState.lastX[touchId] = x;
        touchState.lastY[touchId] = y;
        
        backend_dispatch_touch(root, POINTER_DOWN, x, y, 0, 0,
                               touchId, touchState.fingerCount, 0.0f, 0.0f);
        
        // 双击检测（仅单指）
        if (touchState.fingerCount == 1) {
            Uint32 currentTime = SDL_GetTicks();
            if (currentTime - touchState.lastTapTime < 300) {
                touchState.tapCount++;
                if (touchState.tapCount == 2) {
                    backend_dispatch_touch(root, POINTER_DOUBLE_TAP, x, y, 0, 0,
                                           touchId, 1, 0.0f, 0.0f);
                    touchState.tapCount = 0;
                }
            } else {
                touchState.tapCount = 1;
            }
            touchState.lastTapTime = currentTime;
        }
    }
    // 触摸移动事件
    else if (event->type == SDL_FINGERMOTION) {
        // 转换触摸坐标为窗口坐标
        int x, y;
        SDL_GetWindowSize(window, &x, &y);
        x = (int)(event->tfinger.x * x);
        y = (int)(event->tfinger.y * y);
        
        // 计算位移
        int touchId = event->tfinger.fingerId % MAX_TOUCHES;
        int deltaX = x - touchState.lastX[touchId];
        int deltaY = y - touchState.lastY[touchId];
        
        // 保存当前位置
        touchState.lastX[touchId] = x;
        touchState.lastY[touchId] = y;
        
        backend_dispatch_touch(root, POINTER_MOVE, x, y, deltaX, deltaY,
                               touchId, touchState.fingerCount, 0.0f, 0.0f);
        
        // 长按检测（500ms，仅单指）
        if (touchState.fingerCount == 1 && !touchState.longPressDetected) {
            if (SDL_GetTicks() - touchState.startTime > 500) {
                backend_dispatch_touch(root, POINTER_LONG_PRESS, x, y, deltaX, deltaY,
                                       touchId, 1, 0.0f, 0.0f);
                touchState.longPressDetected = 1;
            }
        }
    }
    // 触摸结束事件
    else if (event->type == SDL_FINGERUP) {
        // 更新触摸状态
        if (touchState.fingerCount > 0) {
            touchState.fingerCount--;
        }
        
        // 转换触摸坐标为窗口坐标
        int x, y;
        SDL_GetWindowSize(window, &x, &y);
        x = (int)(event->tfinger.x * x);
        y = (int)(event->tfinger.y * y);
        
        {
            int touchId = (int)(event->tfinger.fingerId % MAX_TOUCHES);
            backend_dispatch_touch(root, POINTER_UP, x, y, 0, 0,
                                   touchId, touchState.fingerCount, 0.0f, 0.0f);
        }

        if (touchState.fingerCount == 0) {
            backend_emit_swipe_event(root, x, y,
                x - touch_swipe_start_x, y - touch_swipe_start_y);
        }

        // 如果所有手指都离开屏幕，重置状态
        if (touchState.fingerCount == 0) {
            resetTouchState();
        }
    }
    else if (event->type == SDL_MULTIGESTURE) {
        int winW, winH;
        SDL_GetWindowSize(window, &winW, &winH);
        int cx = (int)(event->mgesture.x * winW);
        int cy = (int)(event->mgesture.y * winH);
        int fingers = (int)event->mgesture.numFingers;

        if (event->mgesture.dDist != 0.0f) {
            backend_dispatch_touch(root, POINTER_PINCH, cx, cy, 0, 0, -1, fingers,
                                   1.0f + event->mgesture.dDist, 0.0f);
        }
        if (event->mgesture.dTheta != 0.0f) {
            backend_dispatch_touch(root, POINTER_ROTATE, cx, cy, 0, 0, -1, fingers,
                                   1.0f, event->mgesture.dTheta);
        }
    }
}


void backend_render_text_copy(Texture * texture,
                   const Rect * srcrect,
                   const Rect * dstrect){

   SDL_RenderCopy(renderer, texture, srcrect, dstrect);                 
}

void backend_render_text_destroy(Texture * texture){
    // 检查纹理是否在缓存中，如果在缓存中则不销毁
    if (!texture) return;
    
    for (int i = 0; i < MAX_TEXTURE_CACHE_ENTRIES; i++) {
        if (texture_cache[i].texture == texture) {
            // 纹理在缓存中，不销毁
            return;
        }
    }
    
    // 不在缓存中，正常销毁
    SDL_DestroyTexture(texture);
}

// 毛玻璃效果缓存管理函数声明
void init_blur_cache();
void cleanup_blur_cache();
int find_matching_cache_entry(Rect* rect, int blur_radius, float saturation, float brightness);
int find_available_cache_entry();

void backend_quit(){
      // 清理毛玻璃缓存
      cleanup_blur_cache();
      yui_aa_circle_cache_free();
      cleanup_font_cache();
      cleanup_font_cache();
      
      // 清理纹理缓存
      cleanup_texture_cache();
      
      // 清理资源
#ifndef __EMSCRIPTEN__
    IMG_Quit();
#endif
    if (default_font) {
        TTF_CloseFont(default_font);
    }
    TTF_Quit();
    
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

// 打印所有图层信息的辅助函数
void print_layer_info(Layer* layer, int depth) {
    if (yui_log_get_level() < YUI_LOG_DEBUG) return;
    char indent[64];
    int indent_len = depth * 2;
    if (indent_len > (int)sizeof(indent) - 1) indent_len = (int)sizeof(indent) - 1;
    memset(indent, ' ', indent_len);
    indent[indent_len] = '\0';
    LOGD("layer", "%sLayer '%s': type=%d, scrollable=%d, scrollbar_v=%p, scrollbar_h=%p",
         indent, layer->id, layer->type, layer->scrollable,
         (void*)layer->scrollbar_v, (void*)layer->scrollbar_h);
    
    for (int i = 0; i < layer->child_count; i++) {
        if (layer->children[i]) {
            print_layer_info(layer->children[i], depth + 1);
        }
    }
}

void backend_run(Layer* ui_root){
    if (yui_log_get_level() >= YUI_LOG_DEBUG) {
        LOGD("layer", "=== Layer Information ===");
        print_layer_info(ui_root, 0);
        LOGD("layer", "========================");
    }

    g_ui_root = ui_root;

#ifdef __EMSCRIPTEN__
    // Emscripten 环境下使用 emscripten 主循环
    g_running = 1;
    printf("Emscripten: Starting main loop...\n");

    // 先注册主循环，再设置时序（顺序不可颠倒）
    emscripten_set_main_loop(backend_main_loop, 0, 1);
#else
    // 桌面环境使用普通循环
    // 主循环（onLoad 可能已在 backend_run 前调用 YUI.exit）
    SDL_Event event;
    int running = !g_request_quit;
    while (running) {
        if (g_request_quit) {
            running = 0;
            break;
        }

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = 0;
            handle_event(ui_root, &event);

#ifdef YUI_WIN32_NATIVE
            // 窗口移动/大小改变后重新应用标题栏暗色
            if (event.type == SDL_WINDOWEVENT) {
                switch (event.window.event) {
                case SDL_WINDOWEVENT_MOVED:
                case SDL_WINDOWEVENT_RESIZED:
                case SDL_WINDOWEVENT_EXPOSED:
                    {
                        SDL_SysWMinfo wmInfo;
                        SDL_VERSION(&wmInfo.version);
                        if (SDL_GetWindowWMInfo(window, &wmInfo)) {
                            apply_titlebar_dark_mode(wmInfo.info.win.window);
                        }
                    }
                    break;
                }
            }
#endif
        }

        // 调用所有注册的更新回调
        for (int i = 0; i < update_callback_count; i++) {
            if (update_callbacks[i]) {
                update_callbacks[i]();
            }
        }

        SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
        SDL_RenderClear(renderer);

        perf_frame_begin();
        perf_render_tree_begin();
        render_layer(ui_root);
        perf_render_tree_end();
        render_inspect_overlay(ui_root);
        perf_draw_overlay(ui_root);

        // 渲染弹出层
        popup_manager_render();
        perf_frame_end();

        SDL_RenderPresent(renderer);

#ifdef YUI_WIN32_NATIVE
        // 第一帧渲染后重新应用暗色（此时窗口已完全显示）
        static int first_frame = 1;
        if (first_frame) {
            first_frame = 0;
            SDL_SysWMinfo wmInfo;
            SDL_VERSION(&wmInfo.version);
            if (SDL_GetWindowWMInfo(window, &wmInfo)) {
                apply_titlebar_dark_mode(wmInfo.info.win.window);
            }
        }
#endif

        if (g_auto_frames >= 0) {
            if (g_auto_frames == 0) {
                running = 0;
            } else {
                g_auto_frames--;
            }
        }

        SDL_Delay(16);
    }
#endif

}

void backend_tick(Layer* ui_root) {
    SDL_Event event;

    if (!ui_root || !renderer) {
        return;
    }

    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            return;
        }
        handle_event(ui_root, &event);
    }

    for (int i = 0; i < update_callback_count; i++) {
        if (update_callbacks[i]) {
            update_callbacks[i]();
        }
    }

    SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
    SDL_RenderClear(renderer);

    perf_frame_begin();
    perf_render_tree_begin();
    render_layer(ui_root);
    perf_render_tree_end();
    render_inspect_overlay(ui_root);
    perf_draw_overlay(ui_root);
    popup_manager_render();
    perf_frame_end();

    SDL_RenderPresent(renderer);
}

DFont* backend_load_font(char* font_path,int size){
    return backend_load_font_with_weight(font_path, size, "normal");
}

#ifdef __EMSCRIPTEN__
static void emscripten_font_variant_path(const char* font_path, const char* variant,
                                         char* out_path, size_t out_size)
{
    const char* regular_suffix = "-Regular";
    const char* pos;

    if (!font_path || !out_path || out_size == 0) {
        return;
    }

    out_path[0] = '\0';
    if (strstr(font_path, variant)) {
        strncpy(out_path, font_path, out_size - 1);
        out_path[out_size - 1] = '\0';
        return;
    }

    pos = strstr(font_path, regular_suffix);
    if (pos) {
        snprintf(out_path, out_size, "%.*s-%s%s",
                 (int)(pos - font_path), font_path, variant, pos + strlen(regular_suffix));
        return;
    }

    pos = strrchr(font_path, '.');
    if (pos) {
        snprintf(out_path, out_size, "%.*s-%s%s",
                 (int)(pos - font_path), font_path, variant, pos);
        return;
    }

    strncpy(out_path, font_path, out_size - 1);
    out_path[out_size - 1] = '\0';
}

static TTF_Font* emscripten_open_font_file(const char* path, int size)
{
    FILE* f;
    long file_size;
    unsigned char* font_data;
    SDL_RWops* rw;
    TTF_Font* font;

    if (!path || !path[0]) {
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

    font = TTF_OpenFontRW(rw, 1, size * yui_density);
    return font;
}
#endif

DFont* backend_load_font_with_weight(char* font_path,int size,const char* weight){
    // 初始化字体缓存
    if (!font_cache_initialized) {
        init_font_cache();
    }

    // 先在缓存中查找字体
    TTF_Font* cached_font = find_font_in_cache(font_path, size, weight);
    if (cached_font) {
        return cached_font;
    }

    char full_path[MAX_PATH];
    TTF_Font* default_font = NULL;

#ifdef __EMSCRIPTEN__
    const char* try_paths[6];
    int try_count = 0;

    if (TTF_WasInit() == 0 && TTF_Init() == -1) {
        LOGW("font", "SDL_ttf init failed: %s", TTF_GetError());
        return NULL;
    }

    if (strcmp(weight, "bold") == 0) {
        emscripten_font_variant_path(font_path, "Bold", full_path, sizeof(full_path));
        try_paths[try_count++] = full_path;
        try_paths[try_count++] = "app/assets/Roboto-Bold.ttf";
        try_paths[try_count++] = font_path;
        try_paths[try_count++] = "app/assets/Roboto-Regular.ttf";
    } else if (strcmp(weight, "light") == 0) {
        emscripten_font_variant_path(font_path, "Light", full_path, sizeof(full_path));
        try_paths[try_count++] = full_path;
        try_paths[try_count++] = "app/assets/Roboto-Light.ttf";
        try_paths[try_count++] = font_path;
        try_paths[try_count++] = "app/assets/Roboto-Regular.ttf";
    } else {
        try_paths[try_count++] = font_path;
        try_paths[try_count++] = "app/assets/Roboto-Regular.ttf";
    }

    for (int i = 0; i < try_count && !default_font; i++) {
        default_font = emscripten_open_font_file(try_paths[i], size);
        /* Emscripten: 不用 TTF_SetFontStyle 模拟粗体，会触发 zero width；仅使用真实 Bold 文件 */
    }

    if (!default_font) {
        LOGW("font", "emscripten font load failed: %s (weight: %s) %s",
             font_path, weight, TTF_GetError());
    }
#else
    // 非 Emscripten 环境，使用普通的 TTF_OpenFont
    // 根据字体粗细选择字体文件
    if (strcmp(weight, "bold") == 0) {
        // 尝试加载粗体字体
        snprintf(full_path, sizeof(full_path), "%s", font_path);
        // 如果路径中没有包含Bold，尝试添加Bold后缀
        if (strstr(font_path, "Bold") == NULL && strstr(font_path, "bold") == NULL) {
            char* ext = strrchr(font_path, '.');
            if (ext) {
                int base_len = ext - font_path;
                snprintf(full_path, sizeof(full_path), "%.*s-Bold%s", base_len, font_path, ext);
            }
        }
        default_font = TTF_OpenFont(full_path, size*yui_density);

        // 如果粗体字体不存在，尝试其他粗体字体
        if (!default_font) {
            default_font = TTF_OpenFont("assets/Roboto-Bold.ttf", size*yui_density);
        }
        if (!default_font) {
            default_font = TTF_OpenFont("app/assets/Roboto-Bold.ttf", size*yui_density);
        }
        if (!default_font) {
            // 如果找不到粗体字体文件，使用normal字体并设置样式
            default_font = TTF_OpenFont(font_path, size*yui_density);
            if (default_font) {
                TTF_SetFontStyle(default_font, TTF_STYLE_BOLD);
            }
        }
    } else if (strcmp(weight, "light") == 0) {
        // 尝试加载细体字体
        snprintf(full_path, sizeof(full_path), "%s", font_path);
        if (strstr(font_path, "Light") == NULL && strstr(font_path, "light") == NULL) {
            char* ext = strrchr(font_path, '.');
            if (ext) {
                int base_len = ext - font_path;
                snprintf(full_path, sizeof(full_path), "%.*s-Light%s", base_len, font_path, ext);
            }
        }
        default_font = TTF_OpenFont(full_path, size*yui_density);

        if (!default_font) {
            default_font = TTF_OpenFont("assets/Roboto-Light.ttf", size*yui_density);
        }
        if (!default_font) {
            default_font = TTF_OpenFont("app/assets/Roboto-Light.ttf", size*yui_density);
        }
        if (!default_font) {
            default_font = TTF_OpenFont(font_path, size*yui_density);
            if (default_font) {
                TTF_SetFontStyle(default_font, TTF_STYLE_NORMAL);
            }
        }
    } else {
        // normal或其他情况，使用普通字体
        default_font = TTF_OpenFont(font_path,size*yui_density);
    }
#endif

    // 如果指定的字体加载失败，尝试备用字体 (仅在非 Emscripten 环境下)
    if (!default_font) {
#ifndef __EMSCRIPTEN__
        printf("Warning: Could not load font '%s', trying fallback fonts\n", font_path);
        // 非 Emscripten 环境下的备用字体
        default_font = TTF_OpenFont("arial.ttf",size*yui_density);
        if (!default_font) {
            default_font = TTF_OpenFont("Arial.ttf",size*yui_density);
        }
        if (!default_font) {
            default_font = TTF_OpenFont("assets/Roboto-Regular.ttf",size*yui_density);
        }
        if (!default_font) {
            default_font = TTF_OpenFont("app/assets/Roboto-Regular.ttf",size*yui_density);
        }
#else
        // Emscripten 环境的备用字体已经在上面的 SDL_RWops 代码中处理了
        printf("Warning: Could not load font '%s' (weight: %s) in Emscripten environment\n", font_path, weight);
#endif
    }
    
    if (default_font) {
        TTF_SetFontHinting(default_font, TTF_HINTING_LIGHT); 
        TTF_SetFontOutline(default_font, 0); // 无轮廓
        
        add_font_to_cache(font_path, size, weight, default_font);
        LOGD("font", "opened: %s (size: %d, weight: %s) -> %p",
             font_path, size, weight, (void*)default_font);
    }

    return default_font;
}

void backend_get_windowsize(int* width,int * height){
    SDL_GetWindowSize(window, width, height);
}

void backend_set_windowsize(int width,int  height){
    SDL_SetWindowSize(window, width, height);
    backend_apply_display_scale();
}

void backend_set_window_size(char* title){
    SDL_SetWindowTitle(window,title);
}

void backend_set_resizable(int resizable) {
    if (window) {
        SDL_SetWindowResizable(window, resizable ? SDL_TRUE : SDL_FALSE);
    }
}

void backend_set_minimum_windowsize(int width, int height) {
    if (window && width > 0 && height > 0) {
        SDL_SetWindowMinimumSize(window, width, height);
    }
}



// 检查文件是否为SVG格式
bool is_svg_file(const char* path) {
    const char* ext = strrchr(path, '.');
    if (ext && strlen(ext) > 4) {
        // 检查文件扩展名是否为.svg或.SVG
        return (strcmp(ext, ".svg") == 0 || strcmp(ext, ".SVG") == 0);
    }
    return false;
}

Texture* backend_load_texture(char* path){
    SDL_Surface* surface = NULL;
    SDL_Texture* texture=NULL;
    
    // 检查是否为SVG文件
    if (is_svg_file(path)) {
        // SDL_image 2.6.0及以上版本原生支持SVG
        surface = IMG_Load(path);
        
        if (!surface) {
            printf("Failed to load SVG image %s: %s\n", path, IMG_GetError());
            printf("Note: SVG support requires SDL_image 2.6.0 or newer.\n");
            
            // 创建一个占位符表面，用于显示SVG加载失败的提示
            surface = SDL_CreateRGBSurface(0, 100, 100, 32, 0, 0, 0, 0);
            if (surface) {
                // 填充为灰色背景
                SDL_FillRect(surface, NULL, SDL_MapRGB(surface->format, 200, 200, 200));
            }
        }
    } else {
        // 非SVG文件，使用常规加载方式
        surface = IMG_Load(path);
    }
    
    if (surface) {
        texture = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_FreeSurface(surface);
    } else {
        printf("Failed to load image %s: %s\n", path, IMG_GetError());
    }
    return texture;
}

int backend_query_texture(Texture * texture,
                     Uint32 * format, int *access,
                     int *w, int *h){

   return SDL_QueryTexture(texture,format,access,w,h);                      
}

int backend_measure_text_width(DFont* font, const char* text) {
    DFont* fallback = NULL;
    DFont* render_font;
    int total_w = 0;
    int w = 0;
    int h = 0;

    if (!font || !text || !text[0]) {
        return 0;
    }

    render_font = backend_resolve_render_font(font, text, &fallback);
    if (render_font) {
        if (TTF_SizeUTF8(render_font, text, &w, &h) == 0 && w > 0) {
            return (int)(((float)w / yui_density) + 0.5f);
        }
        return 0;
    }

    if (!fallback || fallback == font) {
        return 0;
    }

    {
        const char* cursor = text;
        const char* run_start = NULL;
        const char* run_end = NULL;
        DFont* run_font = NULL;
        char run_buf[256];

        while (1) {
            if (!*cursor) {
                if (run_start && run_font && run_end > run_start) {
                    int len = (int)(run_end - run_start);
                    if (len > 0 && len < (int)sizeof(run_buf)) {
                        memcpy(run_buf, run_start, (size_t)len);
                        run_buf[len] = '\0';
                        if (TTF_SizeUTF8(run_font, run_buf, &w, &h) == 0) {
                            total_w += w;
                        }
                    }
                }
                break;
            }

            {
                const char* next = cursor;
                Uint32 codepoint = 0;
                DFont* chosen;

                if (!utf8_decode_codepoint(&next, &codepoint)) {
                    break;
                }
                chosen = backend_pick_font_for_codepoint(font, fallback, codepoint);
                if (!chosen) {
                    cursor = next;
                    continue;
                }

                if (run_font == chosen) {
                    run_end = next;
                } else {
                    if (run_start && run_font && run_end > run_start) {
                        int len = (int)(run_end - run_start);
                        if (len > 0 && len < (int)sizeof(run_buf)) {
                            memcpy(run_buf, run_start, (size_t)len);
                            run_buf[len] = '\0';
                            if (TTF_SizeUTF8(run_font, run_buf, &w, &h) == 0) {
                                total_w += w;
                            }
                        }
                    }
                    run_start = cursor;
                    run_end = next;
                    run_font = chosen;
                }
                cursor = next;
            }
        }
    }

    if (total_w > 0) {
        return (int)(((float)total_w / yui_density) + 0.5f);
    }
    return 0;
}

Texture* backend_render_texture(DFont* font,const char* text,Color color){
    if (!font) {
        printf("error: backend_render_texture called with NULL font (text: '%s')\n", text ? text : "(null)");
        return NULL;
    }
    if(strlen(text)==0){
        return NULL;
    }
    
    // 检查字体指针是否被破坏
    if ((uintptr_t)font == 0xbebebebebebebebeULL) {
        printf("error: corrupted font pointer in backend_render_texture (text: '%s')\n", text ? text : "(null)");
        return NULL;
    }
    
    // 初始化纹理缓存
    if (!texture_cache_initialized) {
        init_texture_cache();
    }
    
    DFont* fallback = NULL;
    DFont* render_font = backend_resolve_render_font(font, text, &fallback);

    // 尝试从缓存中查找（统一按主字体 key，混合纹理也缓存于此）
    int font_size_px = backend_get_font_size(font);
    int cached_width, cached_height;
    SDL_Texture* cached_texture = find_texture_in_cache(font, text, color, font_size_px,
                                                        &cached_width, &cached_height);
    if (cached_texture) {
        return cached_texture;
    }

#ifdef __EMSCRIPTEN__
    {
        SDL_Surface* em_surface = backend_render_emscripten_text_surface(
            font, text,
            (SDL_Color){color.r, color.g, color.b, color.a},
            font_size_px);
        SDL_Texture* em_texture;

        if (em_surface) {
            em_texture = SDL_CreateTextureFromSurface(renderer, em_surface);
            SDL_FreeSurface(em_surface);
            if (em_texture) {
                SDL_SetTextureBlendMode(em_texture, SDL_BLENDMODE_BLEND);
                SDL_SetTextureScaleMode(em_texture, SDL_ScaleModeBest);
                int width = 0;
                int height = 0;
                SDL_QueryTexture(em_texture, NULL, NULL, &width, &height);
                add_texture_to_cache(font, text, color, font_size_px, em_texture, width, height, 0);
                return em_texture;
            }
        }
    }
#endif

    // 混合排版：主字体与 fallback 各自覆盖部分字形，分段渲染后横向拼接
    if (render_font == NULL && fallback && fallback != font) {
        const char* cursor = text;
        const char* run_starts[128];
        const char* run_ends[128];
        DFont* run_fonts[128];
        SDL_Surface* surfaces[128];
        int run_count = 0;
        int total_w = 0;
        int max_h = 0;
        int i;
        int x = 0;
        char run_buf[256];
        SDL_Surface* final_surface = NULL;
        SDL_Texture* mixed_texture = NULL;

        for (i = 0; i < 128; i++) {
            surfaces[i] = NULL;
        }

        while (*cursor && run_count < 128) {
            const char* run_start = cursor;
            Uint32 codepoint = 0;
            DFont* chosen;

            if (!utf8_decode_codepoint(&cursor, &codepoint)) {
                break;
            }
            chosen = backend_pick_font_for_codepoint(font, fallback, codepoint);
            if (!chosen) {
                continue;
            }

            if (run_count > 0 && run_fonts[run_count - 1] == chosen) {
                run_ends[run_count - 1] = cursor;
            } else {
                run_starts[run_count] = run_start;
                run_ends[run_count] = cursor;
                run_fonts[run_count] = chosen;
                run_count++;
            }
        }

        for (i = 0; i < run_count; i++) {
            int len = (int)(run_ends[i] - run_starts[i]);
            if (len <= 0 || len >= (int)sizeof(run_buf)) {
                continue;
            }
            memcpy(run_buf, run_starts[i], (size_t)len);
            run_buf[len] = '\0';
            surfaces[i] = backend_render_blended_utf8(run_fonts[i], run_buf, color);
            if (!surfaces[i]) {
                continue;
            }
            total_w += surfaces[i]->w;
            if (surfaces[i]->h > max_h) {
                max_h = surfaces[i]->h;
            }
        }

        if (total_w > 0 && max_h > 0) {
            final_surface = SDL_CreateRGBSurfaceWithFormat(0, total_w, max_h, 32, SDL_PIXELFORMAT_RGBA32);
            if (final_surface) {
                SDL_FillRect(final_surface, NULL, SDL_MapRGBA(final_surface->format, 0, 0, 0, 0));
                for (i = 0; i < run_count; i++) {
                    SDL_Surface* surf = surfaces[i];
                    SDL_Rect dst;
                    if (!surf) {
                        continue;
                    }
                    dst.x = x;
                    dst.y = (max_h - surf->h) / 2;
                    dst.w = surf->w;
                    dst.h = surf->h;
                    SDL_BlitSurface(surf, NULL, final_surface, &dst);
                    x += surf->w;
                    SDL_FreeSurface(surf);
                    surfaces[i] = NULL;
                }
                mixed_texture = SDL_CreateTextureFromSurface(renderer, final_surface);
                SDL_FreeSurface(final_surface);
                if (mixed_texture) {
                    SDL_SetTextureScaleMode(mixed_texture, SDL_ScaleModeBest);
                    int mw = 0, mh = 0;
                    SDL_QueryTexture(mixed_texture, NULL, NULL, &mw, &mh);
                    add_texture_to_cache(font, text, color, font_size_px, mixed_texture, mw, mh, 0);
                    return mixed_texture;
                }
            }
        }

        for (i = 0; i < run_count; i++) {
            if (surfaces[i]) {
                SDL_FreeSurface(surfaces[i]);
            }
        }
        // 合成失败则退回主字体单独渲染
        render_font = font;
    }

    SDL_Surface* surface = backend_render_blended_utf8(render_font, text, color);
    if (!surface && fallback && fallback != render_font) {
        surface = backend_render_blended_utf8(fallback, text, color);
    }
    if (!surface) {
        LOGD("font", "skip unrenderable text: '%s'", text);
        return NULL;
    }
    
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_SetTextureScaleMode(texture, SDL_ScaleModeBest);

    SDL_FreeSurface(surface);

    // 将纹理添加到缓存
    if (texture) {
        int width, height;
        SDL_QueryTexture(texture, NULL, NULL, &width, &height);
        add_texture_to_cache(font, text, color, font_size_px, texture, width, height, 0);
    }

    return texture;
}

void backend_render_clear_color(unsigned char r,unsigned char g,unsigned char b,unsigned char a){
    SDL_SetRenderDrawColor(renderer, r, g, b, a);
    SDL_RenderClear(renderer);
}

void backend_render_present(){
    SDL_RenderPresent(renderer);
}

void backend_delay(int delay){
    SDL_Delay(delay);
}

Uint32 backend_get_ticks(void){
    return SDL_GetTicks();
}

void backend_get_pointer_state(int* x, int* y){
    SDL_GetMouseState(x, y);
}

void backend_render_fill_rect(Rect* rect,Color color){
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 
                                color.r, 
                                color.g, 
                                color.b, 
                                color.a);
    SDL_RenderFillRect(renderer, rect);
}

void backend_render_fill_rect_color(Rect* rect,unsigned char r,unsigned char g,unsigned char b,unsigned char a){
    SDL_SetRenderDrawColor(renderer, r, g, b, a);
    SDL_RenderFillRect(renderer, rect);
}

// 绘制带圆角的填充矩形
void backend_render_rounded_rect(Rect* rect, Color color, int radius) {
    draw_rounded_rect(renderer, rect->x, rect->y, rect->w, rect->h, radius, color);
}

// 绘制带圆角的填充矩形（直接指定颜色值）
void backend_render_rounded_rect_color(Rect* rect, unsigned char r, unsigned char g, unsigned char b, unsigned char a, int radius) {
    SDL_Color color = {r, g, b, a};
    draw_rounded_rect(renderer, rect->x, rect->y, rect->w, rect->h, radius, color);
}

// 绘制带圆角和边框的填充矩形
void backend_render_rounded_rect_with_border(Rect* rect, Color bg_color, int radius, int border_width, Color border_color) {
    // 转换Color到SDL_Color
    SDL_Color sdl_bg_color = {bg_color.r, bg_color.g, bg_color.b, bg_color.a};
    SDL_Color sdl_border_color = {border_color.r, border_color.g, border_color.b, border_color.a};
    draw_rounded_rect_with_border(renderer, rect->x, rect->y, rect->w, rect->h, radius, border_width, sdl_bg_color, sdl_border_color);
}

void backend_render_rect(Rect* rect,Color color){
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 
                                color.r, 
                                color.g, 
                                color.b, 
                                color.a);
    SDL_RenderDrawRect(renderer, rect);
}

void backend_render_rect_color(Rect* rect,unsigned char r,unsigned char g,unsigned char b,unsigned char a){
     // 绘制按钮边框
    SDL_SetRenderDrawColor(renderer, r, g, b, a);
    SDL_RenderDrawRect(renderer,rect);
}


void backend_render_get_clip_rect(Rect* prev_clip){
    SDL_RenderGetClipRect(renderer, prev_clip);
}

void backend_render_set_clip_rect(Rect* clip){
    SDL_RenderSetClipRect(renderer, clip);
}


// 创建带透明度的颜色
SDL_Color create_color(Uint8 r, Uint8 g, Uint8 b, Uint8 a) {
    SDL_Color color = {r, g, b, a};
    return color;
}

// 绘制带透明度的矩形
void draw_rect(SDL_Renderer* renderer, int x, int y, int w, int h, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_Rect rect = {x, y, w, h};
    SDL_RenderFillRect(renderer, &rect);
}

// 绘制带透明度的圆形
void draw_circle(SDL_Renderer* renderer, int center_x, int center_y, int radius, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    
    for (int x = -radius; x <= radius; x++) {
        for (int y = -radius; y <= radius; y++) {
            if (x*x + y*y <= radius*radius) {
                SDL_RenderDrawPoint(renderer, center_x + x, center_y + y);
            }
        }
    }
}

// // 绘制带透明度的多边形
// void draw_polygon(SDL_Renderer* renderer, SDL_Point points[], int count, SDL_Color color) {
//     SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
//     
//     // 绘制多边形轮廓
//     for (int i = 0; i < count; i++) {
//         int next = (i + 1) % count;
//         SDL_RenderDrawLine(renderer, points[i].x, points[i].y, points[next].x, points[next].y);
//     }
//     
//     // 填充多边形 (简单实现，实际应用中可能需要更复杂的算法)
//     int min_y = points[0].y, max_y = points[0].y;
//     for (int i = 1; i < count; i++) {
//         if (points[i].y < min_y) min_y = points[i].y;
//         if (points[i].y > max_y) max_y = points[i].y;
//     }
//     
//     for (int y = min_y; y <= max_y; y++) {
//         for (int x = 0; x < WINDOW_WIDTH; x++) {
//             int inside = 0;
//             for (int i = 0, j = count-1; i < count; j = i++) {
//                 if (((points[i].y > y) != (points[j].y > y)) &&
//                     (x < (points[j].x - points[i].x) * (y - points[i].y) / (points[j].y - points[i].y) + points[i].x)) {
//                     inside = !inside;
//                 }
//             }
//             if (inside) {
//                 SDL_RenderDrawPoint(renderer, x, y);
//             }
//         }
//     }
// }

/* LVGL-style 4x supersampled quarter-circle AA (see lv_draw_mask.c circ_calc_aa4) */
typedef struct {
    int radius;
    uint8_t *buf;
    uint8_t *cir_opa;
    uint16_t *opa_start_on_y;
    uint16_t *x_start_on_y;
} YuiRadiusAA;

#define YUI_AA_CIRCLE_CACHE_SIZE 16
static YuiRadiusAA yui_aa_circle_cache[YUI_AA_CIRCLE_CACHE_SIZE];

static void yui_aa_circ_init(int *cx, int *cy, int *tmp, int radius)
{
    *cx = radius;
    *cy = 0;
    *tmp = 1 - radius;
}

static bool yui_aa_circ_cont(int cx, int cy)
{
    return cy <= cx;
}

static void yui_aa_circ_next(int *cx, int *cy, int *tmp)
{
    if (*tmp <= 0) {
        *tmp += 2 * (*cy) + 3;
    } else {
        *tmp += 2 * ((*cy) - (*cx)) + 5;
        (*cx)--;
    }
    (*cy)++;
}

static void yui_aa_circle_build(YuiRadiusAA *c, int radius)
{
    int *cir_x;
    int *cir_y;
    int cir_size = 0;
    int cp_x;
    int cp_y;
    int tmp;
    int i;

    if (c->buf) {
        free(c->buf);
        c->buf = NULL;
    }
    if (radius <= 0) {
        c->radius = 0;
        return;
    }

    c->radius = radius;
    c->buf = (uint8_t *)malloc((size_t)radius * 6 + 6);
    if (!c->buf) {
        c->radius = 0;
        return;
    }
    c->cir_opa = c->buf;
    c->opa_start_on_y = (uint16_t *)(c->buf + 2 * radius + 2);
    c->x_start_on_y = (uint16_t *)(c->buf + 4 * radius + 4);

    if (radius == 1) {
        c->cir_opa[0] = 180;
        c->opa_start_on_y[0] = 0;
        c->opa_start_on_y[1] = 1;
        c->x_start_on_y[0] = 0;
        return;
    }

    cir_x = (int *)malloc((size_t)(radius + 1) * 4 * sizeof(int));
    if (!cir_x) {
        free(c->buf);
        c->buf = NULL;
        c->radius = 0;
        return;
    }
    cir_y = cir_x + (radius + 1) * 2;

    {
        const int cir_opa_max = radius * 2 + 2;
        int y_8th_cnt = 0;
        int x_int[4];
        int x_fract[4];

        yui_aa_circ_init(&cp_x, &cp_y, &tmp, radius * 4);
        x_int[0] = cp_x >> 2;
        x_fract[0] = 0;

        while (yui_aa_circ_cont(cp_x, cp_y)) {
            for (i = 0; i < 4; i++) {
                yui_aa_circ_next(&cp_x, &cp_y, &tmp);
                if (!yui_aa_circ_cont(cp_x, cp_y)) {
                    break;
                }
                x_int[i] = cp_x >> 2;
                x_fract[i] = cp_x & 0x3;
            }
            if (i != 4) {
                break;
            }

#define YUI_AA_PUSH_CIR(px, py, opa) do { \
                if (cir_size >= cir_opa_max) { goto yui_aa_build_done; } \
                cir_x[cir_size] = (px); \
                cir_y[cir_size] = (py); \
                c->cir_opa[cir_size] = (uint8_t)(opa); \
                cir_size++; \
            } while (0)

            if (x_int[0] == x_int[3]) {
                YUI_AA_PUSH_CIR(x_int[0], y_8th_cnt, (x_fract[0] + x_fract[1] + x_fract[2] + x_fract[3]) * 16);
            } else if (x_int[0] != x_int[1]) {
                YUI_AA_PUSH_CIR(x_int[0], y_8th_cnt, x_fract[0] * 16);
                YUI_AA_PUSH_CIR(x_int[0] - 1, y_8th_cnt, (4 + x_fract[1] + x_fract[2] + x_fract[3]) * 16);
            } else if (x_int[0] != x_int[2]) {
                YUI_AA_PUSH_CIR(x_int[0], y_8th_cnt, (x_fract[0] + x_fract[1]) * 16);
                YUI_AA_PUSH_CIR(x_int[0] - 1, y_8th_cnt, (8 + x_fract[2] + x_fract[3]) * 16);
            } else {
                YUI_AA_PUSH_CIR(x_int[0], y_8th_cnt, (x_fract[0] + x_fract[1] + x_fract[2]) * 16);
                YUI_AA_PUSH_CIR(x_int[0] - 1, y_8th_cnt, (12 + x_fract[3]) * 16);
            }
            y_8th_cnt++;
        }

        {
            int mid = radius * 723;
            int mid_int = mid >> 10;
            if (cir_size == 0 || cir_x[cir_size - 1] != mid_int || cir_y[cir_size - 1] != mid_int) {
                int tmp_val = mid - (mid_int << 10);
                if (tmp_val <= 512) {
                    tmp_val = (tmp_val * tmp_val * 2) >> (10 + 6);
                } else {
                    tmp_val = 1024 - tmp_val;
                    tmp_val = (tmp_val * tmp_val * 2) >> (10 + 6);
                    tmp_val = 15 - tmp_val;
                }
                YUI_AA_PUSH_CIR(mid_int, mid_int, tmp_val * 16);
            }
        }

        for (i = cir_size - 2; i >= 0; i--, cir_size++) {
            if (cir_size >= cir_opa_max) {
                break;
            }
            cir_x[cir_size] = cir_y[i];
            cir_y[cir_size] = cir_x[i];
            c->cir_opa[cir_size] = c->cir_opa[i];
        }

yui_aa_build_done:
#undef YUI_AA_PUSH_CIR

        {
            int y = 0;
            i = 0;
            c->opa_start_on_y[0] = 0;
            while (i < cir_size && y <= radius) {
                c->opa_start_on_y[y] = (uint16_t)i;
                c->x_start_on_y[y] = (uint16_t)cir_x[i];
                for (; i < cir_size && cir_y[i] == y; i++) {
                    if (cir_x[i] < (int)c->x_start_on_y[y]) {
                        c->x_start_on_y[y] = (uint16_t)cir_x[i];
                    }
                }
                y++;
            }
            if (y <= radius) {
                c->opa_start_on_y[y] = (uint16_t)cir_size;
            }
        }
    }

    free(cir_x);
}

static YuiRadiusAA *yui_aa_circle_get(int radius)
{
    int i;
    int slot = -1;

    if (radius <= 0) {
        return NULL;
    }

    for (i = 0; i < YUI_AA_CIRCLE_CACHE_SIZE; i++) {
        if (yui_aa_circle_cache[i].radius == radius && yui_aa_circle_cache[i].buf) {
            return &yui_aa_circle_cache[i];
        }
        if (slot < 0 && yui_aa_circle_cache[i].radius == 0) {
            slot = i;
        }
    }
    if (slot < 0) {
        slot = 0;
    }

    yui_aa_circle_build(&yui_aa_circle_cache[slot], radius);
    return yui_aa_circle_cache[slot].buf ? &yui_aa_circle_cache[slot] : NULL;
}

static void yui_aa_circle_cache_free(void)
{
    int i;
    for (i = 0; i < YUI_AA_CIRCLE_CACHE_SIZE; i++) {
        free(yui_aa_circle_cache[i].buf);
        memset(&yui_aa_circle_cache[i], 0, sizeof(yui_aa_circle_cache[i]));
    }
}

static void yui_aa_circle_get_line(const YuiRadiusAA *c, int cir_y, int *aa_len, int *x_start, uint8_t **aa_opa)
{
    int r = c->radius;
    if (cir_y < 0) {
        cir_y = 0;
    }
    if (cir_y >= r) {
        cir_y = r - 1;
    }
    *aa_len = (int)(c->opa_start_on_y[cir_y + 1] - c->opa_start_on_y[cir_y]);
    *x_start = (int)c->x_start_on_y[cir_y];
    *aa_opa = &c->cir_opa[c->opa_start_on_y[cir_y]];
    if (*aa_len < 0) {
        *aa_len = 0;
    }
}

static void yui_draw_rounded_row_aa(SDL_Renderer *renderer, int x, int py, int w, int r, int cir_y,
                                    const YuiRadiusAA *aa, SDL_Color color)
{
    int aa_len;
    int x_start;
    uint8_t *aa_opa;
    int cir_x_left;
    int cir_x_right;
    int i;

    yui_aa_circle_get_line(aa, cir_y, &aa_len, &x_start, &aa_opa);
    cir_x_left = x + r - x_start - 1;
    cir_x_right = x + w - r + x_start;

    if (cir_x_right > cir_x_left + 1) {
        SDL_Rect solid = {cir_x_left + 1, py, cir_x_right - cir_x_left - 1, 1};
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
        SDL_RenderFillRect(renderer, &solid);
    }

    for (i = 0; i < aa_len; i++) {
        Uint8 a = (Uint8)((color.a * aa_opa[aa_len - 1 - i]) / 255);
        if (a == 0) {
            continue;
        }
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, a);
        SDL_RenderDrawPoint(renderer, cir_x_left - i, py);
        SDL_RenderDrawPoint(renderer, cir_x_right + i, py);
    }
}

void draw_rounded_rect(SDL_Renderer* renderer, int x, int y, int w, int h, int radius, SDL_Color color) {
    int r = radius;
    YuiRadiusAA *aa;

    if (r > w / 2) r = w / 2;
    if (r > h / 2) r = h / 2;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    if (r <= 0) {
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
        SDL_Rect rect = {x, y, w, h};
        SDL_RenderFillRect(renderer, &rect);
        return;
    }

    aa = yui_aa_circle_get(r);
    if (!aa) {
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
        SDL_Rect rect = {x, y, w, h};
        SDL_RenderFillRect(renderer, &rect);
        return;
    }

    for (int py = y; py < y + h; py++) {
        int local_y = py - y;
        int cir_y = -1;

        if (local_y < r) {
            cir_y = r - local_y - 1;
        } else if (local_y >= h - r) {
            cir_y = local_y - (h - r);
        }

        if (cir_y < 0) {
            SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
            SDL_Rect row = {x, py, w, 1};
            SDL_RenderFillRect(renderer, &row);
        } else {
            yui_draw_rounded_row_aa(renderer, x, py, w, r, cir_y, aa, color);
        }
    }
}

// 绘制带边框的圆角矩形
void draw_rounded_rect_with_border(SDL_Renderer* renderer, int x, int y, int w, int h, int radius, int border_width, SDL_Color bg_color, SDL_Color border_color) {
    // 如果边框宽度为0，只绘制填充
    if (border_width <= 0) {
        draw_rounded_rect(renderer, x, y, w, h, radius, bg_color);
        return;
    }
    
    // 限制圆角半径不超过宽高的一半
    int r = radius;
    if (r > w / 2) r = w / 2;
    if (r > h / 2) r = h / 2;
    
    // 先绘制边框（外层）
    draw_rounded_rect(renderer, x, y, w, h, r, border_color);
    
    // 然后绘制内部填充，这样背景色和圆角都是正确的
    int inner_x = x + border_width;
    int inner_y = y + border_width;
    int inner_w = w - 2 * border_width;
    int inner_h = h - 2 * border_width;
    int inner_r = r - border_width;
    if (inner_r < 0) inner_r = 0;
    
    if (inner_w > 0 && inner_h > 0) {
        draw_rounded_rect(renderer, inner_x, inner_y, inner_w, inner_h, inner_r, bg_color);
    }
}

// Add this function anywhere in backend_sdl.c after the global renderer declaration

// 绘制抗锯齿线段的辅助函数
static void plot_aa_pixel(int x, int y, float alpha, Color color) {
    if (alpha <= 0.0f) return;
    Uint8 a = (Uint8)(alpha * color.a);
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, a);
    SDL_RenderDrawPoint(renderer, x, y);
}

// 绘制抗锯齿线段
void backend_render_line(int x1, int y1, int x2, int y2, Color color) {
    // 对于水平或垂直线，使用SDL原生绘制（更快）
    if (x1 == x2 || y1 == y2) {
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
        SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
        return;
    }
    
    // 对于斜线，使用简化的抗锯齿算法
    // 使用Bresenham算法的改进版本
    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy;
    
    int x = x1, y = y1;
    
    while (1) {
        // 绘制主像素
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
        SDL_RenderDrawPoint(renderer, x, y);
        
        // 计算误差来决定是否绘制周围的像素进行抗锯齿
        int err2 = 2 * err;
        float alpha_factor = 0.3f; // 抗锯齿强度
        
        if (err2 > -dy) {
            // 绘制旁边像素进行抗锯齿
            if (y + sy >= 0) {
                plot_aa_pixel(x, y + sy, alpha_factor, color);
            }
            err -= dy;
            x += sx;
        }
        
        if (err2 < dx) {
            // 绘制旁边像素进行抗锯齿
            if (x + sx >= 0) {
                plot_aa_pixel(x + sx, y, alpha_factor, color);
            }
            err += dx;
            y += sy;
        }
        
        if (x == x2 && y == y2) break;
    }
}

// 绘制抗锯齿圆弧（逐像素渲染，无锯齿）
// center_x, center_y: 圆心坐标
// radius: 圆弧半径（到弧线中心的距离）
// start_angle, end_angle: 起止角度（度数，0度在顶部，顺时针）
// color: 颜色
// line_width: 弧线宽度
void backend_render_arc(int center_x, int center_y, int radius, float start_angle, float end_angle, Color color, int line_width) {
    if (radius <= 0 || line_width <= 0) return;
    
    float half_w = line_width / 2.0f;
    float r_inner = radius - half_w;
    float r_outer = radius + half_w;
    if (r_inner < 0) r_inner = 0;
    
    // 将角度转换为弧度（0度在顶部，顺时针）
    float start_rad = (start_angle - 90.0f) * M_PI / 180.0f;
    float end_rad = (end_angle - 90.0f) * M_PI / 180.0f;
    
    // 确保 end > start
    while (end_rad < start_rad) {
        end_rad += 2.0f * M_PI;
    }

    float sweep_deg = end_angle - start_angle;
    while (sweep_deg <= 0.0f) {
        sweep_deg += 360.0f;
    }
    int is_full_circle = (sweep_deg >= 359.0f);
    
    // 遍历圆弧的边界框
    int extent = (int)(r_outer + 2);
    int min_x = center_x - extent;
    int min_y = center_y - extent;
    int max_x = center_x + extent;
    int max_y = center_y + extent;
    
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    
    for (int py = min_y; py <= max_y; py++) {
        for (int px = min_x; px <= max_x; px++) {
            float dx = px - center_x + 0.5f;
            float dy = py - center_y + 0.5f;
            float dist = sqrtf(dx * dx + dy * dy);
            
            // 快速跳过：距离太远或太近
            if (dist < r_inner - 1.0f || dist > r_outer + 1.0f) continue;
            
            // 检查角度是否在弧线范围内
            float angle = atan2f(dy, dx);
            
            // 归一化角度到 [start_rad, start_rad + 2π) 范围
            while (angle < start_rad) angle += 2.0f * M_PI;
            while (angle > start_rad + 2.0f * M_PI) angle -= 2.0f * M_PI;
            
            // 角度边缘的抗锯齿（约1像素的平滑过渡）；整圆不淡化端点，避免顶部接缝缺口
            float angle_aa = 1.0f;
            if (!is_full_circle) {
                float pixel_angle = 1.0f / (dist > 0 ? dist : 1.0f); // 1像素对应的角度

                if (angle < start_rad + pixel_angle) {
                    angle_aa = (angle - start_rad) / pixel_angle;
                } else if (angle > end_rad - pixel_angle) {
                    angle_aa = (end_rad - angle) / pixel_angle;
                } else if (angle > end_rad) {
                    continue; // 完全超出角度范围
                }
                if (angle_aa <= 0.0f) continue;
                if (angle_aa > 1.0f) angle_aa = 1.0f;
            } else if (angle > end_rad) {
                continue;
            }
            
            // 径向抗锯齿：根据像素到弧线内外边缘的距离计算alpha
            float radial_aa = 1.0f;
            if (dist < r_inner) {
                radial_aa = 1.0f - (r_inner - dist); // 内边缘平滑
            } else if (dist > r_outer) {
                radial_aa = 1.0f - (dist - r_outer); // 外边缘平滑
            }
            if (radial_aa <= 0.0f) continue;
            if (radial_aa > 1.0f) radial_aa = 1.0f;
            
            // 组合alpha
            float final_alpha = angle_aa * radial_aa;
            Uint8 a = (Uint8)(final_alpha * color.a);
            if (a == 0) continue;
            
            SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, a);
            SDL_RenderDrawPoint(renderer, px, py);
        }
    }
}

// 毛玻璃效果缓存管理函数
void init_blur_cache() {
    if (blur_cache_initialized) return;
    
    for (int i = 0; i < MAX_BLUR_CACHE_ENTRIES; i++) {
        blur_cache[i].texture = NULL;
        blur_cache[i].in_use = 0;
        blur_cache[i].last_used = 0;
    }
    
    blur_cache_initialized = 1;
}

// 清理毛玻璃缓存
void cleanup_blur_cache() {
    printf("Cleaning up blur cache...\n");
    for (int i = 0; i < MAX_BLUR_CACHE_ENTRIES; i++) {
        if (blur_cache[i].texture) {
            printf("Destroying texture at cache index %d\n", i);
            SDL_DestroyTexture(blur_cache[i].texture);
            blur_cache[i].texture = NULL;
        }
        blur_cache[i].in_use = 0;
    }
    printf("Blur cache cleanup completed\n");
}

// 查找匹配的缓存条目
int find_matching_cache_entry(Rect* rect, int blur_radius, float saturation, float brightness) {
    //printf("Searching for matching cache entry...\n");
    for (int i = 0; i < MAX_BLUR_CACHE_ENTRIES; i++) {
        if (blur_cache[i].in_use &&
            blur_cache[i].x == rect->x &&
            blur_cache[i].y == rect->y &&
            blur_cache[i].w == rect->w &&
            blur_cache[i].h == rect->h &&
            blur_cache[i].blur_radius == blur_radius &&
            blur_cache[i].saturation == saturation &&
            blur_cache[i].brightness == brightness) {
            //printf("Found matching cache entry at index %d\n", i);
            return i;
        }
    }
    printf("No matching cache entry found\n");
    return -1;
}

// 查找可用的缓存条目
int find_available_cache_entry() {
    // 首先查找未使用的条目
    for (int i = 0; i < MAX_BLUR_CACHE_ENTRIES; i++) {
        if (!blur_cache[i].in_use) {
            return i;
        }
    }
    
    // 如果没有未使用的，查找最久未使用的条目
    Uint32 oldest_time = SDL_GetTicks();
    int oldest_index = 0;
    
    for (int i = 0; i < MAX_BLUR_CACHE_ENTRIES; i++) {
        if (blur_cache[i].last_used < oldest_time) {
            oldest_time = blur_cache[i].last_used;
            oldest_index = i;
        }
    }
    
    // 清理最旧的条目
    if (blur_cache[oldest_index].texture) {
        SDL_DestroyTexture(blur_cache[oldest_index].texture);
        blur_cache[oldest_index].texture = NULL;
    }
    
    return oldest_index;
}

// 实现优化的毛玻璃效果（带缓存）
void backend_render_backdrop_filter(Rect* rect, int blur_radius, float saturation, float brightness) {
    if (!renderer || !rect || blur_radius <= 0) {
        return;
    }
    
    // 初始化缓存（如果尚未初始化）
    if (!blur_cache_initialized) {
        init_blur_cache();
    }
    
    // 查找匹配的缓存条目
    int cache_index = find_matching_cache_entry(rect, blur_radius, saturation, brightness);
    
    // 如果找到匹配的缓存，直接使用
    if (cache_index >= 0) {
        SDL_RenderCopy(renderer, blur_cache[cache_index].texture, NULL, rect);
        blur_cache[cache_index].last_used = SDL_GetTicks();
        return;
    }
    
    // 获取当前渲染目标
    SDL_Texture* current_target = SDL_GetRenderTarget(renderer);
    
    // 限制模糊半径以提高性能
    if (blur_radius > 20) blur_radius = 20;
    
    // 创建一个临时纹理来捕获当前屏幕内容
    // 使用渲染器支持的像素格式（Windows Direct3D需要ARGB8888）
    Uint32 pixel_format = SDL_PIXELFORMAT_RGBA8888;
    SDL_RendererInfo renderer_info;
    if (SDL_GetRendererInfo(renderer, &renderer_info) == 0 && renderer_info.num_texture_formats > 0) {
        // 使用第一个支持的纹理格式（通常是ARGB8888或RGBA8888）
        pixel_format = renderer_info.texture_formats[0];
        printf("DEBUG: Using pixel format %s for blur texture\n", SDL_GetPixelFormatName(pixel_format));
    }
    
    SDL_Texture* temp_texture = SDL_CreateTexture(
        renderer, 
        pixel_format, 
        SDL_TEXTUREACCESS_TARGET, 
        rect->w, 
        rect->h
    );
    
    if (!temp_texture) {
        return;
    }
    
    // 设置临时纹理为渲染目标
    if (SDL_SetRenderTarget(renderer, temp_texture) != 0) {
        SDL_DestroyTexture(temp_texture);
        return;
    }
    
    // 将当前屏幕内容复制到临时纹理
    SDL_Rect src_rect = *rect;
    SDL_Rect dst_rect = {0, 0, rect->w, rect->h};
    
    // 将屏幕内容渲染到临时纹理
    SDL_RenderCopy(renderer, current_target, &src_rect, &dst_rect);
    
    // 恢复原始渲染目标
    SDL_SetRenderTarget(renderer, current_target);
    
    // 创建模糊纹理（只创建一次）
    SDL_Texture* blur_texture = SDL_CreateTexture(
        renderer, 
        pixel_format, 
        SDL_TEXTUREACCESS_TARGET, 
        rect->w, 
        rect->h
    );
    
    if (!blur_texture) {
        SDL_DestroyTexture(temp_texture);
        return;
    }
    
    // 设置模糊纹理为渲染目标
    SDL_SetRenderTarget(renderer, blur_texture);
    SDL_SetTextureBlendMode(blur_texture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureBlendMode(temp_texture, SDL_BLENDMODE_BLEND);
    
    // 优化的模糊算法 - 使用更少的采样点
    SDL_SetTextureAlphaMod(temp_texture, 200); // 增加不透明度以减少模糊次数
    
    // 使用优化的采样模式 - 减少采样点数量
    int sample_step = (blur_radius > 10) ? 3 : 2; // 根据模糊半径调整采样步长
    
    for (int x = -blur_radius; x <= blur_radius; x += sample_step) {
        for (int y = -blur_radius; y <= blur_radius; y += sample_step) {
            // 使用圆形采样区域
            if (x * x + y * y <= blur_radius * blur_radius) {
                SDL_Rect offset_rect = {x, y, rect->w, rect->h};
                SDL_RenderCopy(renderer, temp_texture, NULL, &offset_rect);
            }
        }
    }
    
    // 恢复原始渲染目标
    SDL_SetRenderTarget(renderer, current_target);
    
    // 应用饱和度和亮度调整（如果需要）
    if (saturation != 1.0f || brightness != 1.0f) {
        // 应用颜色调整
        SDL_SetTextureColorMod(blur_texture, 
            (Uint8)(255 * brightness), 
            (Uint8)(255 * brightness), 
            (Uint8)(255 * brightness)
        );
    }
    
    // 将最终结果渲染到屏幕
    SDL_RenderCopy(renderer, blur_texture, NULL, rect);
    
    // 查找可用的缓存条目并保存结果
    cache_index = find_available_cache_entry();
    if (cache_index >= 0) {
        // 如果已有纹理，先销毁
        if (blur_cache[cache_index].texture) {
            SDL_DestroyTexture(blur_cache[cache_index].texture);
        }
        
        // 创建一个新的纹理作为缓存副本
        blur_cache[cache_index].texture = SDL_CreateTexture(
            renderer, 
            pixel_format, 
            SDL_TEXTUREACCESS_TARGET, 
            rect->w, 
            rect->h
        );
        
        if (blur_cache[cache_index].texture) {
            // 将模糊纹理复制到缓存纹理
            SDL_SetRenderTarget(renderer, blur_cache[cache_index].texture);
            SDL_RenderCopy(renderer, blur_texture, NULL, NULL);
            SDL_SetRenderTarget(renderer, current_target);
            
            // 更新缓存条目信息
            blur_cache[cache_index].x = rect->x;
            blur_cache[cache_index].y = rect->y;
            blur_cache[cache_index].w = rect->w;
            blur_cache[cache_index].h = rect->h;
            blur_cache[cache_index].blur_radius = blur_radius;
            blur_cache[cache_index].saturation = saturation;
            blur_cache[cache_index].brightness = brightness;
            blur_cache[cache_index].last_used = SDL_GetTicks();
            blur_cache[cache_index].in_use = 1;
        }
    }
    
    // 清理纹理资源
    SDL_DestroyTexture(temp_texture);
    SDL_DestroyTexture(blur_texture);
}

// ====================== 主循环回调管理 ======================

// 注册主循环更新回调
void backend_set_resize_callback(ResizeCallback callback) {
    resize_callback = callback;
}

void backend_register_update_callback(UpdateCallback callback) {
    if (!callback) return;

    if (update_callback_count >= MAX_UPDATE_CALLBACKS) {
        printf("Warning: Update callback limit reached\n");
        return;
    }

    update_callbacks[update_callback_count++] = callback;
}

// 获取剪贴板文本
// Base64 解码表
static const unsigned char base64_decode_table[256] = {
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255, 62,255,255,255, 63,
     52, 53, 54, 55, 56, 57, 58, 59, 60, 61,255,255,255,255,255,255,
    255,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
     15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,255,255,255,255,255,
    255, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
     41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51,255,255,255,255,255
};

// Base64 解码函数
static size_t base64_decode(const char* input, unsigned char** output) {
    if (!input || !output) return 0;
    
    size_t input_len = strlen(input);
    if (input_len == 0) return 0;
    
    // 计算输出缓冲区大小
    size_t output_len = (input_len / 4) * 3;
    if (input_len > 0 && input[input_len - 1] == '=') output_len--;
    if (input_len > 1 && input[input_len - 2] == '=') output_len--;
    
    // 分配输出缓冲区
    *output = (unsigned char*)malloc(output_len);
    if (!*output) return 0;
    
    size_t i, j;
    for (i = 0, j = 0; i < input_len; i += 4) {
        // 获取4个字符的base64值
        unsigned char a = base64_decode_table[(unsigned char)input[i]];
        unsigned char b = base64_decode_table[(unsigned char)input[i + 1]];
        unsigned char c = base64_decode_table[(unsigned char)input[i + 2]];
        unsigned char d = base64_decode_table[(unsigned char)input[i + 3]];
        
        // 解码为3个字节
        (*output)[j++] = (a << 2) | (b >> 4);
        if (j < output_len) (*output)[j++] = (b << 4) | (c >> 2);
        if (j < output_len) (*output)[j++] = (c << 6) | d;
    }
    
    return output_len;
}

// 从 base64 数据加载纹理
Texture* backend_load_texture_from_base64(const char* base64_data, size_t data_len) {
    if (!base64_data || data_len == 0) {
        printf("Invalid base64 data\n");
        return NULL;
    }
    
    // 解码 base64 数据
    unsigned char* decoded_data = NULL;
    size_t decoded_len = base64_decode(base64_data, &decoded_data);
    
    if (decoded_len == 0 || !decoded_data) {
        printf("Failed to decode base64 data\n");
        return NULL;
    }
    
    // 使用 SDL_RWops 从内存加载图片
    SDL_RWops* rw = SDL_RWFromMem(decoded_data, decoded_len);
    if (!rw) {
        printf("Failed to create SDL_RWops: %s\n", SDL_GetError());
        free(decoded_data);
        return NULL;
    }
    
    // 加载图片
    SDL_Surface* surface = IMG_Load_RW(rw, 1);  // 1 表示自动释放 rw
    free(decoded_data);  // 解码后的数据已经被复制到 surface，可以释放
    
    if (!surface) {
        printf("Failed to load image from base64 data: %s\n", IMG_GetError());
        return NULL;
    }
    
    // 创建纹理
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    
    if (!texture) {
        printf("Failed to create texture from base64 data: %s\n", SDL_GetError());
        return NULL;
    }
    
    return (Texture*)texture;
}

char* backend_get_clipboard_text() {
#ifdef __EMSCRIPTEN__
    // Emscripten 环境下，使用 JavaScript 读取剪贴板
    static int clipboard_read_pending = 0;

    // 如果上次有待读取的剪贴板内容，直接返回（使用 malloc 分配）
    char* result = (char*)EM_ASM_INT({
        if (window._clipboard_result) {
            var len = lengthBytesUTF8(window._clipboard_result) + 1;
            var ptr = _malloc(len);
            stringToUTF8(window._clipboard_result, ptr, len);
            return ptr;
        }
        return 0;
    });

    if (result) {
        return result;
    }

    // 触发异步读取（下次调用时获取结果）
    if (!clipboard_read_pending) {
        clipboard_read_pending = 1;
        EM_ASM({
            // 检查是否支持 navigator.clipboard API
            if (navigator && navigator.clipboard && navigator.clipboard.readText) {
                navigator.clipboard.readText().then(function(text) {
                    window._clipboard_result = text;
                    console.log('Clipboard pre-read:', text);
                }).catch(function(err) {
                    console.error('Failed to pre-read clipboard:', err);
                    window._clipboard_result = '';
                });
            } else {
                // 不支持 clipboard API，使用回退方法
                console.log('Clipboard API not available');
                window._clipboard_result = '';
            }
        });
    }

    // 返回 malloc 分配的空字符串
    char* empty = (char*)malloc(1);
    if (empty) {
        empty[0] = '\0';
    }
    return empty;
#else
    // 桌面环境使用 SDL_GetClipboardText
    char* clipboard_text = SDL_GetClipboardText();
    if (!clipboard_text) {
        printf("Failed to get clipboard text: %s\n", SDL_GetError());
        return NULL;
    }

    // 分配内存并复制剪贴板内容
    size_t len = strlen(clipboard_text);
    char* result = (char*)malloc(len + 1);
    if (!result) {
        printf("Failed to allocate memory for clipboard text\n");
        SDL_free(clipboard_text);
        return NULL;
    }

    strcpy(result, clipboard_text);
    SDL_free(clipboard_text);

    return result;
#endif
}

void backend_set_clipboard_text(const char* text) {
    if (!text) return;

#ifdef __EMSCRIPTEN__
    // Emscripten 环境下，使用 JavaScript 设置剪贴板
    printf("Setting clipboard: '%s'\n", text);

    EM_ASM_({
        var text = UTF8ToString($0);

        // 检查是否支持 navigator.clipboard API
        if (navigator && navigator.clipboard && navigator.clipboard.writeText) {
            navigator.clipboard.writeText(text).then(function() {
                console.log('Clipboard set successfully');
            }).catch(function(err) {
                console.error('Failed to set clipboard:', err);
            });
        } else {
            // 回退方法：使用 document.execCommand
            var textarea = document.createElement('textarea');
            textarea.value = text;
            textarea.style.position = 'fixed';
            textarea.style.opacity = '0';
            textarea.style.top = '0';
            textarea.style.left = '0';
            document.body.appendChild(textarea);
            textarea.focus();
            textarea.select();
            try {
                document.execCommand('copy');
                console.log('Clipboard set via execCommand');
            } catch (e) {
                console.error('Failed to set clipboard via execCommand:', e);
            }
            document.body.removeChild(textarea);
        }

        // 同时保存到全局变量
        window._clipboard_result = text;
    }, text);
#else
    // 桌面环境使用 SDL_SetClipboardText
    if (SDL_SetClipboardText(text) < 0) {
        printf("Failed to set clipboard text: %s\n", SDL_GetError());
    }
#endif
}

// IME 文本输入支持
void backend_start_text_input() {
    SDL_StartTextInput();
}

void backend_stop_text_input() {
    SDL_StopTextInput();
}

void backend_set_text_input_rect(Rect* rect) {
    if (rect) {
        SDL_Rect sdl_rect;
        sdl_rect.x = rect->x;
        sdl_rect.y = rect->y;
        sdl_rect.w = rect->w;
        sdl_rect.h = rect->h;
        SDL_SetTextInputRect(&sdl_rect);
    }
}

void backend_set_ui_root(Layer* root) {
    g_ui_root = root;
}

int backend_screenshot(const char* path) {
    if (!renderer || !g_ui_root || !path || !path[0]) {
        return -1;
    }

    int w = 0, h = 0;
    SDL_GetWindowSize(window, &w, &h);
    if (w <= 0 || h <= 0) {
        return -2;
    }

    SDL_Texture* target = SDL_CreateTexture(
        renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, w, h);
    if (!target) {
        printf("screenshot: CreateTexture failed: %s\n", SDL_GetError());
        return -3;
    }

    SDL_Texture* prev_target = SDL_GetRenderTarget(renderer);
    SDL_SetRenderTarget(renderer, target);

    SDL_SetRenderDrawColor(renderer, 10, 13, 18, 255);
    SDL_RenderClear(renderer);

    render_layer(g_ui_root);
    render_inspect_overlay(g_ui_root);
    popup_manager_render();

    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(
        0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
    if (!surface) {
        SDL_SetRenderTarget(renderer, prev_target);
        SDL_DestroyTexture(target);
        return -4;
    }

    if (SDL_RenderReadPixels(renderer, NULL, SDL_PIXELFORMAT_ARGB8888,
                             surface->pixels, surface->pitch) != 0) {
        printf("screenshot: ReadPixels failed: %s\n", SDL_GetError());
        SDL_FreeSurface(surface);
        SDL_SetRenderTarget(renderer, prev_target);
        SDL_DestroyTexture(target);
        return -5;
    }

    SDL_SetRenderTarget(renderer, prev_target);
    SDL_DestroyTexture(target);

    SDL_Surface* rgba = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(surface);
    if (!rgba) {
        return -6;
    }

    int rc = screenshot_save_png(path, (const unsigned char*)rgba->pixels,
                                 rgba->w, rgba->h, rgba->pitch);
    SDL_FreeSurface(rgba);
    return rc;
}