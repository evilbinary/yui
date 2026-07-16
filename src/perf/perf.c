#include "perf.h"
#include "../layer.h"
#include "../render.h"
#include "../backend.h"
#include "../component_registry.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#if defined(YUI_BACKEND_SDL) || defined(__EMSCRIPTEN__)
#include <SDL.h>
#endif

#define PERF_MAX_SLOTS 512
#define PERF_MAX_WATCH 16
#define PERF_MAX_OVERLAY_LINES 14

typedef struct PerfSlot {
    Layer* layer;
    char id[50];
    int type;
    uint32_t frame_count;
    uint64_t frame_self_ns;
    uint64_t total_self_ns;
    uint64_t total_frames_seen;
    uint32_t max_frame_count;
} PerfSlot;

static int g_perf_enabled = 0;
static int g_perf_overlay = 0;
static int g_perf_top_n = 10;
static int g_perf_log_interval = 0;

static PerfSlot g_slots[PERF_MAX_SLOTS];
static int g_slot_count = 0;

static char g_watch_ids[PERF_MAX_WATCH][50];
static int g_watch_count = 0;

static PerfFrameStats g_frame;
static uint64_t g_frame_start_ns = 0;
static uint64_t g_render_tree_start_ns = 0;
static double g_fps_ema = 0.0;

uint64_t perf_now_ns(void)
{
#if defined(YUI_BACKEND_SDL) || defined(__EMSCRIPTEN__)
    static uint64_t freq = 0;
    if (!freq) {
        freq = (uint64_t)SDL_GetPerformanceFrequency();
        if (!freq) {
            freq = 1000000000ULL;
        }
    }
    return ((uint64_t)SDL_GetPerformanceCounter() * 1000000000ULL) / freq;
#else
    return (uint64_t)backend_get_ticks() * 1000000ULL;
#endif
}

static double perf_ns_to_ms(uint64_t ns)
{
    return (double)ns / 1000000.0;
}

int perf_is_enabled(void)
{
    return g_perf_enabled;
}

void perf_enable(int on)
{
    g_perf_enabled = on ? 1 : 0;
    if (!g_perf_enabled) {
        g_perf_overlay = 0;
    }
}

void perf_reset(void)
{
    g_slot_count = 0;
    memset(&g_frame, 0, sizeof(g_frame));
    g_fps_ema = 0.0;
}

void perf_set_overlay(int on)
{
    g_perf_overlay = on ? 1 : 0;
    if (g_perf_overlay) {
        g_perf_enabled = 1;
    }
}

int perf_overlay_enabled(void)
{
    return g_perf_overlay;
}

void perf_set_top_n(int n)
{
    if (n < 1) {
        n = 1;
    }
    if (n > PERF_MAX_OVERLAY_LINES) {
        n = PERF_MAX_OVERLAY_LINES;
    }
    g_perf_top_n = n;
}

void perf_set_log_interval(int frames)
{
    g_perf_log_interval = frames < 0 ? 0 : frames;
}

static int perf_is_watched_layer(Layer* layer)
{
    if (!layer || g_watch_count == 0) {
        return 0;
    }
    for (int i = 0; i < g_watch_count; i++) {
        if (strcmp(g_watch_ids[i], layer->id) == 0) {
            return 1;
        }
    }
    return 0;
}

int perf_watch(const char* layer_id)
{
    if (!layer_id || !layer_id[0]) {
        return 0;
    }
    for (int i = 0; i < g_watch_count; i++) {
        if (strcmp(g_watch_ids[i], layer_id) == 0) {
            return 1;
        }
    }
    if (g_watch_count >= PERF_MAX_WATCH) {
        return 0;
    }
    strncpy(g_watch_ids[g_watch_count], layer_id, sizeof(g_watch_ids[0]) - 1);
    g_watch_ids[g_watch_count][sizeof(g_watch_ids[0]) - 1] = '\0';
    g_watch_count++;
    g_perf_enabled = 1;
    return 1;
}

int perf_unwatch(const char* layer_id)
{
    if (!layer_id) {
        return 0;
    }
    for (int i = 0; i < g_watch_count; i++) {
        if (strcmp(g_watch_ids[i], layer_id) == 0) {
            for (int j = i + 1; j < g_watch_count; j++) {
                strcpy(g_watch_ids[j - 1], g_watch_ids[j]);
            }
            g_watch_count--;
            return 1;
        }
    }
    return 0;
}

void perf_clear_watch(void)
{
    g_watch_count = 0;
}

static PerfSlot* perf_get_slot(Layer* layer, int create)
{
    if (!layer) {
        return NULL;
    }

    for (int i = 0; i < g_slot_count; i++) {
        if (g_slots[i].layer == layer) {
            return &g_slots[i];
        }
    }

    if (!create || g_slot_count >= PERF_MAX_SLOTS) {
        return NULL;
    }

    PerfSlot* slot = &g_slots[g_slot_count++];
    memset(slot, 0, sizeof(*slot));
    slot->layer = layer;
    strncpy(slot->id, layer->id, sizeof(slot->id) - 1);
    slot->id[sizeof(slot->id) - 1] = '\0';
    slot->type = layer->type;
    return slot;
}

