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

#include "theme_manager.h"
#include "layer.h"
#include "layout.h"
#include "layer_lifecycle.h"
#include "cJSON.h"

#ifndef STDLIB_BUILD
#include "../../src/perf/perf.h"
#endif


#define MAX_TEXT 256
#define MAX_PATH 1024

// YUI Layer 类型定义（最小化定义）
typedef struct Layer Layer;
// 查找图层的函数指针类型
typedef Layer* (*FindLayerFunc)(Layer* root, const char* id);

// 外部变量和函数（从 js_module.c 和 layer.c 导入）
extern Layer* g_layer_root;
extern Layer* find_layer_by_id(Layer* root, const char* id);
extern Layer* parse_layer_from_string(const char* json_str, Layer* parent);
extern void destroy_layer(Layer* layer);
extern int js_module_load_from_json(cJSON* root_json, const char* json_file_path, int append);
extern char* js_module_read_file(const char* file_path);

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
#define JS_CLASS_COUNT (JS_CLASS_USER + 2)

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
            layer_set_text(layer, text);
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
            const char* layer_text = layer_get_text(layer);
            return JS_NewString(ctx, layer_text);
        }
    }

    return JS_UNDEFINED;
}

// 获取图层的属性值（通用）
static JSValue js_get_property(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    if (argc < 2) return JS_UNDEFINED;

    JSCStringBuf buf1, buf2;
    const char* layer_id = JS_ToCString(ctx, argv[0], &buf1);
    const char* property_name = JS_ToCString(ctx, argv[1], &buf2);

    if (layer_id && property_name && g_layer_root) {
        // 直接调用 layer_get_property_as_json，避免字符串中转
        extern cJSON* layer_get_property_as_json(Layer* layer, const char* key);
        Layer* layer = find_layer_by_id(g_layer_root, layer_id);
        
        JS_FreeCString(ctx, &buf1);
        JS_FreeCString(ctx, &buf2);
        
        if (!layer) {
            printf("JS: Layer '%s' not found\n", layer_id);
            return JS_UNDEFINED;
        }
        
        cJSON* json_value = layer_get_property_as_json(layer, property_name);
        if (!json_value) {
            printf("JS: Property '%s' not found for layer '%s'\n", property_name, layer_id);
            return JS_UNDEFINED;
        }
        
        JSValue result;
        if (cJSON_IsString(json_value)) {
            result = JS_NewString(ctx, json_value->valuestring);
        } else if (cJSON_IsNumber(json_value)) {
            result = JS_NewNumber(ctx, json_value->valuedouble);
        } else if (cJSON_IsBool(json_value)) {
            result = JS_NewBool(ctx, cJSON_IsTrue(json_value));
        } else if (cJSON_IsNull(json_value)) {
            result = JS_NULL;
        } else if (cJSON_IsArray(json_value) || cJSON_IsObject(json_value)) {
            // 数组和对象转为 JSON 字符串
            char* json_str = cJSON_PrintUnformatted(json_value);
            result = JS_NewString(ctx, json_str);
            free(json_str);
        } else {
            result = JS_UNDEFINED;
        }
        
        cJSON_Delete(json_value);
        return result;
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
            layer_hide(layer);
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
            layer_show(layer);
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

static int append_layer_child(Layer* parent, Layer* child) {
    if (!parent || !child) return -1;

    Layer** new_children = realloc(parent->children, sizeof(Layer*) * (parent->child_count + 1));
    if (!new_children) {
        return -2;
    }

    parent->children = new_children;
    parent->children[parent->child_count] = child;
    parent->child_count++;
    return 0;
}

// 从 JSON 字符串动态渲染到指定图层（可选第三参数 append：true 时追加子图层，不清空已有子节点）
static JSValue js_render_from_json(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    if (argc < 2) {
        return JS_ThrowTypeError(ctx, "Expected at least 2 arguments: layer_id, json_string, append?");
    }

    JSCStringBuf buf1, buf2;
    const char* layer_id = JS_ToCString(ctx, argv[0], &buf1);
    const char* json_str = JS_ToCString(ctx, argv[1], &buf2);

    if (!layer_id || !json_str) {
        return JS_ThrowTypeError(ctx, "Invalid arguments");
    }

    int append = 0;
    if (argc >= 3) {
        append = JS_ToBool(ctx, argv[2]);
    }

    const char* json_source_path = NULL;
    JSCStringBuf buf3;
    if (argc >= 4) {
        json_source_path = JS_ToCString(ctx, argv[3], &buf3);
    }

    printf("YUI: render_from_json called with layer_id='%s', append=%d\n", layer_id, append);

    if (!g_layer_root) {
        return JS_NewInt32(ctx, -4);
    }

    Layer* parent_layer = NULL;
    if (layer_id[0] == '\0') {
        parent_layer = g_layer_root;
    } else {
        parent_layer = find_layer_by_id(g_layer_root, layer_id);
        if (!parent_layer) {
            printf("YUI: WARN - Layer '%s' not found, fallback to root '%s'\n",
                   layer_id, g_layer_root->id);
            parent_layer = g_layer_root;
        }
    }

    if (!append && parent_layer->children) {
        for (int i = 0; i < parent_layer->child_count; i++) {
            if (parent_layer->children[i]) {
                layer_lifecycle_before_destroy(parent_layer->children[i]);
                destroy_layer(parent_layer->children[i]);
            }
        }
        free(parent_layer->children);
        parent_layer->children = NULL;
        parent_layer->child_count = 0;
    }

    Layer* new_layer = parse_layer_from_string(json_str, parent_layer);
    if (!new_layer) {
        printf("YUI: ERROR - Failed to parse JSON string\n");
        return JS_NewInt32(ctx, -3);
    }

    if (append) {
        if (append_layer_child(parent_layer, new_layer) != 0) {
            destroy_layer(new_layer);
            return JS_NewInt32(ctx, -2);
        }
    } else {
        parent_layer->children = malloc(sizeof(Layer*));
        if (!parent_layer->children) {
            printf("YUI: ERROR - Failed to allocate memory for children array\n");
            destroy_layer(new_layer);
            return JS_NewInt32(ctx, -2);
        }
        parent_layer->children[0] = new_layer;
        parent_layer->child_count = 1;
    }

    layout_layer(parent_layer);
    theme_manager_apply_to_tree(new_layer);

    cJSON* page_json = cJSON_Parse(json_str);
    if (page_json) {
        js_module_load_from_json(page_json, json_source_path, 1);
        cJSON_Delete(page_json);
    }

    printf("YUI: Successfully rendered JSON to layer '%s', new layer id: '%s'\n",
           layer_id, new_layer->id);
    return JS_NewInt32(ctx, 0);
}

static JSValue js_read_file(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    if (argc < 1) {
        return JS_ThrowTypeError(ctx, "Expected 1 argument: file_path");
    }

    JSCStringBuf buf;
    const char* file_path = JS_ToCString(ctx, argv[0], &buf);
    if (!file_path) {
        return JS_ThrowTypeError(ctx, "Invalid file path");
    }

    char* content = js_module_read_file(file_path);
    if (!content) {
        return JS_NULL;
    }

    JSValue result = JS_NewString(ctx, content);
    free(content);
    return result;
}

static JSValue js_resize_root(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    if (argc < 2) {
        return JS_ThrowTypeError(ctx, "Expected 2 arguments: width, height");
    }

    int width = 0;
    int height = 0;
    if (JS_ToInt32(ctx, &width, argv[0]) != 0 || JS_ToInt32(ctx, &height, argv[1]) != 0) {
        return JS_ThrowTypeError(ctx, "Invalid width or height");
    }

    extern int js_module_resize_root(int width, int height);
    return JS_NewInt32(ctx, js_module_resize_root(width, height));
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
    JS_CFUNC_DEF("log", 1, js_yui_log ),
    JS_CFUNC_DEF("setText", 1, js_set_text ),
    JS_CFUNC_DEF("getText", 1, js_get_text ),
    JS_CFUNC_DEF("getProperty", 2, js_get_property ),
    JS_CFUNC_DEF("setBgColor", 1, js_set_bg_color ),
    JS_CFUNC_DEF("hide", 1, js_hide ),
    JS_CFUNC_DEF("show", 1, js_show ),
    JS_CFUNC_DEF("renderFromJson", 3, js_render_from_json ),
    JS_CFUNC_DEF("readFile", 1, js_read_file ),
    JS_CFUNC_DEF("resizeRoot", 2, js_resize_root ),
    JS_CFUNC_DEF("call", 2, js_yui_call ),
    JS_CFUNC_DEF("update", 1, js_yui_update ),
    JS_CFUNC_DEF("dump", 1, js_yui_dump ),
    JS_CFUNC_DEF("themeLoad", 1, js_yui_themeLoad ),
    JS_CFUNC_DEF("themeSetCurrent", 1, js_yui_themeSetCurrent ),
    JS_CFUNC_DEF("themeUnload", 1, js_yui_themeUnload ),
    JS_CFUNC_DEF("themeApplyToTree", 0, js_yui_themeApplyToTree ),
    JS_CFUNC_DEF("inspect.enable", 0, js_yui_inspect_enable ),
    JS_CFUNC_DEF("inspect.disable", 0, js_yui_inspect_disable ),
    JS_CFUNC_DEF("inspect.setLayer", 2, js_yui_inspect_setLayer ),
    JS_CFUNC_DEF("inspect.setShowBounds", 1, js_yui_inspect_setShowBounds ),
    JS_CFUNC_DEF("inspect.setShowInfo", 1, js_yui_inspect_setShowInfo ),
    JS_CFUNC_DEF("perf.enable", 0, js_yui_perf_enable ),
    JS_CFUNC_DEF("perf.disable", 0, js_yui_perf_disable ),
    JS_CFUNC_DEF("perf.reset", 0, js_yui_perf_reset ),
    JS_CFUNC_DEF("perf.setOverlay", 1, js_yui_perf_setOverlay ),
    JS_CFUNC_DEF("perf.setTopN", 1, js_yui_perf_setTopN ),
    JS_CFUNC_DEF("perf.setLogInterval", 1, js_yui_perf_setLogInterval ),
    JS_CFUNC_DEF("perf.watch", 1, js_yui_perf_watch ),
    JS_CFUNC_DEF("perf.unwatch", 1, js_yui_perf_unwatch ),
    JS_CFUNC_DEF("perf.clearWatch", 0, js_yui_perf_clearWatch ),
    JS_CFUNC_DEF("perf.getFrameStats", 0, js_yui_perf_getFrameStats ),
    JS_CFUNC_DEF("perf.getLayerStats", 1, js_yui_perf_getLayerStats ),
    JS_CFUNC_DEF("setEvent", 3, js_yui_set_event ),
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

// JSON 增量更新 API
#ifndef STDLIB_BUILD
extern int yui_update(Layer* root, const char* update_json);
#endif

static int js_yui_dump_bool(JSContext *ctx, JSValue val)
{
    if (JS_IsBool(val)) {
        return JS_VALUE_GET_SPECIAL_VALUE(val) != 0;
    }
    if (JS_IsInt(val)) {
        return JS_VALUE_GET_INT(val) != 0;
    }
    return !JS_IsNull(val) && !JS_IsUndefined(val);
}

static int js_yui_dump_parse_flags(JSContext *ctx, JSValue opts)
{
    int flags = 0;
    JSValue v;

    if (!JS_IsObject(ctx, opts)) {
        return 0;
    }

    v = JS_GetPropertyStr(ctx, opts, "style");
    if (!JS_IsUndefined(v) && !JS_IsNull(v) && js_yui_dump_bool(ctx, v)) {
        flags |= LAYER_JSON_STYLE;
    }

    v = JS_GetPropertyStr(ctx, opts, "events");
    if (!JS_IsUndefined(v) && !JS_IsNull(v) && js_yui_dump_bool(ctx, v)) {
        flags |= LAYER_JSON_EVENTS;
    }

    return flags;
}

/* YUI.dump(id?, opts?) — dump layer tree as JSON string (also prints to stdout) */
static JSValue js_yui_dump(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
#ifdef STDLIB_BUILD
    (void)ctx; (void)this_val; (void)argc; (void)argv;
    return JS_UNDEFINED;
#else
    const char* layer_id = NULL;
    JSCStringBuf buf;
    int flags = 0;
    int argi = 0;
    Layer* layer;
    cJSON* json;
    char* printed;
    JSValue result;

    (void)this_val;

    if (argc >= 1 && JS_IsString(ctx, argv[0])) {
        layer_id = JS_ToCString(ctx, argv[0], &buf);
        argi = 1;
    } else if (argc >= 1 && JS_IsObject(ctx, argv[0])) {
        flags = js_yui_dump_parse_flags(ctx, argv[0]);
        argi = 1;
    }

    if (argi < argc && JS_IsObject(ctx, argv[argi])) {
        flags |= js_yui_dump_parse_flags(ctx, argv[argi]);
    }

    if (!g_layer_root) {
        return JS_NULL;
    }

    if (layer_id && layer_id[0]) {
        layer = find_layer_by_id(g_layer_root, layer_id);
        if (!layer) {
            printf("YUI.dump: layer '%s' not found\n", layer_id);
            return JS_NULL;
        }
    } else {
        layer = g_layer_root;
    }

    json = layer_to_json(layer, flags);
    if (!json) {
        return JS_NULL;
    }

    printed = cJSON_Print(json);
    cJSON_Delete(json);
    if (!printed) {
        return JS_NULL;
    }

    puts(printed);
    fflush(stdout);
    result = JS_NewString(ctx, printed);
    free(printed);
    return result;
#endif
}

// YUI.update() - JSON 增量更新
static JSValue js_yui_update(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
#ifdef STDLIB_BUILD
    return JS_UNDEFINED;
#else
    if (argc < 1) {
        printf("YUI.update: 需要至少一个参数\n");
        return JS_NewInt32(ctx, -1);
    }

    JSCStringBuf buf;
    const char* update_json = NULL;
    JSValue stringified = JS_UNDEFINED;

    if (JS_IsString(ctx, argv[0])) {
        update_json = JS_ToCString(ctx, argv[0], &buf);
    } else if (JS_IsObject(ctx, argv[0])) {
        JSValue global = JS_GetGlobalObject(ctx);
        JSValue json = JS_GetPropertyStr(ctx, global, "JSON");
        if (!JS_IsUndefined(json) && !JS_IsNull(json)) {
            JSValue fn = JS_GetPropertyStr(ctx, json, "stringify");
            if (!JS_IsUndefined(fn) && !JS_IsNull(fn)) {
                if (!JS_StackCheck(ctx, 3)) {
                    JS_PushArg(ctx, argv[0]);   // arg
                    JS_PushArg(ctx, fn);         // func
                    JS_PushArg(ctx, json);       // this
                    stringified = JS_Call(ctx, 1);
                }
                JS_FreeValue(ctx, fn);
            }
        }
        JS_FreeValue(ctx, json);
        JS_FreeValue(ctx, global);
        if (JS_IsException(stringified)) {
            JSValue exc = JS_GetException(ctx);
            fprintf(stderr, "YUI.update: JSON.stringify failed\n");
            JS_FreeValue(ctx, exc);
            JS_FreeValue(ctx, stringified);
            return JS_NewInt32(ctx, -1);
        }
        if (JS_IsString(ctx, stringified)) {
            update_json = JS_ToCString(ctx, stringified, &buf);
        }
    } else {
        printf("YUI.update: 参数必须是 JSON 字符串或对象/数组\n");
        return JS_NewInt32(ctx, -1);
    }

    int result = -1;
    if (update_json && g_layer_root) {
        result = yui_update(g_layer_root, update_json);
    } else {
        printf("YUI.update: 无效的参数或未初始化\n");
    }

    if (!JS_IsUndefined(stringified)) {
        JS_FreeValue(ctx, stringified);
    }
    return JS_NewInt32(ctx, result);
#endif
}

// 主题加载函数 - JS 接口
static JSValue js_yui_themeLoad(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    const char *theme_input = NULL;
    JSCStringBuf buf;
    
    // 检查参数
    if (argc < 1) {
        return JS_ThrowTypeError(ctx, "themeLoad requires 1 argument: theme_path or theme_json");
    }
    
    // 获取主题输入（可以是文件路径或JSON字符串）
    theme_input = JS_ToCString(ctx, argv[0], &buf);
    if (!theme_input) {
        return JS_ThrowTypeError(ctx, "Invalid theme input");
    }
    
    // 调用主题管理器的加载函数
    ThemeManager* manager = theme_manager_get_instance();
    Theme* theme = NULL;
    
    // 检查输入是文件路径还是JSON字符串
    // 如果是JSON字符串，应该以 '{' 或 '[' 开头
    size_t input_len = strlen(theme_input);
    if (input_len > 0 && (theme_input[0] == '{' || theme_input[0] == '[')) {
        // 是JSON字符串，从JSON加载
        theme = theme_manager_load_theme_from_json(theme_input);
    } else {
        // 是文件路径，从文件加载
        theme = theme_manager_load_theme(theme_input);
    }
    
    if (theme) {
        // 创建返回对象
        JSValue result = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, result, "success", JS_NewBool(1));
        JS_SetPropertyStr(ctx, result, "name", JS_NewString(ctx, theme->name));
        JS_SetPropertyStr(ctx, result, "version", JS_NewString(ctx, theme->version));
        return result;
    } else {
        // 创建返回对象
        JSValue result = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, result, "success", JS_NewBool( 0));
        JS_SetPropertyStr(ctx, result, "name", JS_NewString(ctx, ""));
        JS_SetPropertyStr(ctx, result, "version", JS_NewString(ctx, ""));
        JS_SetPropertyStr(ctx, result, "error", JS_NewString(ctx, "Failed to load theme"));
        return result;
    }
}

// 设置当前主题函数 - JS 接口
static JSValue js_yui_themeSetCurrent(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    const char *theme_name = NULL;
    JSCStringBuf buf;
    
    // 检查参数
    if (argc < 1) {
        return JS_ThrowTypeError(ctx, "themeSetCurrent requires 1 argument: theme_name");
    }
    
    // 获取主题名称
    theme_name = JS_ToCString(ctx, argv[0], &buf);
    if (!theme_name) {
        return JS_ThrowTypeError(ctx, "Invalid theme name");
    }
    
    // 调用主题管理器的设置函数
    int result = theme_manager_set_current(theme_name);
    
    return JS_NewBool( result);
}

// 卸载主题函数 - JS 接口
static JSValue js_yui_themeUnload(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    const char *theme_name = NULL;
    JSCStringBuf buf;
    
    // 检查参数
    if (argc < 1) {
        return JS_ThrowTypeError(ctx, "themeUnload requires 1 argument: theme_name");
    }
    
    // 获取主题名称
    theme_name = JS_ToCString(ctx, argv[0], &buf);
    if (!theme_name) {
        return JS_ThrowTypeError(ctx, "Invalid theme name");
    }
    
    // 调用主题管理器的卸载函数
    theme_manager_unload_theme(theme_name);
    
    return JS_NewBool( 1);
}

// 应用主题到图层树函数 - JS 接口
static JSValue js_yui_themeApplyToTree(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    // 调用主题管理器的应用函数
    ThemeManager* manager = theme_manager_get_instance();
    Theme* current = theme_manager_get_current();
    
    if (current && g_layer_root) {
        theme_manager_apply_to_tree(g_layer_root);
        return JS_NewBool( 1);
    }
    
    return JS_NewBool( 0);
}

// Inspect 启用函数 - JS 接口
static JSValue js_yui_inspect_enable(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    // 启用全局 inspect 模式
    extern int yui_inspect_mode_enabled;
    yui_inspect_mode_enabled = 1;
    printf("YUI Inspect: Enabled global inspect mode\n");
    return JS_NewBool( 1);
}

// Inspect 禁用函数 - JS 接口
static JSValue js_yui_inspect_disable(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    // 禁用全局 inspect 模式
    extern int yui_inspect_mode_enabled;
    yui_inspect_mode_enabled = 0;
    printf("YUI Inspect: Disabled global inspect mode\n");
    return JS_NewBool( 1);
}

// Inspect 设置图层函数 - JS 接口
static JSValue js_yui_inspect_setLayer(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    if (argc < 2) {
        return JS_ThrowTypeError(ctx, "inspect_setLayer requires 2 arguments: layer_id and enabled");
    }
    
    JSCStringBuf buf;
    const char* layer_id = JS_ToCString(ctx, argv[0], &buf);
    
    // mquickjs 不支持 JS_ToBool 和 JS_IsTruthy，直接检查值的类型
    int enabled = 0;
    if (JS_IsBool(argv[1])) {
        // 对于布尔值，检查特殊值的实际值
        enabled = JS_VALUE_GET_SPECIAL_VALUE(argv[1]) != 0;
    } else if (JS_IsInt(argv[1])) {
        enabled = JS_VALUE_GET_INT(argv[1]) != 0;
#ifdef JS_USE_SHORT_FLOAT
    } else if (JS_IsShortFloat(argv[1])) {
        // 短浮点值，需要获取实际的双精度值
        // 在 mquickjs 中，短浮点值存储为特殊值，需要转换为 double
        // 简化处理：短浮点非零即为真
        enabled = 1;
#endif
    } else {
        // 其他类型（字符串、对象等），非空即为真
        enabled = !JS_IsNull(argv[1]) && !JS_IsUndefined(argv[1]);
    }
    
    if (layer_id && g_layer_root) {
        Layer* layer = find_layer_by_id(g_layer_root, layer_id);
        if (layer) {
            layer->inspect_enabled = enabled;
            printf("YUI Inspect: Set layer '%s' inspect enabled = %d\n", layer_id, enabled);
            return JS_NewBool( 1);
        } else {
            printf("YUI Inspect: Layer '%s' not found\n", layer_id);
            return JS_NewBool( 0);
        }
    }
    
    return JS_NewBool( 0);
}

// Inspect 设置显示边界函数 - JS 接口
static JSValue js_yui_inspect_setShowBounds(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    if (argc < 1) {
        return JS_ThrowTypeError(ctx, "inspect_setShowBounds requires 1 argument: show_bounds");
    }
    
    // mquickjs 不支持 JS_ToBool 和 JS_IsTruthy，直接检查值的类型
    int show_bounds = 0;
    if (JS_IsBool(argv[0])) {
        // 对于布尔值，检查特殊值的实际值
        show_bounds = JS_VALUE_GET_SPECIAL_VALUE(argv[0]) != 0;
    } else if (JS_IsInt(argv[0])) {
        show_bounds = JS_VALUE_GET_INT(argv[0]) != 0;
#ifdef JS_USE_SHORT_FLOAT
    } else if (JS_IsShortFloat(argv[0])) {
        // 短浮点值，需要获取实际的双精度值
        // 在 mquickjs 中，短浮点值存储为特殊值，需要转换为 double
        // 简化处理：短浮点非零即为真
        show_bounds = 1;
#endif
    } else {
        // 其他类型（字符串、对象等），非空即为真
        show_bounds = !JS_IsNull(argv[0]) && !JS_IsUndefined(argv[0]);
    }
    
    extern int yui_inspect_show_bounds;
    yui_inspect_show_bounds = show_bounds;
    printf("YUI Inspect: Set show bounds = %d\n", show_bounds);
    return JS_NewBool( 1);
}

// Inspect 设置显示信息函数 - JS 接口
static JSValue js_yui_inspect_setShowInfo(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    if (argc < 1) {
        return JS_ThrowTypeError(ctx, "inspect_setShowInfo requires 1 argument: show_info");
    }
    
    // mquickjs 不支持 JS_ToBool 和 JS_IsTruthy，直接检查值的类型
    int show_info = 0;
    if (JS_IsBool(argv[0])) {
        // 对于布尔值，检查特殊值的实际值
        show_info = JS_VALUE_GET_SPECIAL_VALUE(argv[0]) != 0;
    } else if (JS_IsInt(argv[0])) {
        show_info = JS_VALUE_GET_INT(argv[0]) != 0;
#ifdef JS_USE_SHORT_FLOAT
    } else if (JS_IsShortFloat(argv[0])) {
        // 短浮点值，需要获取实际的双精度值
        // 在 mquickjs 中，短浮点值存储为特殊值，需要转换为 double
        // 简化处理：短浮点非零即为真
        show_info = 1;
#endif
    } else {
        // 其他类型（字符串、对象等），非空即为真
        show_info = !JS_IsNull(argv[0]) && !JS_IsUndefined(argv[0]);
    }
    
    extern int yui_inspect_show_info;
    yui_inspect_show_info = show_info;
    printf("YUI Inspect: Set show info = %d\n", show_info);
    return JS_NewBool( 1);
}

#ifndef STDLIB_BUILD
static int js_yui_parse_bool_arg(JSContext* ctx, JSValue val)
{
    if (JS_IsBool(val)) {
        return JS_VALUE_GET_SPECIAL_VALUE(val) != 0;
    }
    if (JS_IsInt(val)) {
        return JS_VALUE_GET_INT(val) != 0;
    }
    return !JS_IsNull(val) && !JS_IsUndefined(val);
}

static JSValue js_yui_perf_enable(JSContext* ctx, JSValue* this_val, int argc, JSValue* argv)
{
    perf_enable(1);
    printf("YUI Perf: enabled\n");
    return JS_NewBool(1);
}

static JSValue js_yui_perf_disable(JSContext* ctx, JSValue* this_val, int argc, JSValue* argv)
{
    perf_enable(0);
    printf("YUI Perf: disabled\n");
    return JS_NewBool(1);
}

static JSValue js_yui_perf_reset(JSContext* ctx, JSValue* this_val, int argc, JSValue* argv)
{
    perf_reset();
    return JS_NewBool(1);
}

static JSValue js_yui_perf_setOverlay(JSContext* ctx, JSValue* this_val, int argc, JSValue* argv)
{
    if (argc < 1) {
        return JS_ThrowTypeError(ctx, "perf.setOverlay requires 1 argument");
    }
    perf_set_overlay(js_yui_parse_bool_arg(ctx, argv[0]));
    return JS_NewBool(1);
}

static JSValue js_yui_perf_setTopN(JSContext* ctx, JSValue* this_val, int argc, JSValue* argv)
{
    if (argc < 1 || !JS_IsInt(argv[0])) {
        return JS_ThrowTypeError(ctx, "perf.setTopN requires an integer");
    }
    perf_set_top_n(JS_VALUE_GET_INT(argv[0]));
    return JS_NewBool(1);
}

static JSValue js_yui_perf_setLogInterval(JSContext* ctx, JSValue* this_val, int argc, JSValue* argv)
{
    if (argc < 1 || !JS_IsInt(argv[0])) {
        return JS_ThrowTypeError(ctx, "perf.setLogInterval requires an integer");
    }
    perf_set_log_interval(JS_VALUE_GET_INT(argv[0]));
    return JS_NewBool(1);
}

static JSValue js_yui_perf_watch(JSContext* ctx, JSValue* this_val, int argc, JSValue* argv)
{
    if (argc < 1) {
        return JS_ThrowTypeError(ctx, "perf.watch requires layer id");
    }
    JSCStringBuf buf;
    const char* layer_id = JS_ToCString(ctx, argv[0], &buf);
    int ok = layer_id ? perf_watch(layer_id) : 0;
    return JS_NewBool(ok);
}

static JSValue js_yui_perf_unwatch(JSContext* ctx, JSValue* this_val, int argc, JSValue* argv)
{
    if (argc < 1) {
        return JS_ThrowTypeError(ctx, "perf.unwatch requires layer id");
    }
    JSCStringBuf buf;
    const char* layer_id = JS_ToCString(ctx, argv[0], &buf);
    int ok = layer_id ? perf_unwatch(layer_id) : 0;
    return JS_NewBool(ok);
}

static JSValue js_yui_perf_clearWatch(JSContext* ctx, JSValue* this_val, int argc, JSValue* argv)
{
    perf_clear_watch();
    return JS_NewBool(1);
}

static JSValue js_yui_perf_getFrameStats(JSContext* ctx, JSValue* this_val, int argc, JSValue* argv)
{
    const PerfFrameStats* stats = perf_get_frame_stats();
    JSValue obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, obj, "fps", JS_NewFloat64(ctx, stats ? stats->fps : 0.0));
    JS_SetPropertyStr(ctx, obj, "frameIndex", JS_NewInt64(ctx, stats ? (int64_t)stats->frame_index : 0));
    JS_SetPropertyStr(ctx, obj, "frameMs", JS_NewFloat64(ctx, stats ? (double)stats->frame_ns / 1000000.0 : 0.0));
    JS_SetPropertyStr(ctx, obj, "renderMs", JS_NewFloat64(ctx, stats ? (double)stats->render_tree_ns / 1000000.0 : 0.0));
    JS_SetPropertyStr(ctx, obj, "layerCount", JS_NewInt32(ctx, stats ? (int)stats->layers_rendered : 0));
    return obj;
}

