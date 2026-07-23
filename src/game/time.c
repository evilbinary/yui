#include "game.h"

#if YUI_WITH_GAME

#include "../backend.h"
#include <string.h>

static float g_dt;
static float g_scale = 1.0f;
static Uint32 g_last_ticks;
static int g_time_inited;

void game_time_reset(void)
{
    g_dt = 0.0f;
    g_scale = 1.0f;
    g_last_ticks = backend_get_ticks();
    g_time_inited = 1;
}

float game_time_tick(void)
{
    Uint32 now;
    float raw;
    if (!g_time_inited) {
        game_time_reset();
    }
    now = backend_get_ticks();
    raw = (float)(now - g_last_ticks) / 1000.0f;
    g_last_ticks = now;
    if (raw < 0.0f) {
        raw = 0.0f;
    }
    if (raw > 0.05f) {
        raw = 0.05f;
    }
    g_dt = raw * g_scale;
    return g_dt;
}

float game_time_dt(void)
{
    return g_dt;
}

float game_time_scale(void)
{
    return g_scale;
}

void game_time_set_scale(float scale)
{
    g_scale = scale > 0.0f ? scale : 0.0f;
}

#endif
