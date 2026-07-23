#include "game.h"
#include "internal.h"

#if YUI_WITH_GAME

#include "../perf/perf.h"
#include <string.h>

static GamePerfStats g_stats;
static uint64_t g_upd_t0;
static uint64_t g_ren_t0;

void game_perf_begin_update(void)
{
    g_upd_t0 = perf_now_ns();
}

void game_perf_end_update(void)
{
    uint64_t dt = perf_now_ns() - g_upd_t0;
    g_stats.update_ms = (double)dt / 1.0e6;
}

void game_perf_begin_render(void)
{
    g_ren_t0 = perf_now_ns();
}

void game_perf_end_render(int entity_draws)
{
    uint64_t dt = perf_now_ns() - g_ren_t0;
    int n = 0;
    int i;
    GameEntity* all = game_entities(&n);
    int alive = 0;
    const PerfFrameStats* fs = perf_get_frame_stats();
    for (i = 0; i < n; i++) {
        if (all[i].alive) {
            alive++;
        }
    }
    g_stats.render_ms = (double)dt / 1.0e6;
    g_stats.entities = alive;
    g_stats.draws = entity_draws + game_particles_draw_count();
    g_stats.particles = game_particles_draw_count();
    if (fs) {
        g_stats.fps = fs->fps;
    }
}

const GamePerfStats* game_perf_get_stats(void)
{
    return &g_stats;
}

#endif
