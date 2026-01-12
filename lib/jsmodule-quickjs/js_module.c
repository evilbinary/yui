#include "lib/jsmodule/js_module.h"
#include "../../lib/quickjs/quickjs.h"
#include "../../src/layer.h"
#include "../../src/layout.h"
#include "../../src/render.h"
#include "js_socket.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

// Layer 结构的最小定义
#define MAX_TEXT 256

/* ====================== 全局变量 ====================== */

static JSRuntime* g_js_rt = NULL;
static JSContext* g_js_ctx = NULL;
extern struct Layer* g_layer_root;

/* ====================== QuickJS 原生函数 ====================== */

// 设置文本
static JSValue js_set_text(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    if (argc < 2) return JS_UNDEFINED;

    size_t len1, len2;
    const char* layer_id = JS_ToCStringLen(ctx, &len1, argv[0]);
    const char* text = JS_ToCStringLen(ctx, &len2, argv[1]);

    if (layer_id && text && g_layer_root) {
        struct Layer* layer = find_layer_by_id(g_layer_root, layer_id);
        if (layer) {
            layer_set_text(layer, text); //
        
            printf("JS(QuickJS): Set text for layer '%s': %s\n", layer_id, text);
        }
    }

    JS_FreeCString(ctx, layer_id);
    JS_FreeCString(ctx, text);
    return JS_UNDEFINED;
}

// 获取文本
static JSValue js_get_text(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    if (argc < 1) return JS_UNDEFINED;

    size_t len;
    const char* layer_id = JS_ToCStringLen(ctx, &len, argv[0]);

    if (layer_id && g_layer_root) {
        struct Layer* layer = find_layer_by_id(g_layer_root, layer_id);
        if (layer) {
            JS_FreeCString(ctx, layer_id);
            const char* layer_text = layer_get_text(layer);
            return JS_NewString(ctx, layer_text);
        }
    }

    JS_FreeCString(ctx, layer_id);
    return JS_UNDEFINED;
}

// 设置背景颜色
static JSValue js_set_bg_color(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    if (argc < 2) return JS_UNDEFINED;

    size_t len1, len2;
    const char* layer_id = JS_ToCStringLen(ctx, &len1, argv[0]);
    const char* color_hex = JS_ToCStringLen(ctx, &len2, argv[1]);

    if (layer_id && color_hex && g_layer_root) {
        struct Layer* layer = find_layer_by_id(g_layer_root, layer_id);
        if (layer) {
            // 解析十六进制颜色 #RRGGBB
            if (strlen(color_hex) == 7 && color_hex[0] == '#') {
                int r = 0, g = 0, b = 0;
                sscanf(color_hex, "#%02x%02x%02x", &r, &g, &b);
                layer->bg_color.r = r;
                layer->bg_color.g = g;
                layer->bg_color.b = b;
                layer->bg_color.a = 255;
                printf("JS(QuickJS): Set bg_color for layer '%s': %s\n", layer_id, color_hex);
            }
        }
    }

    JS_FreeCString(ctx, layer_id);
    JS_FreeCString(ctx, color_hex);
    return JS_UNDEFINED;
}

// 隐藏图层
static JSValue js_hide(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    if (argc < 1) return JS_UNDEFINED;

    size_t len;
    const char* layer_id = JS_ToCStringLen(ctx, &len, argv[0]);

    if (layer_id && g_layer_root) {
        struct Layer* layer = find_layer_by_id(g_layer_root, layer_id);
        if (layer) {
            layer->visible = 0; // IN_VISIBLE
            printf("JS(QuickJS): Hide layer '%s'\n", layer_id);
        }
    }

    JS_FreeCString(ctx, layer_id);
    return JS_UNDEFINED;
}

// 显示图层
static JSValue js_show(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    if (argc < 1) return JS_UNDEFINED;

    size_t len;
    const char* layer_id = JS_ToCStringLen(ctx, &len, argv[0]);

    if (layer_id && g_layer_root) {
        struct Layer* layer = find_layer_by_id(g_layer_root, layer_id);
        if (layer) {
            layer->visible = 1; // VISIBLE
            printf("JS(QuickJS): Show layer '%s'\n", layer_id);
        }
    }

    JS_FreeCString(ctx, layer_id);
    return JS_UNDEFINED;
}

// 打印日志
static JSValue js_log(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    for (int i = 0; i < argc; i++) {
        if (i != 0) printf(" ");
        const char* str = JS_ToCString(ctx, argv[i]);
        if (str) {
            printf("%s", str);
            JS_FreeCString(ctx, str);
        }
    }
    printf("\n");
    return JS_UNDEFINED;
}

