#include "lib/jsmodule/js_module.h"
#include "../../lib/quickjs/quickjs.h"
#include "../../src/layer.h"
#include "../../src/layer_properties.h"
#include "../../src/layout.h"
#include "../../src/layer_update.h"
#include "../../src/layer_lifecycle.h"
#include "../../src/render.h"
#include "../../src/theme_manager.h"
#include "js_socket.h"
#include "js_timer.h"
#include "../../src/event.h"
#include "../../src/backend.h"
#include "../../src/log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include "../../lib/cjson/cJSON.h"

// Layer 结构的最小定义
#define MAX_TEXT 256

/* ====================== 全局变量 ====================== */

static JSRuntime* g_js_rt = NULL;
static JSContext* g_js_ctx = NULL;
extern struct Layer* g_layer_root;

static void js_module_timer_tick(void)
{
    if (g_js_ctx) {
        js_timer_run(g_js_ctx);
    }
}

/* ====================== QuickJS 原生函数 ====================== */

JSValue js_read_file(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_write_file(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_screenshot(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_list_dir(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);
JSValue js_focus(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv);

static void print_quickjs_exception(JSContext* ctx, JSValueConst exception, const char* filename);

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
            layer_hide(layer);
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
            layer_show(layer);
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
    fflush(stdout);
    return JS_UNDEFINED;
}

static int append_layer_child_qjs(Layer* parent, Layer* child) {
    if (!parent || !child) return -1;
    Layer** new_children = realloc(parent->children, sizeof(Layer*) * (parent->child_count + 1));
    if (!new_children) return -2;
    parent->children = new_children;
    parent->children[parent->child_count] = child;
    parent->child_count++;
    return 0;
}

// 从 JSON 字符串动态渲染到指定图层（可选第三参数 append：true 时追加子图层，不清空已有子节点）
static JSValue js_render_from_json(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    if (argc < 2) {
        return JS_ThrowTypeError(ctx, "Expected at least 2 arguments: layer_id, json_string, append?");
    }

    size_t len1, len2;
    const char* layer_id = JS_ToCStringLen(ctx, &len1, argv[0]);
    const char* json_str = JS_ToCStringLen(ctx, &len2, argv[1]);

    if (!layer_id || !json_str) {
        if (layer_id) JS_FreeCString(ctx, layer_id);
        if (json_str) JS_FreeCString(ctx, json_str);
        return JS_ThrowTypeError(ctx, "Invalid arguments");
    }

    int append = 0;
    if (argc >= 3) {
        append = JS_ToBool(ctx, argv[2]);
    }

    const char* json_source_path = NULL;
    if (argc >= 4) {
        json_source_path = JS_ToCStringLen(ctx, NULL, argv[3]);
    }

    LOGD("js", "renderFromJson called with layer_id='%s', append=%d", layer_id, append);

    if (!g_layer_root) {
        JS_FreeCString(ctx, layer_id);
        JS_FreeCString(ctx, json_str);
        return JS_NewInt32(ctx, -4);
    }

    Layer* parent_layer = find_layer_by_id(g_layer_root, layer_id);
    if (!parent_layer) {
        LOGE("js", "renderFromJson: layer '%s' not found", layer_id);
        JS_FreeCString(ctx, layer_id);
        JS_FreeCString(ctx, json_str);
        return JS_NewInt32(ctx, -1);
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
        JS_FreeCString(ctx, layer_id);
        JS_FreeCString(ctx, json_str);
        return JS_NewInt32(ctx, -3);
    }
    new_layer->visible = IN_VISIBLE;

    if (append) {
        if (append_layer_child_qjs(parent_layer, new_layer) != 0) {
            destroy_layer(new_layer);
            JS_FreeCString(ctx, layer_id);
            JS_FreeCString(ctx, json_str);
            return JS_NewInt32(ctx, -2);
        }
    } else {
        parent_layer->children = malloc(sizeof(Layer*));
        if (!parent_layer->children) {
            destroy_layer(new_layer);
            JS_FreeCString(ctx, layer_id);
            JS_FreeCString(ctx, json_str);
            return JS_NewInt32(ctx, -2);
        }
        parent_layer->children[0] = new_layer;
        parent_layer->child_count = 1;
    }

    layout_layer(parent_layer);
    theme_manager_apply_to_tree(new_layer);

    cJSON* page_json = parse_json_string(json_str);
    if (page_json) {
        js_module_load_from_json(page_json, json_source_path, 1);
        cJSON_Delete(page_json);
    }

    if (json_source_path) JS_FreeCString(ctx, json_source_path);

    LOGD("js", "rendered JSON to layer '%s', new layer id: '%s'", layer_id, new_layer->id);
    JS_FreeCString(ctx, layer_id);
    JS_FreeCString(ctx, json_str);
    return JS_NewInt32(ctx, 0);
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
    js_module_init_layer_lifecycle();
    backend_register_update_callback(js_module_timer_tick);

    printf("JS(QuickJS): QuickJS engine initialized\n");
    return 0;
}

// 清理 JS 引擎
void js_module_cleanup(void)
{
    js_module_shutdown();
    if (g_js_ctx) {
        js_timer_clear_all(g_js_ctx);
        JS_FreeContext(g_js_ctx);
        g_js_ctx = NULL;
    }
    if (g_js_rt) {
        JS_FreeRuntime(g_js_rt);
        g_js_rt = NULL;
    }
}

// ====================== Layer 包装对象支持 ======================

// 前向声明 (使用正确的 getter/setter 签名)
static JSValue js_layer_wrapper_get_text(JSContext* ctx, JSValueConst this_val);
static JSValue js_layer_wrapper_set_text(JSContext* ctx, JSValueConst this_val, JSValueConst val);
static JSValue js_layer_wrapper_get_style(JSContext* ctx, JSValueConst this_val);
static JSValue js_layer_wrapper_set_style(JSContext* ctx, JSValueConst this_val, JSValueConst val);
static JSValue js_layer_wrapper_get_visible(JSContext* ctx, JSValueConst this_val);
static JSValue js_layer_wrapper_set_visible(JSContext* ctx, JSValueConst this_val, JSValueConst val);
static JSValue js_layer_wrapper_get_size(JSContext* ctx, JSValueConst this_val);
static JSValue js_layer_wrapper_set_size(JSContext* ctx, JSValueConst this_val, JSValueConst val);

// 从包装对象获取 Layer 指针
static Layer* js_get_layer_from_wrapper(JSContext* ctx, JSValueConst val)
{
    if (!JS_IsObject(val)) {
        return NULL;
    }

    JSValue ptr_val = JS_GetPropertyStr(ctx, val, "__layer_ptr");
    if (JS_IsUndefined(ptr_val) || JS_IsNull(ptr_val)) {
        JS_FreeValue(ctx, ptr_val);
        return NULL;
    }

    int64_t ptr_int = 0;
    if (JS_ToInt64(ctx, &ptr_int, ptr_val) != 0 || ptr_int == 0) {
        JS_FreeValue(ctx, ptr_val);
        return NULL;
    }

    JS_FreeValue(ctx, ptr_val);

    Layer* layer = (Layer*)(uintptr_t)ptr_int;
    return layer;
}

// Layer 包装对象的 text 属性 getter
static JSValue js_layer_wrapper_get_text(JSContext* ctx, JSValueConst this_val)
{
    Layer* layer = js_get_layer_from_wrapper(ctx, this_val);
    if (!layer) return JS_UNDEFINED;
    
    const char* text = layer_get_text(layer);
    return JS_NewString(ctx, text ? text : "");
}

// Layer 包装对象的 text 属性 setter
static JSValue js_layer_wrapper_set_text(JSContext* ctx, JSValueConst this_val, JSValueConst val)
{
    Layer* layer = js_get_layer_from_wrapper(ctx, this_val);
    if (!layer) return JS_UNDEFINED;

    const char* text = JS_ToCString(ctx, val);
    if (text) {
        yui_set_text(layer, text);
        JS_FreeCString(ctx, text);
    }
    return JS_UNDEFINED;
}

// Layer 包装对象的 style 属性 getter
static JSValue js_layer_wrapper_get_style(JSContext* ctx, JSValueConst this_val)
{
    Layer* layer = js_get_layer_from_wrapper(ctx, this_val);
    if (!layer) return JS_UNDEFINED;
    
    // 创建一个包含当前样式的对象
    JSValue style_obj = JS_NewObject(ctx);
    
    // 添加 color 属性
    char color_str[32];
    snprintf(color_str, sizeof(color_str), "#%02X%02X%02X", layer->color.r, layer->color.g, layer->color.b);
    JS_SetPropertyStr(ctx, style_obj, "color", JS_NewString(ctx, color_str));
    
    // 添加 bgColor 属性
    char bg_color_str[32];
    snprintf(bg_color_str, sizeof(bg_color_str), "#%02X%02X%02X", layer->bg_color.r, layer->bg_color.g, layer->bg_color.b);
    JS_SetPropertyStr(ctx, style_obj, "bgColor", JS_NewString(ctx, bg_color_str));
    
    return style_obj;
}

// Layer 包装对象的 style 属性 setter
static JSValue js_layer_wrapper_set_style(JSContext* ctx, JSValueConst this_val, JSValueConst val)
{
    Layer* layer = js_get_layer_from_wrapper(ctx, this_val);
    if (!layer) return JS_UNDEFINED;

    int dirty = 0;

    // 获取 color 属性
    JSValue color_val = JS_GetPropertyStr(ctx, val, "color");
    if (!JS_IsUndefined(color_val)) {
        const char* color_str = JS_ToCString(ctx, color_val);
        if (color_str) {
            extern void parse_color(char* valuestring, Color* color);
            parse_color((char*)color_str, &layer->color);
            JS_FreeCString(ctx, color_str);
            dirty = 1;
        }
    }
    JS_FreeValue(ctx, color_val);

    // 获取 bgColor 属性
    JSValue bg_color_val = JS_GetPropertyStr(ctx, val, "bgColor");
    if (!JS_IsUndefined(bg_color_val)) {
        const char* bg_color_str = JS_ToCString(ctx, bg_color_val);
        if (bg_color_str) {
            extern void parse_color(char* valuestring, Color* color);
            parse_color((char*)bg_color_str, &layer->bg_color);
            JS_FreeCString(ctx, bg_color_str);
            dirty = 1;
        }
    }
    JS_FreeValue(ctx, bg_color_val);

    if (dirty) {
        mark_layer_dirty(layer, DIRTY_COLOR);
    }

    return JS_UNDEFINED;
}

// Layer 包装对象的 visible 属性 getter
static JSValue js_layer_wrapper_get_visible(JSContext* ctx, JSValueConst this_val)
{
    Layer* layer = js_get_layer_from_wrapper(ctx, this_val);
    if (!layer) return JS_UNDEFINED;
    return JS_NewBool(ctx, layer->visible == VISIBLE);
}

// Layer 包装对象的 visible 属性 setter
static JSValue js_layer_wrapper_set_visible(JSContext* ctx, JSValueConst this_val, JSValueConst val)
{
    Layer* layer = js_get_layer_from_wrapper(ctx, this_val);
    if (!layer) return JS_UNDEFINED;

    int js_visible = JS_ToBool(ctx, val);
    layer->visible = js_visible ? VISIBLE : IN_VISIBLE;
    mark_layer_dirty(layer, DIRTY_VISIBLE | DIRTY_LAYOUT);

    // 触发父容器重新布局
    if (layer->parent) {
        mark_layer_dirty(layer->parent, DIRTY_LAYOUT);
        layout_layer(layer->parent);
    }

    return JS_UNDEFINED;
}

static JSValue js_layer_wrapper_get_number_property(JSContext* ctx, Layer* layer, const char* key)
{
    if (!layer || !key) return JS_UNDEFINED;
    extern cJSON* layer_get_property_as_json(Layer* layer, const char* key);
    cJSON* value = layer_get_property_as_json(layer, key);
    if (!value) return JS_UNDEFINED;
    JSValue result = JS_UNDEFINED;
    if (cJSON_IsNumber(value)) {
        result = JS_NewFloat64(ctx, value->valuedouble);
    }
    cJSON_Delete(value);
    return result;
}

static JSValue js_layer_wrapper_set_number_property(JSContext* ctx, Layer* layer, const char* key, JSValueConst val)
{
    if (!layer || !key) return JS_UNDEFINED;
    double number = 0;
    if (JS_ToFloat64(ctx, &number, val) != 0) return JS_UNDEFINED;
    extern int layer_set_property_from_json(Layer* layer, const char* key, cJSON* value, int is_creating);
    cJSON* json_value = cJSON_CreateNumber(number);
    if (json_value) {
        layer_set_property_from_json(layer, key, json_value, 0);
        cJSON_Delete(json_value);
    }
    return JS_UNDEFINED;
}

static JSValue js_layer_wrapper_get_page(JSContext* ctx, JSValueConst this_val)
{
    Layer* layer = js_get_layer_from_wrapper(ctx, this_val);
    return js_layer_wrapper_get_number_property(ctx, layer, "page");
}

static JSValue js_layer_wrapper_set_page(JSContext* ctx, JSValueConst this_val, JSValueConst val)
{
    Layer* layer = js_get_layer_from_wrapper(ctx, this_val);
    return js_layer_wrapper_set_number_property(ctx, layer, "page", val);
}

static JSValue js_layer_wrapper_get_page_size(JSContext* ctx, JSValueConst this_val)
{
    Layer* layer = js_get_layer_from_wrapper(ctx, this_val);
    return js_layer_wrapper_get_number_property(ctx, layer, "pageSize");
}

static JSValue js_layer_wrapper_set_page_size(JSContext* ctx, JSValueConst this_val, JSValueConst val)
{
    Layer* layer = js_get_layer_from_wrapper(ctx, this_val);
    return js_layer_wrapper_set_number_property(ctx, layer, "pageSize", val);
}

static JSValue js_layer_wrapper_get_total(JSContext* ctx, JSValueConst this_val)
{
    Layer* layer = js_get_layer_from_wrapper(ctx, this_val);
    return js_layer_wrapper_get_number_property(ctx, layer, "total");
}

static JSValue js_layer_wrapper_set_total(JSContext* ctx, JSValueConst this_val, JSValueConst val)
{
    Layer* layer = js_get_layer_from_wrapper(ctx, this_val);
    return js_layer_wrapper_set_number_property(ctx, layer, "total", val);
}

// Layer 包装对象的 size 属性 getter
static JSValue js_layer_wrapper_get_size(JSContext* ctx, JSValueConst this_val)
{
    Layer* layer = js_get_layer_from_wrapper(ctx, this_val);
    if (!layer) return JS_UNDEFINED;

    JSValue arr = JS_NewArray(ctx);
    JS_SetPropertyUint32(ctx, arr, 0, JS_NewInt32(ctx, layer->rect.w));
    JS_SetPropertyUint32(ctx, arr, 1, JS_NewInt32(ctx, layer->rect.h));
    return arr;
}

// Layer 包装对象的 size 属性 setter
static JSValue js_layer_wrapper_set_size(JSContext* ctx, JSValueConst this_val, JSValueConst val)
{
    Layer* layer = js_get_layer_from_wrapper(ctx, this_val);
    if (!layer) return JS_UNDEFINED;

    if (!JS_IsArray(ctx, val)) return JS_UNDEFINED;

    JSValue w_val = JS_GetPropertyUint32(ctx, val, 0);
    JSValue h_val = JS_GetPropertyUint32(ctx, val, 1);

    int w = 0, h = 0;
    if (JS_ToInt32(ctx, &w, w_val) == 0 && JS_ToInt32(ctx, &h, h_val) == 0) {
        layer->rect.w = w;
        layer->rect.h = h;
        layer->fixed_width = w;
        layer->fixed_height = h;
        mark_layer_dirty(layer, DIRTY_RECT | DIRTY_LAYOUT);

        // 触发父容器重新布局
        if (layer->parent) {
            mark_layer_dirty(layer->parent, DIRTY_LAYOUT);
            layout_layer(layer->parent);
        }
    }

    JS_FreeValue(ctx, w_val);
    JS_FreeValue(ctx, h_val);
    return JS_UNDEFINED;
}

// 递归将 TreeNode 转换为 JS 对象
static JSValue tree_node_to_js_object(JSContext* ctx, TreeNode* node)
{
    JSValue obj = JS_NewObject(ctx);
    
    if (node->text) {
        JS_SetPropertyStr(ctx, obj, "text", JS_NewString(ctx, node->text));
    }
    
    if (node->expanded) {
        JS_SetPropertyStr(ctx, obj, "expanded", JS_NewBool(ctx, 1));
    }
    
    if (node->child_count > 0) {
        JSValue children = JS_NewArray(ctx);
        for (int i = 0; i < node->child_count; i++) {
            JSValue child_obj = tree_node_to_js_object(ctx, node->children[i]);
            JS_SetPropertyUint32(ctx, children, i, child_obj);
        }
        JS_SetPropertyStr(ctx, obj, "children", children);
    }
    
    return obj;
}

// Layer 包装对象的 data 属性 getter
static JSValue js_layer_wrapper_get_data(JSContext* ctx, JSValueConst this_val)
{
    Layer* layer = js_get_layer_from_wrapper(ctx, this_val);
    if (!layer) return JS_UNDEFINED;
    
    if (layer->type != TREEVIEW || !layer->component) {
        return JS_UNDEFINED;
    }
    
    TreeViewComponent* component = (TreeViewComponent*)layer->component;
    JSValue result = JS_NewArray(ctx);
    
    for (int i = 0; i < component->root_count; i++) {
        JSValue node_obj = tree_node_to_js_object(ctx, component->root_nodes[i]);
        JS_SetPropertyUint32(ctx, result, i, node_obj);
    }
    
    return result;
}

// Layer 包装对象的 data 属性 setter（通过统一属性系统）
static JSValue js_layer_wrapper_set_data(JSContext* ctx, JSValueConst this_val, JSValueConst val)
{
    Layer* layer = js_get_layer_from_wrapper(ctx, this_val);
    if (!layer) return JS_UNDEFINED;

    // JSON.stringify → cJSON 后走统一 property 系统
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue json_obj = JS_GetPropertyStr(ctx, global, "JSON");
    JSValue stringify = JS_GetPropertyStr(ctx, json_obj, "stringify");
    
    JSValue json_str_val = JS_Call(ctx, stringify, json_obj, 1, &val);
    if (JS_IsException(json_str_val)) {
        JS_FreeValue(ctx, json_str_val); JS_FreeValue(ctx, stringify);
        JS_FreeValue(ctx, json_obj); JS_FreeValue(ctx, global);
        return JS_UNDEFINED;
    }
    
    const char* json_str = JS_ToCString(ctx, json_str_val);
    if (json_str) {
        cJSON* data_json = cJSON_Parse(json_str);
        if (data_json) {
            int result = layer_set_data(layer, data_json);
            if (result != 2) {
                cJSON_Delete(data_json);
            }
        }
        JS_FreeCString(ctx, json_str);
    }
    
    JS_FreeValue(ctx, json_str_val);
    JS_FreeValue(ctx, stringify);
    JS_FreeValue(ctx, json_obj);
    JS_FreeValue(ctx, global);
    
    return JS_UNDEFINED;
}

// 创建 Layer 包装对象
static JSValue js_create_layer_wrapper(JSContext* ctx, Layer* layer)
{
    // 创建普通对象
    JSValue wrapper = JS_NewObject(ctx);

    // 使用整数存储 layer 指针（避免字符串 GC 问题）
    JSValue layer_ptr_val = JS_NewInt64(ctx, (int64_t)(uintptr_t)layer);
    JS_SetPropertyStr(ctx, wrapper, "__layer_ptr", layer_ptr_val);
    JS_FreeValue(ctx, layer_ptr_val);

    // 定义 text 属性的 getter/setter (使用 JS_NewCFunction2 指定正确的类型)
    JSValue text_getter = JS_NewCFunction2(ctx, (JSCFunction*)js_layer_wrapper_get_text, "get text", 0, JS_CFUNC_getter, 0);
    JSValue text_setter = JS_NewCFunction2(ctx, (JSCFunction*)js_layer_wrapper_set_text, "set text", 1, JS_CFUNC_setter, 0);
    JSAtom text_atom = JS_NewAtom(ctx, "text");
    JS_DefinePropertyGetSet(ctx, wrapper, text_atom, text_getter, text_setter, JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE);
    JS_FreeAtom(ctx, text_atom);
    
    // 定义 style 属性的 getter/setter
    JSValue style_getter = JS_NewCFunction2(ctx, (JSCFunction*)js_layer_wrapper_get_style, "get style", 0, JS_CFUNC_getter, 0);
    JSValue style_setter = JS_NewCFunction2(ctx, (JSCFunction*)js_layer_wrapper_set_style, "set style", 1, JS_CFUNC_setter, 0);
    JSAtom style_atom = JS_NewAtom(ctx, "style");
    JS_DefinePropertyGetSet(ctx, wrapper, style_atom, style_getter, style_setter, JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE);
    JS_FreeAtom(ctx, style_atom);
    
    // 定义 data 属性的 getter/setter
    JSValue data_getter = JS_NewCFunction2(ctx, (JSCFunction*)js_layer_wrapper_get_data, "get data", 0, JS_CFUNC_getter, 0);
    JSValue data_setter = JS_NewCFunction2(ctx, (JSCFunction*)js_layer_wrapper_set_data, "set data", 1, JS_CFUNC_setter, 0);
    JSAtom data_atom = JS_NewAtom(ctx, "data");
    JS_DefinePropertyGetSet(ctx, wrapper, data_atom, data_getter, data_setter, JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE);
    JS_FreeAtom(ctx, data_atom);

    // 定义 visible 属性的 getter/setter
    JSValue visible_getter = JS_NewCFunction2(ctx, (JSCFunction*)js_layer_wrapper_get_visible, "get visible", 0, JS_CFUNC_getter, 0);
    JSValue visible_setter = JS_NewCFunction2(ctx, (JSCFunction*)js_layer_wrapper_set_visible, "set visible", 1, JS_CFUNC_setter, 0);
    JSAtom visible_atom = JS_NewAtom(ctx, "visible");
    JS_DefinePropertyGetSet(ctx, wrapper, visible_atom, visible_getter, visible_setter, JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE);
    JS_FreeAtom(ctx, visible_atom);

    // 定义 size 属性的 getter/setter
    JSValue size_getter = JS_NewCFunction2(ctx, (JSCFunction*)js_layer_wrapper_get_size, "get size", 0, JS_CFUNC_getter, 0);
    JSValue size_setter = JS_NewCFunction2(ctx, (JSCFunction*)js_layer_wrapper_set_size, "set size", 1, JS_CFUNC_setter, 0);
    JSAtom size_atom = JS_NewAtom(ctx, "size");
    JS_DefinePropertyGetSet(ctx, wrapper, size_atom, size_getter, size_setter, JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE);
    JS_FreeAtom(ctx, size_atom);

    JSValue page_getter = JS_NewCFunction2(ctx, (JSCFunction*)js_layer_wrapper_get_page, "get page", 0, JS_CFUNC_getter, 0);
    JSValue page_setter = JS_NewCFunction2(ctx, (JSCFunction*)js_layer_wrapper_set_page, "set page", 1, JS_CFUNC_setter, 0);
    JSAtom page_atom = JS_NewAtom(ctx, "page");
    JS_DefinePropertyGetSet(ctx, wrapper, page_atom, page_getter, page_setter, JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE);
    JS_FreeAtom(ctx, page_atom);

    JSValue page_size_getter = JS_NewCFunction2(ctx, (JSCFunction*)js_layer_wrapper_get_page_size, "get pageSize", 0, JS_CFUNC_getter, 0);
    JSValue page_size_setter = JS_NewCFunction2(ctx, (JSCFunction*)js_layer_wrapper_set_page_size, "set pageSize", 1, JS_CFUNC_setter, 0);
    JSAtom page_size_atom = JS_NewAtom(ctx, "pageSize");
    JS_DefinePropertyGetSet(ctx, wrapper, page_size_atom, page_size_getter, page_size_setter, JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE);
    JS_FreeAtom(ctx, page_size_atom);

    JSValue total_getter = JS_NewCFunction2(ctx, (JSCFunction*)js_layer_wrapper_get_total, "get total", 0, JS_CFUNC_getter, 0);
    JSValue total_setter = JS_NewCFunction2(ctx, (JSCFunction*)js_layer_wrapper_set_total, "set total", 1, JS_CFUNC_setter, 0);
    JSAtom total_atom = JS_NewAtom(ctx, "total");
    JS_DefinePropertyGetSet(ctx, wrapper, total_atom, total_getter, total_setter, JS_PROP_CONFIGURABLE | JS_PROP_ENUMERABLE);
    JS_FreeAtom(ctx, total_atom);

    return wrapper;
}

// YUI.find() - 查找图层并返回包装对象
static JSValue js_yui_find(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv)
{
    if (argc < 1) return JS_NULL;
    
    const char* layer_id = JS_ToCString(ctx, argv[0]);
    if (!layer_id || !g_layer_root) return JS_NULL;
    
    Layer* layer = find_layer_by_id(g_layer_root, layer_id);
    if (!layer) {
        JS_FreeCString(ctx, layer_id);
        return JS_NULL;
    }
    
    // 创建 Layer 包装对象
    JSValue wrapper = js_create_layer_wrapper(ctx, layer);
    
    JS_FreeCString(ctx, layer_id);
    
    return wrapper;
}

// 注册 C API 到 JS
/* ====================== YUI.call Bridge ====================== */

void* js_module_get_context(void) {
    return g_js_ctx;
}

static JSValue js_native_call(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    if (argc < 1) {
        return JS_NewString(ctx, "{\"error\":\"Missing event name\"}");
    }

    const char* event_name = JS_ToCString(ctx, argv[0]);
    if (!event_name) {
        return JS_NewString(ctx, "{\"error\":\"Invalid event name\"}");
    }

    const char* json_args = NULL;
    if (argc > 1 && JS_IsString(argv[1])) {
        json_args = JS_ToCString(ctx, argv[1]);
    }

    EventHandler handler = find_event_by_name(event_name);
    JSValue result;
    if (handler) {
        char* ret = (char*)handler((void*)json_args);
        result = ret ? JS_NewString(ctx, ret) : JS_NewString(ctx, "{\"success\":true}");
        free(ret);
    } else {
        char buf[512];
        snprintf(buf, sizeof(buf), "{\"error\":\"No handler for event: %s\"}", event_name);
        result = JS_NewString(ctx, buf);
    }

    if (json_args) JS_FreeCString(ctx, json_args);
    JS_FreeCString(ctx, event_name);
    return result;
}

void js_module_register_api(void)
{
    if (!g_js_ctx) return;

    // 获取全局对象
    JSValue global_obj = JS_GetGlobalObject(g_js_ctx);

    // 注意：Layer 包装对象现在使用普通 JS 对象，通过 __layer_ptr 属性存储指针
    // getter/setter 在创建对象时动态设置

    // 注册 YUI 对象
    JSValue yui_obj = JS_NewObject(g_js_ctx);
    
    // 添加 find 方法到 YUI 对象
    JS_SetPropertyStr(g_js_ctx, yui_obj, "find", JS_NewCFunction(g_js_ctx, js_yui_find, "find", 1));
    printf("JS(QuickJS): Added find() method to YUI object\n");

    // 设置 YUI 的方法
    JS_SetPropertyStr(g_js_ctx, yui_obj, "setText", JS_NewCFunction(g_js_ctx, js_set_text, "setText", 2));
    JS_SetPropertyStr(g_js_ctx, yui_obj, "getText", JS_NewCFunction(g_js_ctx, js_get_text, "getText", 1));
    JS_SetPropertyStr(g_js_ctx, yui_obj, "getProperty", JS_NewCFunction(g_js_ctx, js_get_property, "getProperty", 2));
    JS_SetPropertyStr(g_js_ctx, yui_obj, "setProperty", JS_NewCFunction(g_js_ctx, js_set_property, "setProperty", 3));

    JS_SetPropertyStr(g_js_ctx, yui_obj, "setBgColor", JS_NewCFunction(g_js_ctx, js_set_bg_color, "setBgColor", 2));
    JS_SetPropertyStr(g_js_ctx, yui_obj, "hide", JS_NewCFunction(g_js_ctx, js_hide, "hide", 1));
    JS_SetPropertyStr(g_js_ctx, yui_obj, "show", JS_NewCFunction(g_js_ctx, js_show, "show", 1));
    JS_SetPropertyStr(g_js_ctx, yui_obj, "renderFromJson", JS_NewCFunction(g_js_ctx, js_render_from_json, "renderFromJson", 3));
    JS_SetPropertyStr(g_js_ctx, yui_obj, "update", JS_NewCFunction(g_js_ctx, js_update, "update", 1));
    JS_SetPropertyStr(g_js_ctx, yui_obj, "log", JS_NewCFunction(g_js_ctx, js_log, "log", 1));
    JS_SetPropertyStr(g_js_ctx, yui_obj, "themeLoad", JS_NewCFunction(g_js_ctx, js_theme_load, "themeLoad", 1));
    JS_SetPropertyStr(g_js_ctx, yui_obj, "themeSetCurrent", JS_NewCFunction(g_js_ctx, js_theme_set_current, "themeSetCurrent", 1));
    JS_SetPropertyStr(g_js_ctx, yui_obj, "themeUnload", JS_NewCFunction(g_js_ctx, js_theme_unload, "themeUnload", 1));
    JS_SetPropertyStr(g_js_ctx, yui_obj, "themeApplyToTree", JS_NewCFunction(g_js_ctx, js_theme_apply_to_tree, "themeApplyToTree", 0));
    JS_SetPropertyStr(g_js_ctx, yui_obj, "themeUnload", JS_NewCFunction(g_js_ctx, js_theme_unload, "themeUnload", 1));

    JS_SetPropertyStr(g_js_ctx, yui_obj, "readFile", JS_NewCFunction(g_js_ctx, js_read_file, "readFile", 1));
    JS_SetPropertyStr(g_js_ctx, yui_obj, "writeFile", JS_NewCFunction(g_js_ctx, js_write_file, "writeFile", 2));
    JS_SetPropertyStr(g_js_ctx, yui_obj, "screenshot", JS_NewCFunction(g_js_ctx, js_screenshot, "screenshot", 1));
    JS_SetPropertyStr(g_js_ctx, yui_obj, "listDir", JS_NewCFunction(g_js_ctx, js_list_dir, "listDir", 1));
    JS_SetPropertyStr(g_js_ctx, yui_obj, "focus", JS_NewCFunction(g_js_ctx, js_focus, "focus", 1));

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

    // 创建 yui 小写别名（指向同一个 YUI 对象）
    // 必须 DupValue 因为 JS_SetPropertyStr 会 steal 引用
    JS_DupValue(g_js_ctx, yui_obj);
    JS_SetPropertyStr(g_js_ctx, global_obj, "yui", yui_obj);
    printf("JS(QuickJS): Created 'yui' alias for 'YUI'\n");
    
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

    js_timer_register_globals(g_js_ctx);

    // 注册 print 到全局对象（与 log 功能相同）
    JS_SetPropertyStr(g_js_ctx, global_obj, "print", JS_NewCFunction(g_js_ctx, js_log, "print", 1));

    // 注册 YUI.call 桥接（C 事件处理器调用入口）
    JS_SetPropertyStr(g_js_ctx, yui_obj, "call", JS_NewCFunction(g_js_ctx, js_native_call, "call", 2));
    printf("JS(QuickJS): Registered YUI.call bridge\n");

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
static int is_js_identifier_name(const char* name)
{
    if (!name || !name[0]) return 0;
    if (!isalpha((unsigned char)name[0]) && name[0] != '_') return 0;
    for (const char* p = name + 1; *p; ++p) {
        if (!isalnum((unsigned char)*p) && *p != '_') return 0;
    }
    return 1;
}

static int call_js_function_value(JSContext* ctx, JSValue func, const char* event_name, Layer* layer)
{
    const TouchEvent* touch = get_current_touch_event();
    JSValue result;
    if (touch) {
        JSValue args[3];
        args[0] = JS_NewString(ctx, touch_type_to_string(touch->type));
        args[1] = JS_NewInt32(ctx, touch->deltaX);
        args[2] = JS_NewInt32(ctx, touch->deltaY);
        result = JS_Call(ctx, func, JS_UNDEFINED, 3, args);
        JS_FreeValue(ctx, args[0]);
        JS_FreeValue(ctx, args[1]);
        JS_FreeValue(ctx, args[2]);
    } else {
        JSValue args[1];
        args[0] = layer ? JS_NewString(ctx, layer->id) : JS_NULL;
        result = JS_Call(ctx, func, JS_UNDEFINED, 1, args);
        JS_FreeValue(ctx, args[0]);
    }

    if (JS_IsException(result)) {
        JSValue exc = JS_GetException(ctx);
        char err_prefix[256];
        snprintf(err_prefix, sizeof(err_prefix), "event '%s'", event_name ? event_name : "<unknown>");
        print_quickjs_exception(ctx, exc, err_prefix);
        JS_FreeValue(ctx, exc);
        JS_FreeValue(ctx, result);
        return -1;
    }

    JS_FreeValue(ctx, result);
    return 0;
}

int js_module_call_event(const char* event_name, Layer* layer)
{
    if (!g_js_ctx || !event_name) return -1;

    // @ 前缀表示函数名引用，否则是内联 JS 代码
    if (event_name[0] == '@') {
        // 函数名引用：移除 @ 前缀后查找全局函数并调用
        const char* func_name = event_name + 1;
        // 获取全局对象
        JSValue global_obj = JS_GetGlobalObject(g_js_ctx);
        JSValue func = JS_GetPropertyStr(g_js_ctx, global_obj, func_name);

        if (JS_IsUndefined(func) || JS_IsNull(func)) {
            JS_FreeValue(g_js_ctx, global_obj);
            JS_FreeValue(g_js_ctx, func);
            // 全局函数不存在时，回退到事件映射表查找
            return js_module_trigger_event(func_name, layer);
        }

        if (!JS_IsFunction(g_js_ctx, func)) {
            JS_FreeValue(g_js_ctx, global_obj);
            JS_FreeValue(g_js_ctx, func);
            return js_module_trigger_event(func_name, layer);
        }

        int ret = call_js_function_value(g_js_ctx, func, event_name, layer);
        JS_FreeValue(g_js_ctx, global_obj);
        JS_FreeValue(g_js_ctx, func);
        return ret;
    }

    if (is_js_identifier_name(event_name)) {
        JSValue global_obj = JS_GetGlobalObject(g_js_ctx);
        JSValue func = JS_GetPropertyStr(g_js_ctx, global_obj, event_name);
        if (JS_IsFunction(g_js_ctx, func)) {
            int ret = call_js_function_value(g_js_ctx, func, event_name, layer);
            JS_FreeValue(g_js_ctx, global_obj);
            JS_FreeValue(g_js_ctx, func);
            return ret;
        }
        JS_FreeValue(g_js_ctx, global_obj);
        JS_FreeValue(g_js_ctx, func);
    }

    // 内联 JS 代码：直接 eval 执行
    const char* eval_src = event_name;
    char* wrapped = NULL;
    size_t eval_len = strlen(event_name);

    if (strncmp(event_name, "function", 8) == 0) {
        wrapped = malloc(eval_len + 3);
        if (!wrapped) return -1;
        wrapped[0] = '(';
        memcpy(wrapped + 1, event_name, eval_len);
        wrapped[1 + eval_len] = ')';
        wrapped[2 + eval_len] = '\0';
        eval_src = wrapped;
        eval_len += 2;
    }

    JSValue val = JS_Eval(g_js_ctx, eval_src, eval_len, "<event>", JS_EVAL_TYPE_GLOBAL);
    if (wrapped) free(wrapped);

    if (JS_IsException(val)) {
        JSValue exc = JS_GetException(g_js_ctx);
        print_quickjs_exception(g_js_ctx, exc, "<event>");
        JS_FreeValue(g_js_ctx, exc);
        JS_FreeValue(g_js_ctx, val);
        return -1;
    }

    // 如果求值结果是函数则调用它
    if (JS_IsFunction(g_js_ctx, val)) {
        const TouchEvent* touch = get_current_touch_event();
        JSValue result;
        if (touch) {
            JSValue args[3];
            args[0] = JS_NewString(g_js_ctx, touch_type_to_string(touch->type));
            args[1] = JS_NewInt32(g_js_ctx, touch->deltaX);
            args[2] = JS_NewInt32(g_js_ctx, touch->deltaY);
            result = JS_Call(g_js_ctx, val, JS_UNDEFINED, 3, args);
            JS_FreeValue(g_js_ctx, args[0]);
            JS_FreeValue(g_js_ctx, args[1]);
            JS_FreeValue(g_js_ctx, args[2]);
        } else {
            JSValue args[1];
            args[0] = layer ? JS_NewString(g_js_ctx, layer->id) : JS_NULL;
            result = JS_Call(g_js_ctx, val, JS_UNDEFINED, 1, args);
            JS_FreeValue(g_js_ctx, args[0]);
        }

        if (JS_IsException(result)) {
            JSValue exc = JS_GetException(g_js_ctx);
            print_quickjs_exception(g_js_ctx, exc, "<event>");
            JS_FreeValue(g_js_ctx, exc);
            JS_FreeValue(g_js_ctx, result);
            JS_FreeValue(g_js_ctx, val);
            return -1;
        }
        JS_FreeValue(g_js_ctx, result);
    }
    JS_FreeValue(g_js_ctx, val);
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

// 文件写入函数绑定
JSValue js_write_file(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    if (argc < 2) {
        return JS_ThrowTypeError(ctx, "Expected 2 arguments: file_path, content");
    }

    const char* file_path = JS_ToCString(ctx, argv[0]);
    if (!file_path) {
        return JS_ThrowTypeError(ctx, "Invalid file path");
    }

    const char* content = JS_ToCString(ctx, argv[1]);
    if (!content) {
        JS_FreeCString(ctx, file_path);
        return JS_ThrowTypeError(ctx, "Invalid content");
    }

    FILE* f = fopen(file_path, "w");
    if (!f) {
        JS_FreeCString(ctx, file_path);
        JS_FreeCString(ctx, content);
        return JS_FALSE;
    }

    fwrite(content, 1, strlen(content), f);
    fclose(f);

    JS_FreeCString(ctx, file_path);
    JS_FreeCString(ctx, content);
    return JS_TRUE;
}

JSValue js_screenshot(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    if (argc < 1) {
        return JS_ThrowTypeError(ctx, "Expected 1 argument: file_path");
    }

    const char* file_path = JS_ToCString(ctx, argv[0]);
    if (!file_path) {
        return JS_ThrowTypeError(ctx, "Invalid file path");
    }

    int rc = backend_screenshot(file_path);
    JS_FreeCString(ctx, file_path);
    return JS_NewInt32(ctx, rc);
}

// 列出目录内容
JSValue js_list_dir(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    const char* dir_path = ".";
    int need_free = 0;

    if (argc >= 1) {
        dir_path = JS_ToCString(ctx, argv[0]);
        if (!dir_path) dir_path = ".";
        else need_free = 1;
    }

    DIR* dir = opendir(dir_path);
    if (!dir) {
        if (need_free) JS_FreeCString(ctx, dir_path);
        return JS_NULL;
    }

    JSValue result = JS_NewArray(ctx);
    int idx = 0;
    struct dirent* entry;

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        JSValue item = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, item, "name", JS_NewString(ctx, entry->d_name));

        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);
        struct stat st;
        int is_dir = (stat(full_path, &st) == 0 && S_ISDIR(st.st_mode)) ? 1 : 0;
        JS_SetPropertyStr(ctx, item, "isDir", JS_NewBool(ctx, is_dir));

        JS_SetPropertyUint32(ctx, result, idx++, item);
    }

    closedir(dir);
    if (need_free) JS_FreeCString(ctx, dir_path);
    return result;
}

JSValue js_focus(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    if (argc < 1 || !g_layer_root) return JS_UNDEFINED;

    const char* layer_id = JS_ToCString(ctx, argv[0]);
    if (!layer_id) return JS_UNDEFINED;

    Layer* layer = find_layer_by_id(g_layer_root, layer_id);
    JS_FreeCString(ctx, layer_id);

    if (!layer) return JS_UNDEFINED;

    // 清除旧焦点图层状态
    extern Layer* focused_layer;
    if (focused_layer && focused_layer != layer) {
        CLEAR_STATE(focused_layer, LAYER_STATE_FOCUSED);
    }

    // 设置新焦点
    focused_layer = layer;
    SET_STATE(layer, LAYER_STATE_FOCUSED);

    return JS_UNDEFINED;
}