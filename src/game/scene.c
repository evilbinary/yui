#include "game.h"
#include "internal.h"

#if YUI_WITH_GAME

#include "../backend.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_scene_active;

void game_clear_scene(void)
{
    game_entity_pool_clear();
    game_camera_init();
    g_scene_active = 0;
}

static void game_apply_sprite(GameEntity* e, const char* path)
{
    if (!e || !path || !path[0]) {
        return;
    }
    strncpy(e->sprite_path, path, GAME_PATH_LEN - 1);
    e->sprite_path[GAME_PATH_LEN - 1] = '\0';
    e->texture = backend_load_texture((char*)path);
}

static Color game_parse_color(cJSON* node, Color fallback)
{
    Color c = fallback;
    if (!node) {
        return c;
    }
    if (cJSON_IsString(node) && node->valuestring) {
        const char* s = node->valuestring;
        unsigned int rgb = 0;
        if (s[0] == '#' && strlen(s) >= 7) {
            sscanf(s + 1, "%06x", &rgb);
            c.r = (unsigned char)((rgb >> 16) & 0xff);
            c.g = (unsigned char)((rgb >> 8) & 0xff);
            c.b = (unsigned char)(rgb & 0xff);
            c.a = 255;
        }
    } else if (cJSON_IsObject(node)) {
        cJSON* r = cJSON_GetObjectItem(node, "r");
        cJSON* g = cJSON_GetObjectItem(node, "g");
        cJSON* b = cJSON_GetObjectItem(node, "b");
        cJSON* a = cJSON_GetObjectItem(node, "a");
        if (cJSON_IsNumber(r)) c.r = (unsigned char)r->valueint;
        if (cJSON_IsNumber(g)) c.g = (unsigned char)g->valueint;
        if (cJSON_IsNumber(b)) c.b = (unsigned char)b->valueint;
        if (cJSON_IsNumber(a)) c.a = (unsigned char)a->valueint;
    }
    return c;
}

GameEntity* game_spawn_from_json(cJSON* obj)
{
    GameEntity* e;
    cJSON* t;
    cJSON* sp;
    cJSON* col;
    const char* id = NULL;
    if (!obj || !cJSON_IsObject(obj)) {
        return NULL;
    }
    t = cJSON_GetObjectItem(obj, "id");
    if (cJSON_IsString(t) && t->valuestring) {
        id = t->valuestring;
    }
    e = game_spawn(id);
    if (!e) {
        return NULL;
    }

    t = cJSON_GetObjectItem(obj, "tag");
    if (cJSON_IsString(t) && t->valuestring) {
        strncpy(e->tag, t->valuestring, GAME_ID_LEN - 1);
    }
    t = cJSON_GetObjectItem(obj, "script");
    if (cJSON_IsString(t) && t->valuestring) {
        strncpy(e->script, t->valuestring, GAME_ID_LEN - 1);
    }

    sp = cJSON_GetObjectItem(obj, "transform");
    if (cJSON_IsObject(sp)) {
        t = cJSON_GetObjectItem(sp, "x");
        if (cJSON_IsNumber(t)) e->x = (float)t->valuedouble;
        t = cJSON_GetObjectItem(sp, "y");
        if (cJSON_IsNumber(t)) e->y = (float)t->valuedouble;
        t = cJSON_GetObjectItem(sp, "z");
        if (cJSON_IsNumber(t)) e->z = (float)t->valuedouble;
    }

    sp = cJSON_GetObjectItem(obj, "sprite");
    if (cJSON_IsObject(sp)) {
        t = cJSON_GetObjectItem(sp, "src");
        if (cJSON_IsString(t) && t->valuestring) {
            game_apply_sprite(e, t->valuestring);
        }
        t = cJSON_GetObjectItem(sp, "w");
        if (cJSON_IsNumber(t)) e->w = (float)t->valuedouble;
        t = cJSON_GetObjectItem(sp, "h");
        if (cJSON_IsNumber(t)) e->h = (float)t->valuedouble;
        t = cJSON_GetObjectItem(sp, "color");
        e->color = game_parse_color(t, e->color);
    } else if (cJSON_IsString(sp) && sp->valuestring) {
        game_apply_sprite(e, sp->valuestring);
    }

    t = cJSON_GetObjectItem(obj, "color");
    e->color = game_parse_color(t, e->color);

    col = cJSON_GetObjectItem(obj, "collider");
    if (cJSON_IsObject(col)) {
        t = cJSON_GetObjectItem(col, "w");
        if (cJSON_IsNumber(t)) e->cw = (float)t->valuedouble;
        t = cJSON_GetObjectItem(col, "h");
        if (cJSON_IsNumber(t)) e->ch = (float)t->valuedouble;
        t = cJSON_GetObjectItem(col, "ox");
        if (cJSON_IsNumber(t)) e->cox = (float)t->valuedouble;
        t = cJSON_GetObjectItem(col, "oy");
        if (cJSON_IsNumber(t)) e->coy = (float)t->valuedouble;
        t = cJSON_GetObjectItem(col, "trigger");
        if (cJSON_IsTrue(t) || (cJSON_IsNumber(t) && t->valueint)) e->trigger = 1;
        t = cJSON_GetObjectItem(col, "solid");
        if (cJSON_IsTrue(t) || (cJSON_IsNumber(t) && t->valueint)) e->solid = 1;
    }

    t = cJSON_GetObjectItem(obj, "solid");
    if (cJSON_IsTrue(t) || (cJSON_IsNumber(t) && t->valueint)) {
        e->solid = 1;
    }
    t = cJSON_GetObjectItem(obj, "vx");
    if (cJSON_IsNumber(t)) e->vx = (float)t->valuedouble;
    t = cJSON_GetObjectItem(obj, "vy");
    if (cJSON_IsNumber(t)) e->vy = (float)t->valuedouble;

    return e;
}

