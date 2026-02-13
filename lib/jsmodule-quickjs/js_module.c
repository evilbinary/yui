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
#include "../../lib/cjson/cJSON.h"

// Layer 结构的最小定义
#define MAX_TEXT 256

/* ====================== 全局变量 ====================== */

static JSRuntime* g_js_rt = NULL;
static JSContext* g_js_ctx = NULL;
extern struct Layer* g_layer_root;

/* ====================== QuickJS 原生函数 ====================== */

JSValue js_read_file(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

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

// 获取图层的属性值（通用）
static JSValue js_get_property(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    if (argc < 2) return JS_UNDEFINED;

    size_t len1, len2;
    const char* layer_id = JS_ToCStringLen(ctx, &len1, argv[0]);
    const char* property_name = JS_ToCStringLen(ctx, &len2, argv[1]);

    if (layer_id && property_name && g_layer_root) {
        // 调用 js_common.c 中的函数
        extern const char* js_module_get_property_value(const char* layer_id, const char* property_name);
        const char* value = js_module_get_property_value(layer_id, property_name);
        
        if (value) {
            JS_FreeCString(ctx, layer_id);
            JS_FreeCString(ctx, property_name);
            JSValue result = JS_NewString(ctx, value);
            // 释放返回的字符串
            free((void*)value);
            return result;
        }
    }

    JS_FreeCString(ctx, layer_id);
    JS_FreeCString(ctx, property_name);
    return JS_UNDEFINED;
}

// 设置属性
static JSValue js_set_property(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc < 3) {
        return JS_ThrowTypeError(ctx, "Expected 3 arguments: layer_id, property_name, value");
    }

    const char* layer_id = JS_ToCString(ctx, argv[0]);
    const char* property_name = JS_ToCString(ctx, argv[1]);
    const char* value = JS_ToCString(ctx, argv[2]);

    if (!layer_id || !property_name || !value) {
        if (layer_id) JS_FreeCString(ctx, layer_id);
        if (property_name) JS_FreeCString(ctx, property_name);
        if (value) JS_FreeCString(ctx, value);
        return JS_ThrowTypeError(ctx, "Invalid arguments");
    }

    if (g_layer_root) {
        Layer* layer = find_layer_by_id(g_layer_root, layer_id);
        if (layer) {
            // 创建 JSON 对象来存储字符串值
            extern cJSON* cJSON_CreateString(const char* string);
            extern void cJSON_Delete(cJSON* item);
            extern int layer_set_property_from_json(Layer* layer, const char* key, cJSON* value, int is_creating);
            
            cJSON* json_value = cJSON_CreateString(value);
            if (json_value) {
                int result = layer_set_property_from_json(layer, property_name, json_value, 0);
                printf("JS(QuickJS): Set property '%s' to '%s' on layer '%s', result=%d\n", 
                       property_name, value, layer_id, result);
                cJSON_Delete(json_value);
            }
        } else {
            printf("JS(QuickJS): Layer '%s' not found\n", layer_id);
        }
    }

    JS_FreeCString(ctx, layer_id);
    JS_FreeCString(ctx, property_name);
    JS_FreeCString(ctx, value);
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

// JSON 增量更新
extern int yui_update(Layer* root, const char* update_json);

static JSValue js_update(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    if (argc < 1) {
        return JS_ThrowTypeError(ctx, "Expected at least 1 argument (string or object)");
    }

    const char* update_json = NULL;
    int need_free = 0;
    
    // 支持字符串和对象两种参数类型
    if (JS_IsString(argv[0])) {
        // 如果是字符串，直接使用
        update_json = JS_ToCString(ctx, argv[0]);
        need_free = 1;
    } else if (JS_IsObject(argv[0])) {
        // 如果是对象，调用 JSON.stringify 转换
        JSValue global = JS_GetGlobalObject(ctx);
        JSValue json_obj = JS_GetPropertyStr(ctx, global, "JSON");
        JSValue stringify_func = JS_GetPropertyStr(ctx, json_obj, "stringify");
        
        if (JS_IsFunction(ctx, stringify_func)) {
            JSValue json_str = JS_Call(ctx, stringify_func, json_obj, 1, argv);
            if (!JS_IsException(json_str) && JS_IsString(json_str)) {
                update_json = JS_ToCString(ctx, json_str);
                need_free = 1;
            }
            JS_FreeValue(ctx, json_str);
        }
        
        JS_FreeValue(ctx, stringify_func);
        JS_FreeValue(ctx, json_obj);
        JS_FreeValue(ctx, global);
        
        if (!update_json) {
            return JS_ThrowTypeError(ctx, "Failed to stringify object");
        }
    } else {
        return JS_ThrowTypeError(ctx, "Argument must be string or object");
    }

    int result = -1;
    if (update_json && g_layer_root) {
        result = yui_update(g_layer_root, update_json);
    }

    if (need_free && update_json) {
        JS_FreeCString(ctx, update_json);
    }

    return JS_NewInt32(ctx, result);
}

/* ====================== 主题管理函数 ====================== */

#include "../../src/theme_manager.h"

// 加载主题文件或JSON字符串
static JSValue js_theme_load(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    if (argc < 1) {
        return JS_ThrowTypeError(ctx, "themeLoad requires 1 argument: theme_path or theme_json");
    }

    size_t len;
    const char* theme_input = JS_ToCStringLen(ctx, &len, argv[0]);
    if (!theme_input) {
        return JS_ThrowTypeError(ctx, "Invalid theme input");
    }

    ThemeManager* manager = theme_manager_get_instance();
    Theme* theme = NULL;
    
    // 检查输入是文件路径还是JSON字符串
    // 如果是JSON字符串，应该以 '{' 或 '[' 开头
    if (len > 0 && (theme_input[0] == '{' || theme_input[0] == '[')) {
        // 是JSON字符串，从JSON加载
        theme = theme_manager_load_theme_from_json(theme_input);
    } else {
        // 是文件路径，从文件加载
        theme = theme_manager_load_theme(theme_input);
    }

    JS_FreeCString(ctx, theme_input);

    if (theme) {
        JSValue result = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, result, "success", JS_NewBool(ctx, 1));
        JS_SetPropertyStr(ctx, result, "name", JS_NewString(ctx, theme->name));
        JS_SetPropertyStr(ctx, result, "version", JS_NewString(ctx, theme->version));
        printf("JS(QuickJS): Loaded theme: %s (v%s)\n", theme->name, theme->version);
        return result;
    } else {
        JSValue result = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, result, "success", JS_NewBool(ctx, 0));
        JS_SetPropertyStr(ctx, result, "error", JS_NewString(ctx, "Failed to load theme"));
        printf("JS(QuickJS): Failed to load theme\n");
        return result;
    }
}