static PerfSortBy js_yui_perf_parse_sort(JSContext* ctx, JSValue val)
{
    if (!JS_IsString(ctx, val)) {
        return PERF_SORT_TIME;
    }
    JSCStringBuf buf;
    const char* sort = JS_ToCString(ctx, val, &buf);
    if (!sort) {
        return PERF_SORT_TIME;
    }
    if (strcmp(sort, "count") == 0) {
        return PERF_SORT_COUNT;
    }
    if (strcmp(sort, "name") == 0) {
        return PERF_SORT_NAME;
    }
    return PERF_SORT_TIME;
}

static JSValue js_yui_perf_getLayerStats(JSContext* ctx, JSValue* this_val, int argc, JSValue* argv)
{
    PerfSortBy sort_by = PERF_SORT_TIME;
    if (argc >= 1) {
        sort_by = js_yui_perf_parse_sort(ctx, argv[0]);
    }

    PerfLayerStats stats[32];
    int count = perf_get_layer_stats(stats, 32, sort_by);
    JSValue arr = JS_NewArray(ctx, count);

    for (int i = 0; i < count; i++) {
        JSValue item = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, item, "id", JS_NewString(ctx, stats[i].id ? stats[i].id : ""));
        JS_SetPropertyStr(ctx, item, "type", JS_NewInt32(ctx, stats[i].type));
        JS_SetPropertyStr(ctx, item, "renderCount", JS_NewInt32(ctx, (int)stats[i].render_count));
        JS_SetPropertyStr(ctx, item, "renderMs", JS_NewFloat64(ctx, (double)stats[i].render_ns / 1000000.0));
        JS_SetPropertyStr(ctx, item, "totalRenderMs", JS_NewFloat64(ctx, (double)stats[i].total_render_ns / 1000000.0));
        JS_SetPropertyStr(ctx, item, "maxRenderCount", JS_NewInt32(ctx, (int)stats[i].max_render_count));
        JS_SetPropertyStr(ctx, item, "framesSeen", JS_NewInt64(ctx, (int64_t)stats[i].total_frames_seen));

        char key[16];
        snprintf(key, sizeof(key), "%d", i);
        JS_SetPropertyStr(ctx, arr, key, item);
    }

    return arr;
}
#else
static JSValue js_yui_perf_enable(JSContext* ctx, JSValue* this_val, int argc, JSValue* argv) { return JS_UNDEFINED; }
static JSValue js_yui_perf_disable(JSContext* ctx, JSValue* this_val, int argc, JSValue* argv) { return JS_UNDEFINED; }
static JSValue js_yui_perf_reset(JSContext* ctx, JSValue* this_val, int argc, JSValue* argv) { return JS_UNDEFINED; }
static JSValue js_yui_perf_setOverlay(JSContext* ctx, JSValue* this_val, int argc, JSValue* argv) { return JS_UNDEFINED; }
static JSValue js_yui_perf_setTopN(JSContext* ctx, JSValue* this_val, int argc, JSValue* argv) { return JS_UNDEFINED; }
static JSValue js_yui_perf_setLogInterval(JSContext* ctx, JSValue* this_val, int argc, JSValue* argv) { return JS_UNDEFINED; }
static JSValue js_yui_perf_watch(JSContext* ctx, JSValue* this_val, int argc, JSValue* argv) { return JS_UNDEFINED; }
static JSValue js_yui_perf_unwatch(JSContext* ctx, JSValue* this_val, int argc, JSValue* argv) { return JS_UNDEFINED; }
static JSValue js_yui_perf_clearWatch(JSContext* ctx, JSValue* this_val, int argc, JSValue* argv) { return JS_UNDEFINED; }
static JSValue js_yui_perf_getFrameStats(JSContext* ctx, JSValue* this_val, int argc, JSValue* argv) { return JS_UNDEFINED; }
static JSValue js_yui_perf_getLayerStats(JSContext* ctx, JSValue* this_val, int argc, JSValue* argv) { return JS_UNDEFINED; }
#endif

