#include "game.h"
#include "internal.h"

#if YUI_WITH_GAME

#include <math.h>
#include <string.h>

#define GAME_MAX_TRIGGER_PAIRS 256

typedef struct TriggerPair {
    GameEntity* a;
    GameEntity* b;
    int active;
} TriggerPair;

static TriggerPair g_prev_pairs[GAME_MAX_TRIGGER_PAIRS];
static int g_prev_count;
static GameTriggerFn g_trigger_fn;

void game_set_trigger_fn(GameTriggerFn fn)
{
    g_trigger_fn = fn;
}

void game_entity_world_aabb(const GameEntity* e, float* out_x, float* out_y,
                           float* out_w, float* out_h)
{
    float w;
    float h;
    if (!e) {
        return;
    }
    w = e->cw > 0 ? e->cw : e->w;
    h = e->ch > 0 ? e->ch : e->h;
    if (out_x) *out_x = e->x + e->cox;
    if (out_y) *out_y = e->y + e->coy;
    if (out_w) *out_w = w;
    if (out_h) *out_h = h;
}

int game_aabb_overlap(float ax, float ay, float aw, float ah,
                      float bx, float by, float bw, float bh)
{
    return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
}

int game_entities_overlap(const GameEntity* a, const GameEntity* b)
{
    float ax, ay, aw, ah, bx, by, bw, bh;
    if (!a || !b || !a->alive || !b->alive) {
        return 0;
    }
    game_entity_world_aabb(a, &ax, &ay, &aw, &ah);
    game_entity_world_aabb(b, &bx, &by, &bw, &bh);
    return game_aabb_overlap(ax, ay, aw, ah, bx, by, bw, bh);
}

static int game_entity_vs_solid(GameEntity* e, GameEntity* solid, float* out_nx, float* out_ny)
{
    float ax, ay, aw, ah, bx, by, bw, bh;
    float overlap_x;
    float overlap_y;
    if (!e || !solid || !solid->solid || solid->trigger) {
        return 0;
    }
    game_entity_world_aabb(e, &ax, &ay, &aw, &ah);
    game_entity_world_aabb(solid, &bx, &by, &bw, &bh);
    if (!game_aabb_overlap(ax, ay, aw, ah, bx, by, bw, bh)) {
        return 0;
    }
    overlap_x = (ax + aw * 0.5f < bx + bw * 0.5f)
                    ? (ax + aw) - bx
                    : (bx + bw) - ax;
    overlap_y = (ay + ah * 0.5f < by + bh * 0.5f)
                    ? (ay + ah) - by
                    : (by + bh) - ay;
    if (overlap_x < overlap_y) {
        if (out_nx) {
            *out_nx = (ax + aw * 0.5f < bx + bw * 0.5f) ? -overlap_x : overlap_x;
        }
        if (out_ny) {
            *out_ny = 0;
        }
    } else {
        if (out_nx) {
            *out_nx = 0;
        }
        if (out_ny) {
            *out_ny = (ay + ah * 0.5f < by + bh * 0.5f) ? -overlap_y : overlap_y;
        }
    }
    return 1;
}

void game_move_and_collide(GameEntity* e, float dt)
{
    int n = 0;
    GameEntity* all;
    int i;
    float nx;
    float ny;
    if (!e || !e->alive || e->solid) {
        return;
    }
    e->grounded = 0;
    e->x += e->vx * dt;
    e->y += e->vy * dt;

    all = game_entities(&n);
    for (i = 0; i < n; i++) {
        if (!all[i].alive || &all[i] == e || !all[i].solid) {
            continue;
        }
        if (game_entity_vs_solid(e, &all[i], &nx, &ny)) {
            e->x += nx;
            e->y += ny;
            if (ny < 0 && e->vy > 0) {
                e->vy = 0;
                e->grounded = 1;
            } else if (ny > 0 && e->vy < 0) {
                e->vy = 0;
            }
            if (nx != 0) {
                e->vx = 0;
            }
        }
    }
    game_tilemap_collide(e);
}

void game_collide_resolve_solids(GameEntity* e, float dt)
{
    game_move_and_collide(e, dt);
}

static int pair_in_list(GameEntity* a, GameEntity* b, TriggerPair* list, int count)
{
    int i;
    for (i = 0; i < count; i++) {
        if ((list[i].a == a && list[i].b == b) || (list[i].a == b && list[i].b == a)) {
            return 1;
        }
    }
    return 0;
}

void game_trigger_update(void)
{
    TriggerPair cur[GAME_MAX_TRIGGER_PAIRS];
    int cur_count = 0;
    int n = 0;
    int i, j;
    GameEntity* all = game_entities(&n);

    for (i = 0; i < n; i++) {
        if (!all[i].alive || !all[i].trigger) {
            continue;
        }
        for (j = 0; j < n; j++) {
            if (i == j || !all[j].alive) {
                continue;
            }
            /* Only emit once per unordered pair; prefer trigger entity as a */
            if (j < i && all[j].trigger) {
                continue;
            }
            if (!game_entities_overlap(&all[i], &all[j])) {
                continue;
            }
            if (cur_count < GAME_MAX_TRIGGER_PAIRS) {
                cur[cur_count].a = &all[i];
                cur[cur_count].b = &all[j];
                cur[cur_count].active = 1;
                cur_count++;
            }
        }
    }

    if (g_trigger_fn) {
        for (i = 0; i < cur_count; i++) {
            if (pair_in_list(cur[i].a, cur[i].b, g_prev_pairs, g_prev_count)) {
                g_trigger_fn(cur[i].a, cur[i].b, GAME_TRIGGER_STAY);
            } else {
                g_trigger_fn(cur[i].a, cur[i].b, GAME_TRIGGER_ENTER);
            }
        }
        for (i = 0; i < g_prev_count; i++) {
            if (!pair_in_list(g_prev_pairs[i].a, g_prev_pairs[i].b, cur, cur_count)) {
                if (g_prev_pairs[i].a && g_prev_pairs[i].a->alive &&
                    g_prev_pairs[i].b && g_prev_pairs[i].b->alive) {
                    g_trigger_fn(g_prev_pairs[i].a, g_prev_pairs[i].b, GAME_TRIGGER_EXIT);
                }
            }
        }
    }

    memcpy(g_prev_pairs, cur, sizeof(TriggerPair) * (size_t)cur_count);
    g_prev_count = cur_count;
}

#endif