// 从 JSON 字符串动态渲染到指定图层
static JSValue js_render_from_json(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    if (argc < 2) {
        return JS_ThrowTypeError(ctx, "Expected 2 arguments: layer_id and json_string");
    }

    size_t len1, len2;
    const char* layer_id = JS_ToCStringLen(ctx, &len1, argv[0]);
    const char* json_str = JS_ToCStringLen(ctx, &len2, argv[1]);

    if (!layer_id || !json_str) {
        if (layer_id) JS_FreeCString(ctx, layer_id);
        if (json_str) JS_FreeCString(ctx, json_str);
        return JS_ThrowTypeError(ctx, "Invalid arguments");
    }

    printf("JS(QuickJS): renderFromJson called with layer_id='%s'\n", layer_id);

    if (g_layer_root) {
        // 查找目标图层
        Layer* parent_layer = find_layer_by_id(g_layer_root, layer_id);
        if (!parent_layer) {
            printf("JS(QuickJS): ERROR - Layer '%s' not found\n", layer_id);
            JS_FreeCString(ctx, layer_id);
            JS_FreeCString(ctx, json_str);
            return JS_NewInt32(ctx, -1);
        }

        printf("JS(QuickJS): Found parent layer '%s'\n", layer_id);

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
                printf("JS(QuickJS): ERROR - Failed to allocate memory for children array\n");
                destroy_layer(new_layer);
                JS_FreeCString(ctx, layer_id);
                JS_FreeCString(ctx, json_str);
                return JS_NewInt32(ctx, -2);
            }

            parent_layer->children[0] = new_layer;
            parent_layer->child_count = 1;

            // 重新布局父图层和新的子图层
            printf("JS(QuickJS): Reloading layout for parent layer '%s'\n", layer_id);
            layout_layer(parent_layer);
            printf("JS(QuickJS): Layout updated successfully\n");

            // 为新创建的图层加载字体
            printf("JS(QuickJS): Loading fonts for new layer\n");
            load_all_fonts(new_layer);
            printf("JS(QuickJS): Fonts loaded successfully\n");

            printf("JS(QuickJS): Successfully rendered JSON to layer '%s', new layer id: '%s'\n",
                   layer_id, new_layer->id);
            JS_FreeCString(ctx, layer_id);
            JS_FreeCString(ctx, json_str);
            return JS_NewInt32(ctx, 0);
        } else {
            printf("JS(QuickJS): ERROR - Failed to parse JSON string\n");
            JS_FreeCString(ctx, layer_id);
            JS_FreeCString(ctx, json_str);
            return JS_NewInt32(ctx, -3);
        }
    }

    printf("JS(QuickJS): ERROR - g_layer_root is NULL\n");
    JS_FreeCString(ctx, layer_id);
    JS_FreeCString(ctx, json_str);
    return JS_NewInt32(ctx, -4);
}

/* ====================== 初始化和清理 ====================== */

// 初始化 JS 引擎（使用 QuickJS）
int js_module_init(void)
{
    printf("JS(QuickJS): Initializing QuickJS engine...\n");
    
    // 创建 QuickJS Runtime
    g_js_rt = JS_NewRuntime();
    if (!g_js_rt) {
        fprintf(stderr, "JS(QuickJS): Failed to create Runtime\n");
        return -1;
    }

    // 创建 QuickJS Context
    g_js_ctx = JS_NewContext(g_js_rt);
    if (!g_js_ctx) {
        fprintf(stderr, "JS(QuickJS): Failed to create Context\n");
        JS_FreeRuntime(g_js_rt);
        g_js_rt = NULL;
        return -1;
    }

    // 注册 API 函数
    js_module_register_api();

    printf("JS(QuickJS): QuickJS engine initialized\n");
    return 0;
}

// 清理 JS 引擎
void js_module_cleanup(void)
{
    if (g_js_ctx) {
        JS_FreeContext(g_js_ctx);
        g_js_ctx = NULL;
    }
    if (g_js_rt) {
        JS_FreeRuntime(g_js_rt);
        g_js_rt = NULL;
    }
}

// 注册 C API 到 JS
void js_module_register_api(void)
{
    if (!g_js_ctx) return;

    // 获取全局对象
    JSValue global_obj = JS_GetGlobalObject(g_js_ctx);

    // 注册 YUI 对象
    JSValue yui_obj = JS_NewObject(g_js_ctx);

    // 设置 YUI 的方法
    JS_SetPropertyStr(g_js_ctx, yui_obj, "setText", JS_NewCFunction(g_js_ctx, js_set_text, "setText", 2));
    JS_SetPropertyStr(g_js_ctx, yui_obj, "getText", JS_NewCFunction(g_js_ctx, js_get_text, "getText", 1));
    JS_SetPropertyStr(g_js_ctx, yui_obj, "setBgColor", JS_NewCFunction(g_js_ctx, js_set_bg_color, "setBgColor", 2));
    JS_SetPropertyStr(g_js_ctx, yui_obj, "hide", JS_NewCFunction(g_js_ctx, js_hide, "hide", 1));
    JS_SetPropertyStr(g_js_ctx, yui_obj, "show", JS_NewCFunction(g_js_ctx, js_show, "show", 1));
    JS_SetPropertyStr(g_js_ctx, yui_obj, "renderFromJson", JS_NewCFunction(g_js_ctx, js_render_from_json, "renderFromJson", 2));
    JS_SetPropertyStr(g_js_ctx, yui_obj, "log", JS_NewCFunction(g_js_ctx, js_log, "log", 1));

    // 将 YUI 对象添加到全局
    JS_SetPropertyStr(g_js_ctx, global_obj, "YUI", yui_obj);
    
    // 也注册为全局函数（为了兼容性）
    JS_SetPropertyStr(g_js_ctx, global_obj, "setText", JS_NewCFunction(g_js_ctx, js_set_text, "setText", 2));
    JS_SetPropertyStr(g_js_ctx, global_obj, "getText", JS_NewCFunction(g_js_ctx, js_get_text, "getText", 1));
    JS_SetPropertyStr(g_js_ctx, global_obj, "setBgColor", JS_NewCFunction(g_js_ctx, js_set_bg_color, "setBgColor", 2));
    JS_SetPropertyStr(g_js_ctx, global_obj, "hide", JS_NewCFunction(g_js_ctx, js_hide, "hide", 1));
    JS_SetPropertyStr(g_js_ctx, global_obj, "show", JS_NewCFunction(g_js_ctx, js_show, "show", 1));

    // 注册 Socket API
    js_module_register_socket_api(g_js_ctx);

    JS_FreeValue(g_js_ctx, global_obj);

    printf("JS(QuickJS): Registered native API functions\n");
}