// 设置当前主题
static JSValue js_theme_set_current(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    if (argc < 1) {
        return JS_ThrowTypeError(ctx, "themeSetCurrent requires 1 argument: theme_name");
    }

    size_t len;
    const char* theme_name = JS_ToCStringLen(ctx, &len, argv[0]);
    if (!theme_name) {
        return JS_ThrowTypeError(ctx, "Invalid theme name");
    }

    int result = theme_manager_set_current(theme_name);
    
    if (result) {
        printf("JS(QuickJS): Current theme set to: %s\n", theme_name);
    } else {
        printf("JS(QuickJS): Failed to set current theme: %s\n", theme_name);
    }
    
    JS_FreeCString(ctx, theme_name);
    
    return JS_NewBool(ctx, result);
}

// 卸载主题
static JSValue js_theme_unload(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    if (argc < 1) {
        return JS_ThrowTypeError(ctx, "themeUnload requires 1 argument: theme_name");
    }

    size_t len;
    const char* theme_name = JS_ToCStringLen(ctx, &len, argv[0]);
    if (!theme_name) {
        return JS_ThrowTypeError(ctx, "Invalid theme name");
    }

    theme_manager_unload_theme(theme_name);
    
    JS_FreeCString(ctx, theme_name);
    
    printf("JS(QuickJS): Unloaded theme: %s\n", theme_name);
    return JS_NewBool(ctx, 1);
}

// 应用主题到图层树
static JSValue js_theme_apply_to_tree(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    ThemeManager* manager = theme_manager_get_instance();
    Theme* current = theme_manager_get_current();
    
    if (current && g_layer_root) {
        theme_manager_apply_to_tree(g_layer_root);
        printf("JS(QuickJS): Applied theme '%s' to layer tree\n", current->name);
        return JS_NewBool(ctx, 1);
    }
    
    printf("JS(QuickJS): No current theme set or no layer root\n");
    return JS_NewBool(ctx, 0);
}

/* ====================== Inspect 调试函数 ====================== */

// Inspect 启用函数
static JSValue js_inspect_enable(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    extern int yui_inspect_mode_enabled;
    yui_inspect_mode_enabled = 1;
    printf("JS(QuickJS): Enabled global inspect mode\n");
    return JS_NewBool(ctx, 1);
}

