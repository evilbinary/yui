#include "game.h"
#include "internal.h"

#if YUI_WITH_GAME

#include "../backend.h"
#include <stdlib.h>
#include <math.h>
#include <string.h>

typedef struct GameParticle {
    int alive;
    float x, y;
    float vx, vy;
    float life;
    float life_max;
    float size;
    Color color;
} GameParticle;

static GameParticle g_parts[GAME_MAX_PARTICLES];
static int g_part_draws;

void game_particles_clear(void)
{
    memset(g_parts, 0, sizeof(g_parts));
    g_part_draws = 0;
}

int game_spawn_particles(float x, float y, int count, Color color, float speed, float life)
{
    int spawned = 0;
    int i;
    int slot;
    if (count < 1) {
        count = 1;
    }
    if (count > 64) {
        count = 64;
    }
    if (speed <= 0) {
        speed = 80.0f;
    }
    if (life <= 0) {
        life = 0.4f;
    }
    for (i = 0; i < count; i++) {
        for (slot = 0; slot < GAME_MAX_PARTICLES; slot++) {
            if (!g_parts[slot].alive) {
                float ang = ((float)rand() / (float)RAND_MAX) * 6.2831853f;
                float spd = speed * (0.4f + ((float)rand() / (float)RAND_MAX) * 0.8f);
                g_parts[slot].alive = 1;
                g_parts[slot].x = x;
                g_parts[slot].y = y;
                g_parts[slot].vx = cosf(ang) * spd;
                g_parts[slot].vy = sinf(ang) * spd;
                g_parts[slot].life = life;
                g_parts[slot].life_max = life;
                g_parts[slot].size = 3.0f + ((float)rand() / (float)RAND_MAX) * 4.0f;
                g_parts[slot].color = color;
                spawned++;
                break;
            }
        }
    }
    return spawned;
}

void game_particles_update(float dt)
{
    int i;
    for (i = 0; i < GAME_MAX_PARTICLES; i++) {
        if (!g_parts[i].alive) {
            continue;
        }
        g_parts[i].life -= dt;
        if (g_parts[i].life <= 0) {
            g_parts[i].alive = 0;
            continue;
        }
        g_parts[i].x += g_parts[i].vx * dt;
        g_parts[i].y += g_parts[i].vy * dt;
        g_parts[i].vy += 200.0f * dt;
    }
}

void game_particles_render(void)
{
    int i;
    float sx, sy;
    Rect dst;
    Color c;
    float a;
    g_part_draws = 0;
    for (i = 0; i < GAME_MAX_PARTICLES; i++) {
        if (!g_parts[i].alive) {
            continue;
        }
        a = g_parts[i].life / g_parts[i].life_max;
        c = g_parts[i].color;
        c.a = (unsigned char)((float)c.a * a);
        game_camera_world_to_screen(g_parts[i].x, g_parts[i].y, &sx, &sy);
        dst.x = (int)sx;
        dst.y = (int)sy;
        dst.w = (int)g_parts[i].size;
        dst.h = (int)g_parts[i].size;
        if (dst.w < 1) dst.w = 1;
        if (dst.h < 1) dst.h = 1;
        backend_render_fill_rect(&dst, c);
        g_part_draws++;
    }
}

int game_particles_draw_count(void)
{
    return g_part_draws;
}

#endif
