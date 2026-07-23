#include "game.h"
#include "internal.h"

#if YUI_WITH_GAME

#include "../backend.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define GAME_TILEMAP_MAX_CELLS (128 * 128)

typedef struct GameTilemap {
    int active;
    int cols;
    int rows;
    int tile_w;
    int tile_h;
    int solid_nonzero; /* non-zero tiles are solid */
    int tiles[GAME_TILEMAP_MAX_CELLS];
    char src[GAME_PATH_LEN];
    Texture* texture;
    int tileset_cols; /* frames per row in tileset; 0 = colored rects */
} GameTilemap;

static GameTilemap g_tm;

void game_tilemap_clear(void)
{
    memset(&g_tm, 0, sizeof(g_tm));
    g_tm.solid_nonzero = 1;
}

int game_tilemap_load_from_json(cJSON* node)
{
    cJSON* map;
    cJSON* row;
    cJSON* cell;
    cJSON* t;
    int r;
    int c;
    int idx;
    if (!node || !cJSON_IsObject(node)) {
        return 0;
    }
    game_tilemap_clear();
    t = cJSON_GetObjectItem(node, "tileSize");
    if (cJSON_IsNumber(t)) {
        g_tm.tile_w = t->valueint;
        g_tm.tile_h = t->valueint;
    } else if (cJSON_IsArray(t) && cJSON_GetArraySize(t) >= 2) {
        g_tm.tile_w = cJSON_GetArrayItem(t, 0)->valueint;
        g_tm.tile_h = cJSON_GetArrayItem(t, 1)->valueint;
    } else {
        g_tm.tile_w = 32;
        g_tm.tile_h = 32;
    }
    t = cJSON_GetObjectItem(node, "src");
    if (cJSON_IsString(t) && t->valuestring) {
        strncpy(g_tm.src, t->valuestring, GAME_PATH_LEN - 1);
        g_tm.texture = backend_load_texture((char*)t->valuestring);
    }
    t = cJSON_GetObjectItem(node, "tilesetCols");
    if (cJSON_IsNumber(t)) {
        g_tm.tileset_cols = t->valueint;
    }
    t = cJSON_GetObjectItem(node, "solidNonZero");
    if (cJSON_IsFalse(t) || (cJSON_IsNumber(t) && !t->valueint)) {
        g_tm.solid_nonzero = 0;
    }
    map = cJSON_GetObjectItem(node, "map");
    if (!cJSON_IsArray(map)) {
        return 0;
    }
    g_tm.rows = cJSON_GetArraySize(map);
    if (g_tm.rows <= 0) {
        return 0;
    }
    row = cJSON_GetArrayItem(map, 0);
    g_tm.cols = cJSON_IsArray(row) ? cJSON_GetArraySize(row) : 0;
    if (g_tm.cols <= 0 || g_tm.rows * g_tm.cols > GAME_TILEMAP_MAX_CELLS) {
        printf("Game tilemap: invalid size %dx%d\n", g_tm.cols, g_tm.rows);
        return 0;
    }
    for (r = 0; r < g_tm.rows; r++) {
        row = cJSON_GetArrayItem(map, r);
        if (!cJSON_IsArray(row)) {
            continue;
        }
        for (c = 0; c < g_tm.cols; c++) {
            cell = cJSON_GetArrayItem(row, c);
            idx = r * g_tm.cols + c;
            g_tm.tiles[idx] = cJSON_IsNumber(cell) ? cell->valueint : 0;
        }
    }
    g_tm.active = 1;
    printf("Game tilemap: %dx%d tile=%dx%d\n", g_tm.cols, g_tm.rows, g_tm.tile_w, g_tm.tile_h);
    return 1;
}

void game_tilemap_render(void)
{
    int r;
    int c;
    int tid;
    float sx;
    float sy;
    Rect dst;
    Rect src;
    Color col;
    if (!g_tm.active) {
        return;
    }
    for (r = 0; r < g_tm.rows; r++) {
        for (c = 0; c < g_tm.cols; c++) {
            tid = g_tm.tiles[r * g_tm.cols + c];
            if (tid == 0) {
                continue;
            }
            game_camera_world_to_screen((float)(c * g_tm.tile_w), (float)(r * g_tm.tile_h), &sx, &sy);
            dst.x = (int)sx;
            dst.y = (int)sy;
            dst.w = g_tm.tile_w;
            dst.h = g_tm.tile_h;
            if (g_tm.texture && g_tm.tileset_cols > 0) {
                int tc = (tid - 1) % g_tm.tileset_cols;
                int tr = (tid - 1) / g_tm.tileset_cols;
                src.x = tc * g_tm.tile_w;
                src.y = tr * g_tm.tile_h;
                src.w = g_tm.tile_w;
                src.h = g_tm.tile_h;
                backend_render_text_copy(g_tm.texture, &src, &dst);
            } else {
                col = (Color){60, 80, 70, 255};
                if (tid == 2) {
                    col = (Color){80, 70, 100, 255};
                }
                backend_render_fill_rect(&dst, col);
            }
        }
    }
}

void game_tilemap_collide(GameEntity* e)
{
    float ax, ay, aw, ah;
    int c0, c1, r0, r1;
    int r, c, tid;
    float bx, by, bw, bh;
    float overlap_x, overlap_y;
    float nx, ny;
    if (!g_tm.active || !e || !e->alive || e->solid) {
        return;
    }
    game_entity_world_aabb(e, &ax, &ay, &aw, &ah);
    c0 = (int)(ax / g_tm.tile_w) - 1;
    c1 = (int)((ax + aw) / g_tm.tile_w) + 1;
    r0 = (int)(ay / g_tm.tile_h) - 1;
    r1 = (int)((ay + ah) / g_tm.tile_h) + 1;
    if (c0 < 0) c0 = 0;
    if (r0 < 0) r0 = 0;
    if (c1 >= g_tm.cols) c1 = g_tm.cols - 1;
    if (r1 >= g_tm.rows) r1 = g_tm.rows - 1;
    for (r = r0; r <= r1; r++) {
        for (c = c0; c <= c1; c++) {
            tid = g_tm.tiles[r * g_tm.cols + c];
            if (!g_tm.solid_nonzero || tid == 0) {
                continue;
            }
            bx = (float)(c * g_tm.tile_w);
            by = (float)(r * g_tm.tile_h);
            bw = (float)g_tm.tile_w;
            bh = (float)g_tm.tile_h;
            if (!game_aabb_overlap(ax, ay, aw, ah, bx, by, bw, bh)) {
                continue;
            }
            overlap_x = (ax + aw * 0.5f < bx + bw * 0.5f)
                            ? (ax + aw) - bx
                            : (bx + bw) - ax;
            overlap_y = (ay + ah * 0.5f < by + bh * 0.5f)
                            ? (ay + ah) - by
                            : (by + bh) - ay;
            if (overlap_x < overlap_y) {
                nx = (ax + aw * 0.5f < bx + bw * 0.5f) ? -overlap_x : overlap_x;
                ny = 0;
            } else {
                nx = 0;
                ny = (ay + ah * 0.5f < by + bh * 0.5f) ? -overlap_y : overlap_y;
            }
            e->x += nx;
            e->y += ny;
            ax += nx;
            ay += ny;
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

#endif
