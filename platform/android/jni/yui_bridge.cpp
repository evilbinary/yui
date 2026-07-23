#include <jni.h>
#include <unistd.h>
#include <android/native_window_jni.h>
#include <android/log.h>
#include <cstring>

#include "yui_boot.h"

#define YUI_LOG_TAG "YuiNative"
#define YUI_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, YUI_LOG_TAG, __VA_ARGS__)

static int g_initialized = 0;
static JavaVM* g_jvm = nullptr;
static jobject g_view_ref = nullptr;

static JNIEnv* yui_get_env(void) {
    JNIEnv* env = nullptr;
    if (!g_jvm) {
        return nullptr;
    }
    if (g_jvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        if (g_jvm->AttachCurrentThread(&env, nullptr) != JNI_OK) {
            return nullptr;
        }
    }
    return env;
}

static jobject yui_get_context(JNIEnv* env) {
    jclass view_class;
    jmethodID get_context;

    if (!env || !g_view_ref) {
        return nullptr;
    }
    view_class = env->GetObjectClass(g_view_ref);
    get_context = env->GetMethodID(view_class, "getContext", "()Landroid/content/Context;");
    return env->CallObjectMethod(g_view_ref, get_context);
}

extern "C" char* android_clipboard_get_text(void) {
    JNIEnv* env = yui_get_env();
    jobject context;
    jclass context_class;
    jmethodID get_service;
    jstring service_name;
    jobject clipboard_service;
    jclass clipboard_class;
    jmethodID has_primary_clip;
    jmethodID get_primary_clip;
    jobject clip_data;
    jclass clip_data_class;
    jmethodID get_item_count;
    jmethodID get_item_at;
    jobject clip_item;
    jclass clip_item_class;
    jmethodID coerce_to_text;
    jstring text_obj;
    const char* text_utf;
    char* result;

    if (!env) {
        return nullptr;
    }
    context = yui_get_context(env);
    if (!context) {
        return nullptr;
    }

    context_class = env->GetObjectClass(context);
    get_service = env->GetMethodID(context_class, "getSystemService",
                                   "(Ljava/lang/String;)Ljava/lang/Object;");
    service_name = env->NewStringUTF("clipboard");
    clipboard_service = env->CallObjectMethod(context, get_service, service_name);
    env->DeleteLocalRef(service_name);
    if (!clipboard_service) {
        return nullptr;
    }

    clipboard_class = env->GetObjectClass(clipboard_service);
    has_primary_clip = env->GetMethodID(clipboard_class, "hasPrimaryClip", "()Z");
    if (!env->CallBooleanMethod(clipboard_service, has_primary_clip)) {
        return nullptr;
    }

    get_primary_clip = env->GetMethodID(clipboard_class, "getPrimaryClip",
                                        "()Landroid/content/ClipData;");
    clip_data = env->CallObjectMethod(clipboard_service, get_primary_clip);
    if (!clip_data) {
        return nullptr;
    }

    clip_data_class = env->GetObjectClass(clip_data);
    get_item_count = env->GetMethodID(clip_data_class, "getItemCount", "()I");
    if (env->CallIntMethod(clip_data, get_item_count) <= 0) {
        return nullptr;
    }

    get_item_at = env->GetMethodID(clip_data_class, "getItemAt",
                                   "(I)Landroid/content/ClipData$Item;");
    clip_item = env->CallObjectMethod(clip_data, get_item_at, 0);
    if (!clip_item) {
        return nullptr;
    }

    clip_item_class = env->GetObjectClass(clip_item);
    coerce_to_text = env->GetMethodID(clip_item_class, "coerceToText",
                                      "(Landroid/content/Context;)Ljava/lang/CharSequence;");
    text_obj = (jstring)env->CallObjectMethod(clip_item, coerce_to_text, context);
    if (!text_obj) {
        return nullptr;
    }

    text_utf = env->GetStringUTFChars(text_obj, nullptr);
    if (!text_utf) {
        return nullptr;
    }
    result = strdup(text_utf);
    env->ReleaseStringUTFChars(text_obj, text_utf);
    return result;
}

