#include "game.h"
#include "internal.h"

#if YUI_WITH_GAME

#include <string.h>

void game_anim_update(GameEntity* e, float dt)
{
    int idx;
    if (!e || !e->alive || e->anim_frame_count <= 0 || e->anim_fps <= 0.0f) {
        return;
    }
    e->anim_t += dt;
    if (e->anim_t >= 1.0f / e->anim_fps) {
        e->anim_t = 0.0f;
        e->anim_frame = (e->anim_frame + 1) % e->anim_frame_count;
    }
    idx = e->anim_frame;
    if (idx < 0 || idx >= e->anim_frame_count) {
        idx = 0;
    }
    e->frame_x = e->anim_frames_x[idx];
    e->frame_y = e->anim_frames_y[idx];
    e->frame_w = e->anim_frames_w[idx];
    e->frame_h = e->anim_frames_h[idx];
    e->use_frame = 1;
}

int game_play_anim(GameEntity* e, const char* clip)
{
    if (!e || !clip) {
        return 0;
    }
    strncpy(e->anim_clip, clip, GAME_ID_LEN - 1);
    e->anim_clip[GAME_ID_LEN - 1] = '\0';
    e->anim_frame = 0;
    e->anim_t = 0.0f;
    return e->anim_frame_count > 0;
}

/* Parse anim from entity JSON: anim: { fps, frames: [{x,y,w,h}, ...] } or clip name only */
void game_anim_apply_json(GameEntity* e, cJSON* anim)
{
    cJSON* frames;
    cJSON* fr;
    cJSON* t;
    int i = 0;
    if (!e || !anim || !cJSON_IsObject(anim)) {
        return;
    }
    t = cJSON_GetObjectItem(anim, "clip");
    if (cJSON_IsString(t) && t->valuestring) {
        strncpy(e->anim_clip, t->valuestring, GAME_ID_LEN - 1);
    }
    t = cJSON_GetObjectItem(anim, "fps");
    if (cJSON_IsNumber(t)) {
        e->anim_fps = (float)t->valuedouble;
    } else {
        e->anim_fps = 8.0f;
    }
    frames = cJSON_GetObjectItem(anim, "frames");
    if (!cJSON_IsArray(frames)) {
        return;
    }
    cJSON_ArrayForEach(fr, frames) {
        if (i >= GAME_ANIM_FRAMES || !cJSON_IsObject(fr)) {
            break;
        }
        t = cJSON_GetObjectItem(fr, "x");
        e->anim_frames_x[i] = cJSON_IsNumber(t) ? t->valueint : 0;
        t = cJSON_GetObjectItem(fr, "y");
        e->anim_frames_y[i] = cJSON_IsNumber(t) ? t->valueint : 0;
        t = cJSON_GetObjectItem(fr, "w");
        e->anim_frames_w[i] = cJSON_IsNumber(t) ? t->valueint : (int)e->w;
        t = cJSON_GetObjectItem(fr, "h");
        e->anim_frames_h[i] = cJSON_IsNumber(t) ? t->valueint : (int)e->h;
        i++;
    }
    e->anim_frame_count = i;
    e->anim_frame = 0;
    e->anim_t = 0.0f;
    if (i > 0) {
        e->frame_x = e->anim_frames_x[0];
        e->frame_y = e->anim_frames_y[0];
        e->frame_w = e->anim_frames_w[0];
        e->frame_h = e->anim_frames_h[0];
        e->use_frame = 1;
    }
}

#endif
