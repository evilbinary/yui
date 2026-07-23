#include "game.h"
#include "internal.h"

#if YUI_WITH_GAME

#include <stdio.h>
#include <string.h>

#ifndef YUI_WITH_GAME_AUDIO
#define YUI_WITH_GAME_AUDIO 1
#endif

#if YUI_WITH_GAME_AUDIO
#define MA_NO_ENCODING
#define MA_NO_GENERATION
#define MINIAUDIO_IMPLEMENTATION
#include "../../lib/miniaudio/miniaudio.h"

static ma_engine g_engine;
static int g_engine_ok;
static ma_sound g_bgm;
static int g_bgm_active;
#endif

void game_audio_init(void)
{
#if YUI_WITH_GAME_AUDIO
    ma_result r;
    if (g_engine_ok) {
        return;
    }
    r = ma_engine_init(NULL, &g_engine);
    if (r != MA_SUCCESS) {
        printf("Game audio: ma_engine_init failed (%d), audio disabled\n", (int)r);
        g_engine_ok = 0;
        return;
    }
    g_engine_ok = 1;
    g_bgm_active = 0;
    printf("Game audio: miniaudio ready\n");
#else
    (void)0;
#endif
}

void game_audio_shutdown(void)
{
#if YUI_WITH_GAME_AUDIO
    if (g_bgm_active) {
        ma_sound_uninit(&g_bgm);
        g_bgm_active = 0;
    }
    if (g_engine_ok) {
        ma_engine_uninit(&g_engine);
        g_engine_ok = 0;
    }
#endif
}

int game_audio_play_sfx(const char* path)
{
#if YUI_WITH_GAME_AUDIO
    ma_result r;
    if (!g_engine_ok || !path || !path[0]) {
        return 0;
    }
    r = ma_engine_play_sound(&g_engine, path, NULL);
    return r == MA_SUCCESS ? 1 : 0;
#else
    (void)path;
    return 0;
#endif
}

int game_audio_play_bgm(const char* path, int loop)
{
#if YUI_WITH_GAME_AUDIO
    ma_result r;
    if (!g_engine_ok || !path || !path[0]) {
        return 0;
    }
    if (g_bgm_active) {
        ma_sound_uninit(&g_bgm);
        g_bgm_active = 0;
    }
    r = ma_sound_init_from_file(&g_engine, path, 0, NULL, NULL, &g_bgm);
    if (r != MA_SUCCESS) {
        return 0;
    }
    ma_sound_set_looping(&g_bgm, loop ? MA_TRUE : MA_FALSE);
    r = ma_sound_start(&g_bgm);
    if (r != MA_SUCCESS) {
        ma_sound_uninit(&g_bgm);
        return 0;
    }
    g_bgm_active = 1;
    return 1;
#else
    (void)path;
    (void)loop;
    return 0;
#endif
}

void game_audio_stop_bgm(void)
{
#if YUI_WITH_GAME_AUDIO
    if (g_bgm_active) {
        ma_sound_stop(&g_bgm);
        ma_sound_uninit(&g_bgm);
        g_bgm_active = 0;
    }
#endif
}

#endif /* YUI_WITH_GAME */