extern "C" void android_clipboard_set_text(const char* text) {
    JNIEnv* env = yui_get_env();
    jobject context;
    jclass context_class;
    jmethodID get_service;
    jstring service_name;
    jobject clipboard_service;
    jclass clipboard_class;
    jmethodID set_primary_clip;
    jstring text_obj;
    jobject clip_data;

    if (!env || !text) {
        return;
    }
    context = yui_get_context(env);
    if (!context) {
        return;
    }

    context_class = env->GetObjectClass(context);
    get_service = env->GetMethodID(context_class, "getSystemService",
                                   "(Ljava/lang/String;)Ljava/lang/Object;");
    service_name = env->NewStringUTF("clipboard");
    clipboard_service = env->CallObjectMethod(context, get_service, service_name);
    env->DeleteLocalRef(service_name);
    if (!clipboard_service) {
        return;
    }

    clipboard_class = env->FindClass("android/content/ClipData");
    {
        jmethodID new_plain_text = env->GetStaticMethodID(
            clipboard_class, "newPlainText",
            "(Ljava/lang/CharSequence;Ljava/lang/CharSequence;)Landroid/content/ClipData;");
        text_obj = env->NewStringUTF(text);
        clip_data = env->CallStaticObjectMethod(clipboard_class, new_plain_text, nullptr, text_obj);
        env->DeleteLocalRef(text_obj);
    }
    if (!clip_data) {
        return;
    }

    set_primary_clip = env->GetMethodID(env->GetObjectClass(clipboard_service),
                                        "setPrimaryClip", "(Landroid/content/ClipData;)V");
    env->CallVoidMethod(clipboard_service, set_primary_clip, clip_data);
}

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    (void)reserved;
    g_jvm = vm;
    return JNI_VERSION_1_6;
}

extern "C" JNIEXPORT void JNICALL
Java_com_yui_YuiView_nativeInit(JNIEnv* env, jobject thiz, jobject surface,
                                jstring jsonPath, jstring assetsPath,
                                jstring dataRoot, jfloat density) {
    ANativeWindow* window = nullptr;
    const char* json_c = nullptr;
    const char* assets_c = nullptr;
    const char* data_root_c = nullptr;

    (void)thiz;

    if (g_initialized) {
        yui_shutdown();
        g_initialized = 0;
    }
    if (g_view_ref) {
        env->DeleteGlobalRef(g_view_ref);
        g_view_ref = nullptr;
    }
    g_view_ref = env->NewGlobalRef(thiz);

    window = ANativeWindow_fromSurface(env, surface);
    if (!window) {
        YUI_LOGE("ANativeWindow_fromSurface failed");
        return;
    }

    yui_set_native_surface(window);
    yui_set_density(density);

    if (jsonPath) {
        json_c = env->GetStringUTFChars(jsonPath, nullptr);
    }
    if (assetsPath) {
        assets_c = env->GetStringUTFChars(assetsPath, nullptr);
    }
    if (dataRoot) {
        data_root_c = env->GetStringUTFChars(dataRoot, nullptr);
    }

    if (data_root_c && data_root_c[0]) {
        if (chdir(data_root_c) != 0) {
            YUI_LOGE("chdir to %s failed", data_root_c);
        }
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
    if (data_root_c) {
        env->ReleaseStringUTFChars(dataRoot, data_root_c);
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
Java_com_yui_YuiView_nativeSetAppFocused(JNIEnv* env, jobject thiz, jint focused) {
    (void)env;
    (void)thiz;
    if (g_initialized) {
        yui_set_app_focused(focused ? 1 : 0);
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_yui_YuiView_nativeOnTouch(JNIEnv* env, jobject thiz, jint pointerId,
                                   jfloat x, jfloat y, jint phase) {
    (void)env;
    (void)thiz;
    if (g_initialized) {
        yui_on_touch((int)pointerId, (int)x, (int)y, (int)phase);
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
    if (g_view_ref) {
        env->DeleteGlobalRef(g_view_ref);
        g_view_ref = nullptr;
    }
}
