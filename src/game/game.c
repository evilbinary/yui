#include "game.h"
#include "internal.h"

#if YUI_WITH_GAME

#include "../event.h"
#include <stdlib.h>
#include <string.h>

static int g_inited;
static int g_enabled = 1;
static int g_paused;
static GameScriptUpdateFn g_script_update;
static int g_entity_draws;

static void game_on_window_event(const WindowEvent* event)
{
    if (!event) {
        return;
    }
    switch (event->type) {
    case WINDOW_FOCUS_LOST:
    case WINDOW_MINIMIZED:
        game_set_paused(1);
        break;
    case WINDOW_FOCUS_GAINED:
    case WINDOW_RESTORED:
        game_set_paused(0);
        break;
    default:
        break;
    }
}

void game_init(void)
{
    if (g_inited) {
        return;
    }
    game_entity_pool_init();
    game_camera_init();
    game_input_reset();
    game_time_reset();
    game_tilemap_clear();
    game_particles_clear();
    game_audio_init();
    g_script_update = NULL;
    g_enabled = 1;
    g_paused = 0;
    register_window_event_listener(game_on_window_event);
    g_inited = 1;
}

void game_shutdown(void)
{
    if (!g_inited) {
        return;
    }
    unregister_window_event_listener(game_on_window_event);
    game_clear_scene();
    game_audio_shutdown();
    g_script_update = NULL;
    g_paused = 0;
    g_inited = 0;
}

int game_is_active(void)
{
    return g_inited && g_enabled && !g_paused;
}

void game_set_enabled(int on)
{
    g_enabled = on ? 1 : 0;
}

void game_set_paused(int on)
{
    int next = on ? 1 : 0;
    if (next == g_paused) {
        return;
    }
    g_paused = next;
    if (!g_paused) {
        game_time_reset();
    }
}

int game_is_paused(void)
{
    return g_paused;
}

void game_set_script_update_fn(GameScriptUpdateFn fn)
{
    g_script_update = fn;
}

static int game_entity_z_cmp(const void* a, const void* b)
{
    const GameEntity* ea = *(const GameEntity* const*)a;
    const GameEntity* eb = *(const GameEntity* const*)b;
    if (ea->z < eb->z) return -1;
    if (ea->z > eb->z) return 1;
    return 0;
}

void game_update(float dt_override)
{
    float dt;
    int n = 0;
    int i;
    GameEntity* all;
    if (!g_inited || !g_enabled || g_paused) {
        return;
    }
    game_perf_begin_update();
    game_input_begin_frame();
    dt = dt_override >= 0.0f ? dt_override : game_time_tick();

    all = game_entities(&n);
    for (i = 0; i < n; i++) {
        GameEntity* e = &all[i];
        if (!e->alive) {
            continue;
        }
        if (g_script_update && e->script[0]) {
            g_script_update(e, dt);
        }
        if (!e->alive) {
            continue;
        }
        game_anim_update(e, dt);
        if (!e->solid) {
            game_move_and_collide(e, dt);
        }
    }
    game_trigger_update();
    game_particles_update(dt);
    game_camera_update();
    game_perf_end_update();
}

void game_render(void)
{
    int n = 0;
    int i;
    int count = 0;
    GameEntity* all;
    GameEntity* sorted[GAME_MAX_ENTITIES];
    if (!g_inited || !g_enabled) {
        return;
    }
    game_perf_begin_render();
    g_entity_draws = 0;
    game_tilemap_render();
    all = game_entities(&n);
    for (i = 0; i < n; i++) {
        if (all[i].alive) {
            sorted[count++] = &all[i];
        }
    }
    if (count > 1) {
        qsort(sorted, (size_t)count, sizeof(GameEntity*), game_entity_z_cmp);
    }
    for (i = 0; i < count; i++) {
        game_sprite_draw_entity(sorted[i]);
        g_entity_draws++;
    }
    game_particles_render();
    game_perf_end_render(g_entity_draws);
}

#endif