// Inspect 禁用函数
static JSValue js_inspect_disable(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    extern int yui_inspect_mode_enabled;
    yui_inspect_mode_enabled = 0;
    printf("JS(QuickJS): Disabled global inspect mode\n");
    return JS_NewBool(ctx, 1);
}

// Inspect 设置图层函数
static JSValue js_inspect_set_layer(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    if (argc < 2) {
        return JS_ThrowTypeError(ctx, "inspect.setLayer requires 2 arguments: layer_id and enabled");
    }
    
    size_t len;
    const char* layer_id = JS_ToCStringLen(ctx, &len, argv[0]);
    int enabled = JS_ToBool(ctx, argv[1]);
    
    if (layer_id && g_layer_root) {
        struct Layer* layer = find_layer_by_id(g_layer_root, layer_id);
        if (layer) {
            layer->inspect_enabled = enabled;
            printf("JS(QuickJS): Set layer '%s' inspect enabled = %d\n", layer_id, enabled);
            JS_FreeCString(ctx, layer_id);
            return JS_NewBool(ctx, 1);
        } else {
            printf("JS(QuickJS): Layer '%s' not found\n", layer_id);
            JS_FreeCString(ctx, layer_id);
            return JS_NewBool(ctx, 0);
        }
    }
    
    if (layer_id) JS_FreeCString(ctx, layer_id);
    return JS_NewBool(ctx, 0);
}

// Inspect 设置显示边界函数
static JSValue js_inspect_set_show_bounds(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    if (argc < 1) {
        return JS_ThrowTypeError(ctx, "inspect.setShowBounds requires 1 argument: show_bounds");
    }
    
    int show_bounds = JS_ToBool(ctx, argv[0]);
    extern int yui_inspect_show_bounds;
    yui_inspect_show_bounds = show_bounds;
    printf("JS(QuickJS): Set show bounds = %d\n", show_bounds);
    return JS_NewBool(ctx, 1);
}

// Inspect 设置显示信息函数
static JSValue js_inspect_set_show_info(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    if (argc < 1) {
        return JS_ThrowTypeError(ctx, "inspect.setShowInfo requires 1 argument: show_info");
    }
    
    int show_info = JS_ToBool(ctx, argv[0]);
    extern int yui_inspect_show_info;
    yui_inspect_show_info = show_info;
    printf("JS(QuickJS): Set show info = %d\n", show_info);
    return JS_NewBool(ctx, 1);
}

