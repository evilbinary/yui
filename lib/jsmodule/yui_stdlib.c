#include <math.h>
#include <stdio.h>
#include <string.h>
#include "mquickjs_build.h"


#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <time.h>
#include <sys/time.h>
#include <math.h>
#include <fcntl.h>

#include "cutils.h"
#include "mquickjs.h"

#include "layer.h"

// YUI Layer 类型定义（最小化定义）
#define MAX_TEXT 256
#define MAX_PATH 1024

typedef struct Layer Layer;

// 查找图层的函数指针类型
typedef Layer* (*FindLayerFunc)(Layer* root, const char* id);

// 外部变量和函数（从 js_module.c 和 layer.c 导入）
extern Layer* g_layer_root;
extern Layer* find_layer_by_id(Layer* root, const char* id);

// 颜色结构体定义

// 辅助函数
static int hex_to_int(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}

#define JS_CLASS_YUI (JS_CLASS_USER + 0)

/* total number of classes */
#define JS_CLASS_COUNT (JS_CLASS_USER + 1)

#define JS_CFUNCTION_rectangle_closure_test (JS_CFUNCTION_USER + 0)

typedef struct {
    int x;
    int y;
} YuiData;

static JSValue js_yui_constructor(JSContext *ctx, JSValue *this_val, int argc,
                                        JSValue *argv)
{
    JSValue obj;
    YuiData *d;

    if (!(argc & FRAME_CF_CTOR))
        return JS_ThrowTypeError(ctx, "must be called with new");
    argc &= ~FRAME_CF_CTOR;
    obj = JS_NewObjectClassUser(ctx, JS_CLASS_YUI);
    d = malloc(sizeof(*d));
    JS_SetOpaque(ctx, obj, d);
    if (JS_ToInt32(ctx, &d->x, argv[0]))
        return JS_EXCEPTION;
    if (JS_ToInt32(ctx, &d->y, argv[1]))
        return JS_EXCEPTION;
    return obj;
}

static void js_yui_finalizer(JSContext *ctx, void *opaque)
{
    YuiData *d = opaque;
    free(d);
}


/* example to call a JS function. parameters: function to call, parameter */
static JSValue js_yui_call(JSContext *ctx, JSValue *this_val, int argc,
                                 JSValue *argv)
{
    if (JS_StackCheck(ctx, 3))
        return JS_EXCEPTION;
    JS_PushArg(ctx, argv[1]); /* parameter */
    JS_PushArg(ctx, argv[0]); /* func name */
    JS_PushArg(ctx, JS_NULL); /* this */
    return JS_Call(ctx, 1); /* single parameter */
}

static JSValue js_print(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    int i;
    JSValue v;
    
    for(i = 0; i < argc; i++) {
        if (i != 0)
            putchar(' ');
        v = argv[i];
        if (JS_IsString(ctx, v)) {
            JSCStringBuf buf;
            const char *str;
            size_t len;
            str = JS_ToCStringLen(ctx, &len, v, &buf);
            fwrite(str, 1, len, stdout);
        } else {
            JS_PrintValueF(ctx, argv[i], JS_DUMP_LONG);
        }
    }
    putchar('\n');
    return JS_UNDEFINED;
}

#if defined(__linux__) || defined(__APPLE__)
static int64_t get_time_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (ts.tv_nsec / 1000000);
}
#else
static int64_t get_time_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000 + (tv.tv_usec / 1000);
}
#endif

static JSValue js_date_now(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return JS_NewInt64(ctx, (int64_t)tv.tv_sec * 1000 + (tv.tv_usec / 1000));
}

static JSValue js_performance_now(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    return JS_NewInt64(ctx, get_time_ms());
}

// ====================== YUI API 函数 ======================

// 设置文本
static JSValue js_set_text(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    if (argc < 2) return JS_UNDEFINED;

    JSCStringBuf buf1, buf2;
    const char* layer_id = JS_ToCString(ctx, argv[0], &buf1);
    const char* text = JS_ToCString(ctx, argv[1], &buf2);

    if (layer_id && text && g_layer_root ) {
        Layer* layer = find_layer_by_id(g_layer_root, layer_id);
        if (layer) {
            strcpy(layer->text, text);
            printf("YUI: Set text for layer '%s': %s\n", layer_id, text);
            fflush(stdout);

        }
    }

    return JS_UNDEFINED;
}

// 获取文本
static JSValue js_get_text(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    if (argc < 1) return JS_UNDEFINED;

    JSCStringBuf buf;
    const char* layer_id = JS_ToCString(ctx, argv[0], &buf);

    if (layer_id && g_layer_root ) {
        Layer* layer = find_layer_by_id(g_layer_root, layer_id);
        if (layer) {
            return JS_NewString(ctx, layer->text);
        }
    }

    return JS_UNDEFINED;
}

// 设置背景颜色
static JSValue js_set_bg_color(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    if (argc < 2) return JS_UNDEFINED;

    JSCStringBuf buf1, buf2;
    const char* layer_id = JS_ToCString(ctx, argv[0], &buf1);
    const char* color_hex = JS_ToCString(ctx, argv[1], &buf2);

    if (layer_id && color_hex && g_layer_root ) {
        Layer* layer = find_layer_by_id(g_layer_root, layer_id);
        if (layer) {
            // 解析十六进制颜色 #RRGGBB
            if (strlen(color_hex) == 7 && color_hex[0] == '#') {
                layer->bg_color.r = (hex_to_int(color_hex[1]) * 16 + hex_to_int(color_hex[2]));
                layer->bg_color.g = (hex_to_int(color_hex[3]) * 16 + hex_to_int(color_hex[4]));
                layer->bg_color.b = (hex_to_int(color_hex[5]) * 16 + hex_to_int(color_hex[6]));
                layer->bg_color.a = 255;
                printf("YUI: Set bg_color for layer '%s': %s\n", layer_id, color_hex);
            }
        }
    }

    return JS_UNDEFINED;
}

