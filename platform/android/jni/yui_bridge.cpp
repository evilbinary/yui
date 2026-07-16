#include <jni.h>
#include "yui_boot.h"

JNIEXPORT void JNICALL
Java_com_yui_YuiView_nativeInit(JNIEnv* env, jobject thiz, jobject surface, jstring assetsPath) {
    (void)env;
    (void)thiz;
    (void)surface;
    (void)assetsPath;
    /* TODO: ANativeWindow_fromSurface → yui_set_native_surface → yui_init */
}

JNIEXPORT void JNICALL
Java_com_yui_YuiView_nativeTick(JNIEnv* env, jobject thiz) {
    (void)env;
    (void)thiz;
    yui_tick();
}

JNIEXPORT void JNICALL
Java_com_yui_YuiView_nativeShutdown(JNIEnv* env, jobject thiz) {
    (void)env;
    (void)thiz;
    yui_shutdown();
}