void perf_frame_begin(void)
{
    if (!g_perf_enabled) {
        return;
    }

    g_frame_start_ns = perf_now_ns();
    g_frame.layers_rendered = 0;

    for (int i = 0; i < g_slot_count; i++) {
        g_slots[i].frame_count = 0;
        g_slots[i].frame_self_ns = 0;
    }
}

void perf_frame_end(void)
{
    if (!g_perf_enabled) {
        return;
    }

    g_frame.frame_ns = perf_now_ns() - g_frame_start_ns;
    g_frame.frame_index++;

    if (g_frame.frame_ns > 0) {
        double instant_fps = 1000000000.0 / (double)g_frame.frame_ns;
        if (g_fps_ema <= 0.0) {
            g_fps_ema = instant_fps;
        } else {
            g_fps_ema = g_fps_ema * 0.9 + instant_fps * 0.1;
        }
        g_frame.fps = g_fps_ema;
    }

    for (int i = 0; i < g_slot_count; i++) {
        PerfSlot* slot = &g_slots[i];
        if (slot->frame_count > 0) {
            slot->total_self_ns += slot->frame_self_ns;
            slot->total_frames_seen++;
            if (slot->frame_count > slot->max_frame_count) {
                slot->max_frame_count = slot->frame_count;
            }
        }
    }

    if (g_perf_log_interval > 0 &&
        g_frame.frame_index % (uint64_t)g_perf_log_interval == 0) {
        PerfLayerStats top[8];
        int n = perf_get_layer_stats(top, 8, PERF_SORT_TIME);
        printf("YUI Perf [frame %llu] fps=%.1f frame=%.2fms render=%.2fms layers=%u\n",
               (unsigned long long)g_frame.frame_index,
               g_frame.fps,
               perf_ns_to_ms(g_frame.frame_ns),
               perf_ns_to_ms(g_frame.render_tree_ns),
               g_frame.layers_rendered);
        for (int i = 0; i < n; i++) {
            printf("  #%d %s type=%d count=%u self=%.2fms\n",
                   i + 1,
                   top[i].id && top[i].id[0] ? top[i].id : "(no-id)",
                   top[i].type,
                   top[i].render_count,
                   perf_ns_to_ms(top[i].render_ns));
        }
    }
}

void perf_render_tree_begin(void)
{
    if (!g_perf_enabled) {
        return;
    }
    g_render_tree_start_ns = perf_now_ns();
}

void perf_render_tree_end(void)
{
    if (!g_perf_enabled) {
        return;
    }
    g_frame.render_tree_ns = perf_now_ns() - g_render_tree_start_ns;
}

void perf_layer_tree_enter(Layer* layer)
{
    if (!g_perf_enabled || !layer) {
        return;
    }

    PerfSlot* slot = perf_get_slot(layer, 1);
    if (!slot) {
        return;
    }

    slot->frame_count++;
    g_frame.layers_rendered++;

    if (layer->id[0]) {
        strncpy(slot->id, layer->id, sizeof(slot->id) - 1);
        slot->id[sizeof(slot->id) - 1] = '\0';
    }
    slot->type = layer->type;
}

void perf_layer_add_self_ns(Layer* layer, uint64_t ns)
{
    if (!g_perf_enabled || !layer || ns == 0) {
        return;
    }

    PerfSlot* slot = perf_get_slot(layer, 1);
    if (!slot) {
        return;
    }
    slot->frame_self_ns += ns;
}

const PerfFrameStats* perf_get_frame_stats(void)
{
    return &g_frame;
}

static int perf_sort_time_desc(const void* a, const void* b)
{
    const PerfLayerStats* sa = (const PerfLayerStats*)a;
    const PerfLayerStats* sb = (const PerfLayerStats*)b;
    if (sb->render_ns > sa->render_ns) return 1;
    if (sb->render_ns < sa->render_ns) return -1;
    return 0;
}

static int perf_sort_count_desc(const void* a, const void* b)
{
    const PerfLayerStats* sa = (const PerfLayerStats*)a;
    const PerfLayerStats* sb = (const PerfLayerStats*)b;
    if (sb->render_count > sa->render_count) return 1;
    if (sb->render_count < sa->render_count) return -1;
    return 0;
}

static int perf_sort_name_asc(const void* a, const void* b)
{
    const PerfLayerStats* sa = (const PerfLayerStats*)a;
    const PerfLayerStats* sb = (const PerfLayerStats*)b;
    const char* na = sa->id ? sa->id : "";
    const char* nb = sb->id ? sb->id : "";
    return strcmp(na, nb);
}