// 隐藏图层
static JSValue js_hide(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    if (argc < 1) return JS_UNDEFINED;

    JSCStringBuf buf;
    const char* layer_id = JS_ToCString(ctx, argv[0], &buf);

    if (layer_id && g_layer_root ) {
        Layer* layer = find_layer_by_id(g_layer_root, layer_id);
        if (layer) {
            layer->visible = 0;
            printf("YUI: Hide layer '%s'\n", layer_id);
        }
    }

    return JS_UNDEFINED;
}

// 显示图层
static JSValue js_show(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    if (argc < 1) return JS_UNDEFINED;

    JSCStringBuf buf;
    const char* layer_id = JS_ToCString(ctx, argv[0], &buf);

    if (layer_id && g_layer_root ) {
        Layer* layer = find_layer_by_id(g_layer_root, layer_id);
        if (layer) {
            layer->visible = 1;
            printf("YUI: Show layer '%s'\n", layer_id);
        }
    }

    return JS_UNDEFINED;
}

// 打印日志（YUI 版本）
static JSValue js_yui_log(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    JSCStringBuf buf;
    for (int i = 0; i < argc; i++) {
        if (i != 0) printf(" ");
        const char* str = JS_ToCString(ctx, argv[i], &buf);
        if (str) {
            printf("%s", str);
        }
    }
    printf("\n");
    return JS_UNDEFINED;
}



extern JSValue js_setTimeout(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
extern JSValue js_clearTimeout(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
extern JSValue js_gc(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);
extern JSValue js_load(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv);

static void js_log_func(void *opaque, const void *buf, size_t buf_len)
{
    fwrite(buf, 1, buf_len, stdout);
}

static uint8_t *load_file(const char *filename, int *plen)
{
    FILE *f;
    uint8_t *buf;
    int buf_len;

    f = fopen(filename, "rb");
    if (!f) {
        perror(filename);
        exit(1);
    }
    fseek(f, 0, SEEK_END);
    buf_len = ftell(f);
    fseek(f, 0, SEEK_SET);
    buf = malloc(buf_len + 1);
    fread(buf, 1, buf_len, f);
    buf[buf_len] = '\0';
    fclose(f);
    if (plen)
        *plen = buf_len;
    return buf;
}


/* simple class example */

static const JSPropDef js_yui_proto[] = {
    JS_CGETSET_DEF("x", js_yui_get_x, NULL ),
    JS_CGETSET_DEF("y", js_yui_get_y, NULL ),
    JS_PROP_END,
};

static const JSPropDef js_yui[] = {
    JS_CFUNC_DEF("log", 1, js_log ),
    JS_CFUNC_DEF("setText", 1, js_set_text ),
    JS_CFUNC_DEF("getText", 1, js_get_text ),
    JS_CFUNC_DEF("setBgColor", 1, js_set_bg_color ),
    JS_CFUNC_DEF("hide", 1, js_hide ),
    JS_CFUNC_DEF("show", 1, js_show ),
    JS_CFUNC_DEF("call", 2, js_yui_call ),
    JS_PROP_END,
};


static JSValue js_yui_get_x(JSContext *ctx, JSValue *this_val, int argc,
    JSValue *argv)
{
    YuiData *d;
    int class_id = JS_GetClassID(ctx, *this_val);
    if (class_id != JS_CLASS_YUI )
    return JS_ThrowTypeError(ctx, "expecting Yui class");
    d = JS_GetOpaque(ctx, *this_val);
    return JS_NewInt32(ctx, d->x);
}

static JSValue js_yui_get_y(JSContext *ctx, JSValue *this_val, int argc,
    JSValue *argv)
{
    YuiData *d;
    int class_id = JS_GetClassID(ctx, *this_val);
    if (class_id != JS_CLASS_YUI)
    return JS_ThrowTypeError(ctx, "expecting Yui class");
    d = JS_GetOpaque(ctx, *this_val);
    return JS_NewInt32(ctx, d->y);
}

/* C closure test */
static JSValue js_yui_getClosure(JSContext *ctx, JSValue *this_val, int argc,
    JSValue *argv)
{
    printf("js_yui_getClosure\n");
return JS_NewCFunctionParams(ctx, JS_CFUNCTION_rectangle_closure_test, argv[0]);
}

/* C closure test */
static JSValue js_yui_test(JSContext *ctx, JSValue *this_val, int argc,
    JSValue *argv)
{
    printf("js_yui_test\n");
return JS_NewCFunctionParams(ctx, JS_CFUNCTION_rectangle_closure_test, argv[0]);
}


static const JSClassDef js_yui_class =
JS_CLASS_DEF("YUI", 2, js_yui_constructor, JS_CLASS_YUI, js_yui, js_yui_proto, NULL, js_yui_finalizer);

static const JSClassDef js_yui2_class =
JS_CLASS_DEF("Yui", 2, js_yui_constructor, JS_CLASS_YUI, js_yui, js_yui_proto, NULL, js_yui_finalizer);


#include "yui_stdlib.h"