int game_load_scene_json(const char* path)
{
    FILE* fp;
    long size;
    char* buf;
    cJSON* root;
    cJSON* ents;
    cJSON* cam;
    cJSON* child;
    GameCamera* camera;
    if (!path) {
        return 0;
    }
    fp = fopen(path, "rb");
    if (!fp) {
        printf("Game: failed to open scene %s\n", path);
        return 0;
    }
    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (size <= 0) {
        fclose(fp);
        return 0;
    }
    buf = (char*)malloc((size_t)size + 1);
    if (!buf) {
        fclose(fp);
        return 0;
    }
    if (fread(buf, 1, (size_t)size, fp) != (size_t)size) {
        free(buf);
        fclose(fp);
        return 0;
    }
    buf[size] = '\0';
    fclose(fp);

    root = cJSON_Parse(buf);
    free(buf);
    if (!root) {
        printf("Game: invalid scene JSON %s\n", path);
        return 0;
    }

    game_clear_scene();
    camera = game_camera();

    cam = cJSON_GetObjectItem(root, "camera");
    if (cJSON_IsObject(cam) && camera) {
        cJSON* t = cJSON_GetObjectItem(cam, "x");
        if (cJSON_IsNumber(t)) camera->x = (float)t->valuedouble;
        t = cJSON_GetObjectItem(cam, "y");
        if (cJSON_IsNumber(t)) camera->y = (float)t->valuedouble;
        t = cJSON_GetObjectItem(cam, "follow");
        if (cJSON_IsString(t) && t->valuestring) {
            game_camera_follow(t->valuestring);
        }
    }

    ents = cJSON_GetObjectItem(root, "entities");
    if (cJSON_IsArray(ents)) {
        cJSON_ArrayForEach(child, ents) {
            game_spawn_from_json(child);
        }
    }

    cJSON_Delete(root);
    g_scene_active = 1;
    printf("Game: loaded scene %s\n", path);
    return 1;
}

#endif