// YUI.setEvent() - 设置图层事件回调
static JSValue js_set_event(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    if (argc < 3) {
        return JS_ThrowTypeError(ctx, "setEvent requires 3 arguments: layer_id, event_name, callback");
    }
    
    size_t len1, len2, len3;
    const char* layer_id = JS_ToCStringLen(ctx, &len1, argv[0]);
    const char* event_name = JS_ToCStringLen(ctx, &len2, argv[1]);
    const char* func_name = JS_ToCStringLen(ctx, &len3, argv[2]);
    
    if (!layer_id || !event_name || !func_name) {
        JS_FreeCString(ctx, layer_id);
        JS_FreeCString(ctx, event_name);
        JS_FreeCString(ctx, func_name);
        return JS_ThrowTypeError(ctx, "Invalid arguments");
    }
    
    // 调用公共实现
    int result = js_module_set_event(layer_id, event_name, func_name);
    
    JS_FreeCString(ctx, layer_id);
    JS_FreeCString(ctx, event_name);
    JS_FreeCString(ctx, func_name);
    
    return JS_NewBool(ctx, result == 0 ? 1 : 0);
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
    JS_SetPropertyStr(g_js_ctx, yui_obj, "getProperty", JS_NewCFunction(g_js_ctx, js_get_property, "getProperty", 2));
    JS_SetPropertyStr(g_js_ctx, yui_obj, "setProperty", JS_NewCFunction(g_js_ctx, js_set_property, "setProperty", 3));

    JS_SetPropertyStr(g_js_ctx, yui_obj, "setBgColor", JS_NewCFunction(g_js_ctx, js_set_bg_color, "setBgColor", 2));
    JS_SetPropertyStr(g_js_ctx, yui_obj, "hide", JS_NewCFunction(g_js_ctx, js_hide, "hide", 1));
    JS_SetPropertyStr(g_js_ctx, yui_obj, "show", JS_NewCFunction(g_js_ctx, js_show, "show", 1));
    JS_SetPropertyStr(g_js_ctx, yui_obj, "renderFromJson", JS_NewCFunction(g_js_ctx, js_render_from_json, "renderFromJson", 2));
    JS_SetPropertyStr(g_js_ctx, yui_obj, "update", JS_NewCFunction(g_js_ctx, js_update, "update", 1));
    JS_SetPropertyStr(g_js_ctx, yui_obj, "log", JS_NewCFunction(g_js_ctx, js_log, "log", 1));
    JS_SetPropertyStr(g_js_ctx, yui_obj, "themeLoad", JS_NewCFunction(g_js_ctx, js_theme_load, "themeLoad", 1));
    JS_SetPropertyStr(g_js_ctx, yui_obj, "themeSetCurrent", JS_NewCFunction(g_js_ctx, js_theme_set_current, "themeSetCurrent", 1));
    JS_SetPropertyStr(g_js_ctx, yui_obj, "themeUnload", JS_NewCFunction(g_js_ctx, js_theme_unload, "themeUnload", 1));
    JS_SetPropertyStr(g_js_ctx, yui_obj, "themeApplyToTree", JS_NewCFunction(g_js_ctx, js_theme_apply_to_tree, "themeApplyToTree", 0));
    JS_SetPropertyStr(g_js_ctx, yui_obj, "themeUnload", JS_NewCFunction(g_js_ctx, js_theme_unload, "themeUnload", 1));

    JS_SetPropertyStr(g_js_ctx, yui_obj, "readFile", JS_NewCFunction(g_js_ctx, js_read_file, "readFile", 1));

    // 创建并注册 Inspect 对象
    JSValue inspect_obj = JS_NewObject(g_js_ctx);
    JS_SetPropertyStr(g_js_ctx, inspect_obj, "enable", JS_NewCFunction(g_js_ctx, js_inspect_enable, "enable", 0));
    JS_SetPropertyStr(g_js_ctx, inspect_obj, "disable", JS_NewCFunction(g_js_ctx, js_inspect_disable, "disable", 0));
    JS_SetPropertyStr(g_js_ctx, inspect_obj, "setLayer", JS_NewCFunction(g_js_ctx, js_inspect_set_layer, "setLayer", 2));
    JS_SetPropertyStr(g_js_ctx, inspect_obj, "setShowBounds", JS_NewCFunction(g_js_ctx, js_inspect_set_show_bounds, "setShowBounds", 1));
    JS_SetPropertyStr(g_js_ctx, inspect_obj, "setShowInfo", JS_NewCFunction(g_js_ctx, js_inspect_set_show_info, "setShowInfo", 1));
    JS_SetPropertyStr(g_js_ctx, yui_obj, "inspect", inspect_obj);
    
    // 添加 setEvent 到 YUI 对象
    JS_SetPropertyStr(g_js_ctx, yui_obj, "setEvent", JS_NewCFunction(g_js_ctx, js_set_event, "setEvent", 3));

    // 将 YUI 对象添加到全局
    JS_SetPropertyStr(g_js_ctx, global_obj, "YUI", yui_obj);
    
    // 也注册为全局函数（为了兼容性）
    JS_SetPropertyStr(g_js_ctx, global_obj, "setText", JS_NewCFunction(g_js_ctx, js_set_text, "setText", 2));
    JS_SetPropertyStr(g_js_ctx, global_obj, "getText", JS_NewCFunction(g_js_ctx, js_get_text, "getText", 1));
    JS_SetPropertyStr(g_js_ctx, global_obj, "getProperty", JS_NewCFunction(g_js_ctx, js_get_property, "getProperty", 2));
    JS_SetPropertyStr(g_js_ctx, global_obj, "setBgColor", JS_NewCFunction(g_js_ctx, js_set_bg_color, "setBgColor", 2));
    JS_SetPropertyStr(g_js_ctx, global_obj, "hide", JS_NewCFunction(g_js_ctx, js_hide, "hide", 1));
    JS_SetPropertyStr(g_js_ctx, global_obj, "show", JS_NewCFunction(g_js_ctx, js_show, "show", 1));
    JS_SetPropertyStr(g_js_ctx, global_obj, "setEvent", JS_NewCFunction(g_js_ctx, js_set_event, "setEvent", 3));

    // 注册 Socket API
    js_module_register_socket_api(g_js_ctx);

    // 注册 readFile 到全局对象
    JS_SetPropertyStr(g_js_ctx, global_obj, "readFile", JS_NewCFunction(g_js_ctx, js_read_file, "readFile", 1));


    JS_FreeValue(g_js_ctx, global_obj);

    printf("JS(QuickJS): Registered native API functions\n");
}

