#ifndef YUI_PERF_H
#define YUI_PERF_H

#include "../ytype.h"

typedef struct Layer Layer;

typedef struct PerfFrameStats {
    uint64_t frame_index;
    uint64_t frame_ns;
    uint64_t render_tree_ns;
    uint32_t layers_rendered;
    double fps;
} PerfFrameStats;

typedef enum PerfSortBy {
    PERF_SORT_TIME = 0,
    PERF_SORT_COUNT = 1,
    PERF_SORT_NAME = 2,
} PerfSortBy;

typedef struct PerfLayerStats {
    const char* id;
    int type;
    uint32_t render_count;
    uint64_t render_ns;
    uint64_t total_render_ns;
    uint64_t total_frames_seen;
    uint32_t max_render_count;
} PerfLayerStats;

int perf_is_enabled(void);
void perf_enable(int on);
void perf_reset(void);
void perf_set_overlay(int on);
int perf_overlay_enabled(void);
void perf_set_top_n(int n);
void perf_set_log_interval(int frames);
int perf_watch(const char* layer_id);
int perf_unwatch(const char* layer_id);
void perf_clear_watch(void);

void perf_frame_begin(void);
void perf_frame_end(void);
void perf_render_tree_begin(void);
void perf_render_tree_end(void);

void perf_layer_tree_enter(Layer* layer);
void perf_layer_add_self_ns(Layer* layer, uint64_t ns);
uint64_t perf_now_ns(void);

void perf_draw_overlay(Layer* root);

const PerfFrameStats* perf_get_frame_stats(void);
int perf_get_layer_stats(PerfLayerStats* out, int max_count, PerfSortBy sort_by);

#endif