int perf_get_layer_stats(PerfLayerStats* out, int max_count, PerfSortBy sort_by)
{
    if (!out || max_count <= 0) {
        return 0;
    }

    PerfLayerStats temp[PERF_MAX_SLOTS];
    int count = 0;

    for (int i = 0; i < g_slot_count && count < PERF_MAX_SLOTS; i++) {
        PerfSlot* slot = &g_slots[i];
        if (slot->frame_count == 0 && slot->frame_self_ns == 0) {
            continue;
        }
        temp[count].id = slot->id;
        temp[count].type = slot->type;
        temp[count].render_count = slot->frame_count;
        temp[count].render_ns = slot->frame_self_ns;
        temp[count].total_render_ns = slot->total_self_ns;
        temp[count].total_frames_seen = slot->total_frames_seen;
        temp[count].max_render_count = slot->max_frame_count;
        count++;
    }

    if (count == 0) {
        return 0;
    }

    if (sort_by == PERF_SORT_COUNT) {
        qsort(temp, (size_t)count, sizeof(PerfLayerStats), perf_sort_count_desc);
    } else if (sort_by == PERF_SORT_NAME) {
        qsort(temp, (size_t)count, sizeof(PerfLayerStats), perf_sort_name_asc);
    } else {
        qsort(temp, (size_t)count, sizeof(PerfLayerStats), perf_sort_time_desc);
    }

    if (count > max_count) {
        count = max_count;
    }
    memcpy(out, temp, (size_t)count * sizeof(PerfLayerStats));
    return count;
}

static void perf_draw_text_line(Layer* root, int x, int y, const char* text, Color color)
{
    if (!root || !text || !text[0]) {
        return;
    }
    Texture* tex = render_text(root, text, color);
    if (!tex) {
        return;
    }
    int tw = 0, th = 0;
    backend_query_texture(tex, NULL, NULL, &tw, &th);
    Rect dst = {x, y, tw / (int)scale, th / (int)scale};
    backend_render_text_copy(tex, NULL, &dst);
    backend_render_text_destroy(tex);
}

void perf_draw_overlay(Layer* root)
{
    if (!g_perf_enabled || !g_perf_overlay || !root) {
        return;
    }

    PerfLayerStats top[PERF_MAX_OVERLAY_LINES];
    int n = perf_get_layer_stats(top, g_perf_top_n, PERF_SORT_TIME);

    char line_buf[PERF_MAX_OVERLAY_LINES + 4][128];
    int line_count = 0;

    snprintf(line_buf[line_count++], sizeof(line_buf[0]),
             "FPS %.1f  Frame %.2fms  Render %.2fms  Layers %u",
             g_frame.fps,
             perf_ns_to_ms(g_frame.frame_ns),
             perf_ns_to_ms(g_frame.render_tree_ns),
             g_frame.layers_rendered);

    for (int i = 0; i < n; i++) {
        const char* name = top[i].id && top[i].id[0] ? top[i].id : yui_type_name(top[i].type);
        snprintf(line_buf[line_count++], sizeof(line_buf[0]),
                 "%s  %.2fms  x%u",
                 name,
                 perf_ns_to_ms(top[i].render_ns),
                 top[i].render_count);
    }

    int line_h = 14;
    int pad = 8;
    int panel_w = 320;
    int panel_h = pad * 2 + line_count * line_h + 4;

    Rect panel = {8, 8, panel_w, panel_h};
    Color panel_bg = {20, 20, 30, 210};
    backend_render_fill_rect(&panel, panel_bg);

    Color text_color = {230, 230, 230, 255};
    Color warn_color = {255, 180, 80, 255};
    int y = panel.y + pad;
    for (int i = 0; i < line_count; i++) {
        Color c = text_color;
        if (i > 0 && n > 0) {
            int stat_idx = i - 1;
            if (stat_idx < n &&
                (top[stat_idx].render_count > 1 ||
                 top[stat_idx].render_ns > 2000000ULL)) {
                c = warn_color;
            }
        }
        perf_draw_text_line(root, panel.x + pad, y, line_buf[i], c);
        y += line_h;
    }

    if (g_watch_count > 0) {
        for (int i = 0; i < g_slot_count; i++) {
            PerfSlot* slot = &g_slots[i];
            Layer* layer = slot->layer;
            if (!layer || !perf_is_watched_layer(layer)) {
                continue;
            }
            if (layer->rect.w <= 0 || layer->rect.h <= 0) {
                continue;
            }

            Color border = {80, 200, 255, 255};
            if (slot->frame_count > 1 || slot->frame_self_ns > 2000000ULL) {
                border = (Color){255, 120, 60, 255};
            }
            backend_render_rect(&layer->rect, border);

            char tag[96];
            snprintf(tag, sizeof(tag), "%s | %.2fms | x%u",
                     slot->id[0] ? slot->id : yui_type_name(slot->type),
                     perf_ns_to_ms(slot->frame_self_ns),
                     slot->frame_count);
            perf_draw_text_line(root, layer->rect.x, layer->rect.y - 14, tag, border);
        }
    }
}