// 加载并执行 JS 文件
int js_module_load_file(const char* filename)
{
    if (!g_js_ctx) {
        fprintf(stderr, "JS(QuickJS): Engine not initialized\n");
        return -1;
    }

    printf("JS(QuickJS): Loading file %s...\n", filename);

    // 读取文件
    FILE* f = fopen(filename, "r");
    if (!f) {
        fprintf(stderr, "JS(QuickJS): Failed to open file %s\n", filename);
        return -1;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* buf = (char*)malloc(size + 1);
    if (!buf) {
        fclose(f);
        return -1;
    }

    fread(buf, 1, size, f);
    buf[size] = '\0';
    fclose(f);

    // 使用 QuickJS 加载并运行代码
    JSValue val = JS_Eval(g_js_ctx, buf, size, filename, JS_EVAL_TYPE_GLOBAL);
    free(buf);

    if (JS_IsException(val)) {
        JSValue exc = JS_GetException(g_js_ctx);
        fprintf(stderr, "JS(QuickJS): Error executing %s:\n", filename);
        
        // 打印异常信息
        JSValue val1 = JS_GetPropertyStr(g_js_ctx, exc, "message");
        if (!JS_IsUndefined(val1)) {
            const char* msg = JS_ToCString(g_js_ctx, val1);
            if (msg) {
                fprintf(stderr, "  %s\n", msg);
                JS_FreeCString(g_js_ctx, msg);
            }
        }
        JS_FreeValue(g_js_ctx, val1);
        
        JS_FreeValue(g_js_ctx, exc);
        JS_FreeValue(g_js_ctx, val);
        return -1;
    }

    JS_FreeValue(g_js_ctx, val);
    printf("JS(QuickJS): Successfully loaded %s\n", filename);
    return 0;
}

// 调用 JS 事件函数
int js_module_call_event(const char* event_name, Layer* layer)
{
    if (!g_js_ctx || !event_name) return -1;

    // 移除 @ 前缀（如果有）
    const char* func_name = event_name;
    if (func_name[0] == '@') {
        func_name++;
    }

    // 获取全局对象
    JSValue global_obj = JS_GetGlobalObject(g_js_ctx);
    JSValue func = JS_GetPropertyStr(g_js_ctx, global_obj, func_name);

    if (JS_IsUndefined(func) || JS_IsNull(func)) {
        JS_FreeValue(g_js_ctx, global_obj);
        JS_FreeValue(g_js_ctx, func);
        return -1;
    }

    if (!JS_IsFunction(g_js_ctx, func)) {
        JS_FreeValue(g_js_ctx, global_obj);
        JS_FreeValue(g_js_ctx, func);
        return -1;
    }

    // 准备参数
    JSValue args[1];
    args[0] = layer ? JS_NewString(g_js_ctx, layer->id) : JS_NULL;

    // 调用函数
    JSValue result = JS_Call(g_js_ctx, func, JS_UNDEFINED, 1, args);

    // 清理
    JS_FreeValue(g_js_ctx, args[0]);
    JS_FreeValue(g_js_ctx, global_obj);
    JS_FreeValue(g_js_ctx, func);

    if (JS_IsException(result)) {
        JSValue exc = JS_GetException(g_js_ctx);
        fprintf(stderr, "JS(QuickJS): Error calling event %s:\n", event_name);
        
        JSValue val1 = JS_GetPropertyStr(g_js_ctx, exc, "message");
        if (!JS_IsUndefined(val1)) {
            const char* msg = JS_ToCString(g_js_ctx, val1);
            if (msg) {
                fprintf(stderr, "  %s\n", msg);
                JS_FreeCString(g_js_ctx, msg);
            }
        }
        JS_FreeValue(g_js_ctx, val1);
        
        JS_FreeValue(g_js_ctx, exc);
        JS_FreeValue(g_js_ctx, result);
        return -1;
    }

    JS_FreeValue(g_js_ctx, result);
    return 0;
}
