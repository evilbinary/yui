#include "game.h"
#include "internal.h"

#if YUI_WITH_GAME

#include <stdlib.h>
#include <string.h>

static GameEntity g_entities[GAME_MAX_ENTITIES];
static int g_entity_count; /* high-water for iteration */

void game_entity_pool_init(void)
{
    memset(g_entities, 0, sizeof(g_entities));
    g_entity_count = 0;
}

void game_entity_pool_clear(void)
{
    int i;
    for (i = 0; i < GAME_MAX_ENTITIES; i++) {
        if (g_entities[i].alive && g_entities[i].texture) {
            /* textures from backend_load_texture are owned by SDL; do not destroy cache */
            g_entities[i].texture = NULL;
        }
        g_entities[i].alive = 0;
    }
    g_entity_count = 0;
}

GameEntity* game_entity_alloc(void)
{
    int i;
    for (i = 0; i < GAME_MAX_ENTITIES; i++) {
        if (!g_entities[i].alive) {
            memset(&g_entities[i], 0, sizeof(GameEntity));
            g_entities[i].alive = 1;
            g_entities[i].w = 16;
            g_entities[i].h = 16;
            g_entities[i].color = (Color){200, 200, 200, 255};
            if (i + 1 > g_entity_count) {
                g_entity_count = i + 1;
            }
            return &g_entities[i];
        }
    }
    return NULL;
}

void game_entity_free(GameEntity* e)
{
    if (!e) {
        return;
    }
    e->alive = 0;
    e->texture = NULL;
    e->id[0] = '\0';
}

GameEntity* game_spawn(const char* id)
{
    GameEntity* e = game_entity_alloc();
    if (!e) {
        return NULL;
    }
    if (id && id[0]) {
        strncpy(e->id, id, GAME_ID_LEN - 1);
        e->id[GAME_ID_LEN - 1] = '\0';
    }
    return e;
}

void game_destroy(GameEntity* e)
{
    game_entity_free(e);
}

void game_destroy_by_id(const char* id)
{
    GameEntity* e = game_find(id);
    if (e) {
        game_destroy(e);
    }
}

GameEntity* game_find(const char* id)
{
    int i;
    if (!id) {
        return NULL;
    }
    for (i = 0; i < g_entity_count; i++) {
        if (g_entities[i].alive && strcmp(g_entities[i].id, id) == 0) {
            return &g_entities[i];
        }
    }
    return NULL;
}

GameEntity* game_find_by_tag(const char* tag)
{
    int i;
    if (!tag) {
        return NULL;
    }
    for (i = 0; i < g_entity_count; i++) {
        if (g_entities[i].alive && strcmp(g_entities[i].tag, tag) == 0) {
            return &g_entities[i];
        }
    }
    return NULL;
}

GameEntity* game_entities(int* out_count)
{
    if (out_count) {
        *out_count = g_entity_count;
    }
    return g_entities;
}

#endif