extern int js_module_set_event(const char* layer_id, const char* event_name, const char* event_func_name);
// YUI.setEvent() - 设置图层事件回调
static JSValue js_yui_set_event(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    if (argc < 3) {
        return JS_ThrowTypeError(ctx, "setEvent requires 3 arguments: layer_id, event_name, callback");
    }
    
    JSCStringBuf buf1, buf2, buf3;
    const char* layer_id = JS_ToCString(ctx, argv[0], &buf1);
    const char* event_name = JS_ToCString(ctx, argv[1], &buf2);
    const char* func_name = JS_ToCString(ctx, argv[2], &buf3);
    
    if (!layer_id || !event_name || !func_name) {
        return JS_ThrowTypeError(ctx, "Invalid arguments");
    }
    
    // 调用公共实现
    int result = js_module_set_event(layer_id, event_name, func_name);
    
    return JS_NewBool(result == 0 ? 1 : 0);
}

static const JSClassDef js_yui_class =
JS_CLASS_DEF("YUI", 1, js_yui_constructor, JS_CLASS_YUI, js_yui, js_yui_proto, NULL, js_yui_finalizer);

// 注册 YUI API 到 JS（导出函数，不使用 static）
void js_module_register_yui_api(JSContext* ctx) {
    if (!ctx) return;
    
    // 获取全局对象
    JSValue global_obj = JS_GetGlobalObject(ctx);
    
    // YUI 对象应该已经通过 js_yuistdlib 数据结构自动创建
    // 这里只需要确保它可用
    
    // 获取 YUI 对象（如果已经存在）
    JSValue yui_obj = JS_GetPropertyStr(ctx, global_obj, "YUI");
    
    JS_SetLogFunc(ctx, js_log_func);

    // 检查 YUI 对象是否存在
    if (JS_IsUndefined(yui_obj) || JS_IsNull(yui_obj)) {
        printf("JS(YUI): YUI object not found, will be created by js_yuistdlib\n");
    } else {
        printf("JS(YUI): YUI object exists, functions should be available\n");
    }
    
    printf("JS(YUI): YUI API registration completed\n");
}

