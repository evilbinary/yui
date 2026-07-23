#include "game.h"
#include "internal.h"

#if YUI_WITH_GAME

#include <math.h>

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
    float dx;
    float dy;
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
}

void game_collide_resolve_solids(GameEntity* e, float dt)
{
    game_move_and_collide(e, dt);
}

#endif
