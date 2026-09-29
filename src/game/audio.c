#include "game.h"
#include "internal.h"

#if YUI_WITH_GAME

#include <stdio.h>
#include <string.h>

#ifndef YUI_WITH_GAME_AUDIO
#define YUI_WITH_GAME_AUDIO 1
#endif

// macOS 支持 miniaudio，iOS 不支持（需要 AVFoundation/Objective-C）
#if YUI_WITH_GAME_AUDIO && defined(__APPLE__)
#include <TargetConditionals.h>
#if TARGET_OS_IPHONE
#undef YUI_WITH_GAME_AUDIO
#define YUI_WITH_GAME_AUDIO 0
#endif
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

static void game_audio_sfx_ended(void* pUserData, ma_sound* pSound);
#endif

void game_audio_init(void)
{
#if YUI_WITH_GAME_AUDIO
    ma_result r;
    if (g_engine_ok) {
        return;
    }
    printf("Game audio: initializing miniaudio\n");
#if defined(__YIYIYA__)
    /* YiYiYa 无实际音频后端，用 noDevice 模式跳过设备初始化 */
    ma_engine_config engine_config = ma_engine_config_init();
    engine_config.noDevice = MA_TRUE;
    r = ma_engine_init(&engine_config, &g_engine);
#else
    r = ma_engine_init(NULL, &g_engine);
#endif
    printf("Game audio: ma_engine_init result = %d\n", (int)r);
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

#if YUI_WITH_GAME_AUDIO
static void game_audio_sfx_ended(void* pUserData, ma_sound* pSound)
{
    (void)pUserData;
    if (pSound) {
        ma_sound_uninit(pSound);
    }
}
#endif

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
        printf("Game audio: BGM stopped\n");
    }
#endif
}

void game_audio_set_volume(float volume)
{
#if YUI_WITH_GAME_AUDIO
    if (g_engine_ok) {
        ma_engine_set_volume(&g_engine, volume);
    }
#else
    (void)volume;
#endif
}
#endif