// 打印 QuickJS 异常信息（包括行号和堆栈）
static void print_quickjs_exception(JSContext* ctx, JSValueConst exception, const char* filename)
{
    printf("JS(QuickJS): Error executing %s:\n", filename);
    
    // 打印错误消息
    JSValue msg_val = JS_GetPropertyStr(ctx, exception, "message");
    if (!JS_IsUndefined(msg_val)) {
        const char* msg = JS_ToCString(ctx, msg_val);
        if (msg) {
            printf("  Message: %s\n", msg);
            JS_FreeCString(ctx, msg);
        }
    }
    JS_FreeValue(ctx, msg_val);
    
    // 打印文件名
    JSValue fname_val = JS_GetPropertyStr(ctx, exception, "fileName");
    if (!JS_IsUndefined(fname_val)) {
        const char* fname = JS_ToCString(ctx, fname_val);
        if (fname) {
            printf("  File: %s\n", fname);
            JS_FreeCString(ctx, fname);
        }
    }
    JS_FreeValue(ctx, fname_val);
    
    // 打印行号
    JSValue line_val = JS_GetPropertyStr(ctx, exception, "lineNumber");
    if (!JS_IsUndefined(line_val)) {
        int32_t line;
        if (JS_ToInt32(ctx, &line, line_val) == 0) {
            printf("  Line: %d\n", line);
        }
    }
    JS_FreeValue(ctx, line_val);
    
    // 打印列号
    JSValue col_val = JS_GetPropertyStr(ctx, exception, "columnNumber");
    if (!JS_IsUndefined(col_val)) {
        int32_t col;
        if (JS_ToInt32(ctx, &col, col_val) == 0) {
            printf("  Column: %d\n", col);
        }
    }
    JS_FreeValue(ctx, col_val);
    
    // 打印堆栈信息
    JSValue stack_val = JS_GetPropertyStr(ctx, exception, "stack");
    if (!JS_IsUndefined(stack_val)) {
        const char* stack = JS_ToCString(ctx, stack_val);
        if (stack) {
            printf("  Stack:\n%s\n", stack);
            JS_FreeCString(ctx, stack);
        }
    }
    JS_FreeValue(ctx, stack_val);
}

// 加载并执行 JS 文件
int js_module_load_file(const char* filename)
{
    if (!g_js_ctx) {
        fprintf(stderr, "JS(QuickJS): Engine not initialized\n");
        return -1;
    }

    printf("JS(QuickJS): Loading file %s...\n", filename);

    // 读取文件 (使用二进制模式避免 Windows 文本模式转换)
    FILE* f = fopen(filename, "rb");
    if (!f) {
        printf("JS(QuickJS): Failed to open file %s\n", filename);
        return -1;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* buf = (char*)malloc(size + 1);
    if (!buf) {
        fclose(f);
        printf("JS(QuickJS): Failed to allocate memory for file\n");
        return -1;
    }

    size_t read_size = fread(buf, 1, size, f);
    buf[read_size] = '\0';
    fclose(f);
    
    printf("JS(QuickJS): Read %ld bytes from file\n", (long)read_size);

    // 使用 QuickJS 加载并运行代码
    JSValue val = JS_Eval(g_js_ctx, buf, read_size, filename, JS_EVAL_TYPE_GLOBAL);
    free(buf);

    if (JS_IsException(val)) {
        JSValue exc = JS_GetException(g_js_ctx);
        print_quickjs_exception(g_js_ctx, exc, filename);
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
        char err_prefix[256];
        snprintf(err_prefix, sizeof(err_prefix), "event '%s'", event_name);
        print_quickjs_exception(g_js_ctx, exc, err_prefix);
        JS_FreeValue(g_js_ctx, exc);
        JS_FreeValue(g_js_ctx, result);
        return -1;
    }

    JS_FreeValue(g_js_ctx, result);
    return 0;
}


// 文件读取函数绑定
JSValue js_read_file(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    if (argc < 1) {
        return JS_ThrowTypeError(ctx, "Expected 1 argument: file_path");
    }

    const char* file_path = JS_ToCString(ctx, argv[0]);
    if (!file_path) {
        return JS_ThrowTypeError(ctx, "Invalid file path");
    }

    extern char* js_module_read_file(const char* file_path);
    char* content = js_module_read_file(file_path);
    JS_FreeCString(ctx, file_path);

    if (!content) {
        return JS_ThrowInternalError(ctx, "Failed to read file");
    }

    JSValue result = JS_NewString(ctx, content);
    free(content);
    return result;
}