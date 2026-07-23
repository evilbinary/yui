#include "game.h"
#include "internal.h"

#if YUI_WITH_GAME

#include "../backend.h"
#include <string.h>

static GameCamera g_camera;

void game_camera_init(void)
{
    memset(&g_camera, 0, sizeof(g_camera));
}

GameCamera* game_camera(void)
{
    return &g_camera;
}

void game_camera_set(float x, float y)
{
    g_camera.x = x;
    g_camera.y = y;
}

void game_camera_follow(const char* id)
{
    if (!id) {
        g_camera.follow[0] = '\0';
        return;
    }
    strncpy(g_camera.follow, id, GAME_ID_LEN - 1);
    g_camera.follow[GAME_ID_LEN - 1] = '\0';
}

void game_camera_update(void)
{
    GameEntity* e;
    int ww = 800;
    int wh = 600;
    if (!g_camera.follow[0]) {
        return;
    }
    e = game_find(g_camera.follow);
    if (!e) {
        return;
    }
    backend_get_windowsize(&ww, &wh);
    g_camera.x = e->x + e->w * 0.5f - (float)ww * 0.5f;
    g_camera.y = e->y + e->h * 0.5f - (float)wh * 0.5f;
}

void game_camera_world_to_screen(float wx, float wy, float* sx, float* sy)
{
    if (sx) {
        *sx = wx - g_camera.x;
    }
    if (sy) {
        *sy = wy - g_camera.y;
    }
}

#endif
