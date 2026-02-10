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
            layer_set_text(layer, text); // 修改为传递 layer 和 text
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

    if (layer_id && property_name) {
        // 调用 js_common.c 中的函数
        extern const char* js_module_get_property_value(const char* layer_id, const char* property_name);
        const char* value = js_module_get_property_value(layer_id, property_name);
        
        if (value) {
            JSValue result = JS_NewString(ctx, value);
            // 释放返回的字符串
            free((void*)value);
            return result;
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

// 从 JSON 字符串动态渲染到指定图层
static JSValue js_render_from_json(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    if (argc < 2) {
        return JS_ThrowTypeError(ctx, "Expected 2 arguments: layer_id and json_string");
    }

    JSCStringBuf buf1, buf2;
    const char* layer_id = JS_ToCString(ctx, argv[0], &buf1);
    const char* json_str = JS_ToCString(ctx, argv[1], &buf2);

    if (!layer_id || !json_str) {
        return JS_ThrowTypeError(ctx, "Invalid arguments");
    }

    printf("YUI: render_from_json called with layer_id='%s'\n", layer_id);

    if (g_layer_root) {
        // 查找目标图层
        Layer* parent_layer = find_layer_by_id(g_layer_root, layer_id);
        if (!parent_layer) {
            printf("YUI: ERROR - Layer '%s' not found\n", layer_id);
            return JS_NewInt32(ctx, -1);
        }

        printf("YUI: Found parent layer '%s'\n", layer_id);

        // 清除父图层的所有子图层
        if (parent_layer->children) {
            for (int i = 0; i < parent_layer->child_count; i++) {
                if (parent_layer->children[i]) {
                    destroy_layer(parent_layer->children[i]);
                }
            }
            free(parent_layer->children);
            parent_layer->children = NULL;
        }
        parent_layer->child_count = 0;

        // 从 JSON 字符串创建新图层
        Layer* new_layer = parse_layer_from_string(json_str, parent_layer);

        if (new_layer) {
            // 为子图层数组分配空间（初始分配1个，可以根据需要扩展）
            parent_layer->children = malloc(sizeof(Layer*));
            if (!parent_layer->children) {
                printf("YUI: ERROR - Failed to allocate memory for children array\n");
                destroy_layer(new_layer);
                return JS_NewInt32(ctx, -2);
            }

            parent_layer->children[0] = new_layer;
            parent_layer->child_count = 1;
            layout_layer(parent_layer);
             // 为新创建的图层加载字体
            printf("JS(mqjs): Loading fonts for new layer\n");
            load_all_fonts(new_layer);
            printf("JS(mqjs): Fonts loaded successfully\n");

            printf("YUI: Successfully rendered JSON to layer '%s', new layer id: '%s'\n",
                   layer_id, new_layer->id);
            return JS_NewInt32(ctx, 0);
        } else {
            printf("YUI: ERROR - Failed to parse JSON string\n");
            return JS_NewInt32(ctx, -3);
        }
    }

    printf("YUI: ERROR - g_layer_root is NULL\n");
    return JS_NewInt32(ctx, -4);
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
    JS_CFUNC_DEF("renderFromJson", 2, js_render_from_json ),
    JS_CFUNC_DEF("call", 2, js_yui_call ),
    JS_CFUNC_DEF("update", 1, js_yui_update ),
    JS_CFUNC_DEF("themeLoad", 1, js_yui_themeLoad ),
    JS_CFUNC_DEF("themeSetCurrent", 1, js_yui_themeSetCurrent ),
    JS_CFUNC_DEF("themeUnload", 1, js_yui_themeUnload ),
    JS_CFUNC_DEF("themeApplyToTree", 0, js_yui_themeApplyToTree ),
    JS_CFUNC_DEF("inspect.enable", 0, js_yui_inspect_enable ),
    JS_CFUNC_DEF("inspect.disable", 0, js_yui_inspect_disable ),
    JS_CFUNC_DEF("inspect.setLayer", 2, js_yui_inspect_setLayer ),
    JS_CFUNC_DEF("inspect.setShowBounds", 1, js_yui_inspect_setShowBounds ),
    JS_CFUNC_DEF("inspect.setShowInfo", 1, js_yui_inspect_setShowInfo ),
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
    int need_free = 0;
    
    // mquickjs 只支持字符串参数
    // 如果 JS 代码需要传入对象，应该在 JS 层调用 JSON.stringify(obj)
    if (JS_IsString(ctx, argv[0])) {
        update_json = JS_ToCString(ctx, argv[0], &buf);
    } else {
        printf("YUI.update: 参数必须是 JSON 字符串。如果要传对象，请在 JS 代码中使用 JSON.stringify(obj)\n");
        return JS_NewInt32(ctx, -1);
    }

    if (update_json && g_layer_root) {
        int result = yui_update(g_layer_root, update_json);
        return JS_NewInt32(ctx, result);
    }

    printf("YUI.update: 无效的参数或未初始化\n");
    return JS_NewInt32(ctx, -1);
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

