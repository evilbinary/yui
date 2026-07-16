#include <jni.h>
#include <android/native_window_jni.h>
#include <android/log.h>
#include <string>

#include "yui_boot.h"

#define YUI_LOG_TAG "YuiNative"
#define YUI_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, YUI_LOG_TAG, __VA_ARGS__)

static int g_initialized = 0;

extern "C" JNIEXPORT void JNICALL
Java_com_yui_YuiView_nativeInit(JNIEnv* env, jobject thiz, jobject surface,
                                jstring jsonPath, jstring assetsPath) {
    ANativeWindow* window = nullptr;
    const char* json_c = nullptr;
    const char* assets_c = nullptr;

    (void)thiz;

    if (g_initialized) {
        yui_shutdown();
        g_initialized = 0;
    }

    window = ANativeWindow_fromSurface(env, surface);
    if (!window) {
        YUI_LOGE("ANativeWindow_fromSurface failed");
        return;
    }

    yui_set_native_surface(window);

    if (jsonPath) {
        json_c = env->GetStringUTFChars(jsonPath, nullptr);
    }
    if (assetsPath) {
        assets_c = env->GetStringUTFChars(assetsPath, nullptr);
    }

    if (!json_c || yui_init(json_c, assets_c) != 0) {
        YUI_LOGE("yui_init failed for %s", json_c ? json_c : "(null)");
    } else {
        g_initialized = 1;
    }

    if (json_c) {
        env->ReleaseStringUTFChars(jsonPath, json_c);
    }
    if (assets_c) {
        env->ReleaseStringUTFChars(assetsPath, assets_c);
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_yui_YuiView_nativeResize(JNIEnv* env, jobject thiz, jint width, jint height) {
    (void)env;
    (void)thiz;
    if (g_initialized) {
        yui_resize((int)width, (int)height);
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_yui_YuiView_nativeTick(JNIEnv* env, jobject thiz) {
    (void)env;
    (void)thiz;
    if (g_initialized) {
        yui_tick();
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_yui_YuiView_nativeShutdown(JNIEnv* env, jobject thiz) {
    (void)env;
    (void)thiz;
    if (g_initialized) {
        yui_shutdown();
        g_initialized = 0;
    }
}
