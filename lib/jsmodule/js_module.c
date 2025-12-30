#include "js_module.h"

#ifdef HAS_JS_MODULE
#include "mquickjs.h"
#include "../cjson/cJSON.h"

// mquickjs 不提供 JS_IsArray 函数，使用 JS_GetClassID 宏来定义
#define JS_IsArray(val) (JS_GetClassID(g_js_ctx, val) == 4) // JS_CLASS_ARRAY = 4

// 前向声明 backend 函数（非侵入式调用）
typedef void (*UpdateCallback)(void);
extern void backend_register_update_callback(UpdateCallback callback);
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Layer 结构的最小定义（只使用我们需要的字段）
#define MAX_TEXT 256
#define MAX_PATH 1024

// 前向声明 Layer 结构
struct Layer {
    char id[50];
    char text[MAX_TEXT];
    int visible;
    struct {
        unsigned char r, g, b, a;
    } bg_color;
};

// 查找图层的函数指针类型
typedef struct Layer* (*FindLayerFunc)(struct Layer* root, const char* id);
static FindLayerFunc g_find_layer_func = NULL;


extern const JSSTDLibraryDef js_stdlib;

// 注册查找图层函数
void js_module_set_find_layer_func(struct Layer* (*func)(struct Layer* root, const char* id)) {
    g_find_layer_func = func;
}

// 全局 JS 上下文
static JSContext* g_js_ctx = NULL;
static uint8_t* g_js_mem = NULL;
static size_t g_js_mem_size = 256 * 1024; // 256KB 内存

// 全局 UI 根图层
static struct Layer* g_ui_root = NULL;


static void check_timers(void);
// ====================== 辅助函数 ======================

int hex_to_int(char c){
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}


// 日期和时间 API
static JSValue js_date_now(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    time_t now = time(NULL);
    return JS_NewInt64(ctx, (int64_t)(now * 1000));
}

// Math.random
static JSValue js_math_random(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    return JS_NewFloat64(ctx, (double)rand() / (double)RAND_MAX);
}

// Math.floor
static JSValue js_math_floor(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    if (argc < 1) return JS_UNDEFINED;
    double val = 0;
    if (JS_ToNumber(ctx, &val, argv[0]) == 0) {
        return JS_NewInt32(ctx, (int32_t)val);
    }
    return JS_UNDEFINED;
}

// setTimeout API
static JSValue js_set_timeout(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    if (argc < 2) return JS_UNDEFINED;

    double delay = 0;
    if (JS_ToNumber(ctx, &delay, argv[1]) != 0) {
        return JS_UNDEFINED;
    }

    // 获取全局 timers 数组和计数器
    JSValue global_obj = JS_GetGlobalObject(ctx);
    JSValue timers_arr = JS_GetPropertyStr(ctx, global_obj, "_timers");
    JSValue counter_val = JS_GetPropertyStr(ctx, global_obj, "_timerIdCounter");

    int timer_id = 0;
    if (JS_IsInt(counter_val)) {
        timer_id = JS_VALUE_GET_INT(counter_val);
    }

    // 创建定时器对象
    JSValue timer_obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, timer_obj, "id", JS_NewInt32(ctx, timer_id));
    JS_SetPropertyStr(ctx, timer_obj, "callback", argv[0]);
    JS_SetPropertyStr(ctx, timer_obj, "triggerTime", JS_NewFloat64(ctx, (double)time(NULL) * 1000 + delay));
    JS_SetPropertyStr(ctx, timer_obj, "triggered", JS_FALSE);

    // 添加到 timers 数组
    int len = 0;
    JS_ToInt32(ctx, &len, timers_arr);
    JS_SetPropertyUint32(ctx, timers_arr, len, timer_obj);

    // 更新计数器
    JS_SetPropertyStr(ctx, global_obj, "_timerIdCounter", JS_NewInt32(ctx, timer_id + 1));

    return JS_NewInt32(ctx, timer_id);
}

// ====================== JS API 函数 ======================

// 设置文本
static JSValue js_set_text(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    if (argc < 2) return JS_UNDEFINED;
    
    const char* layer_id = NULL;
    const char* text = NULL;
    
    JSCStringBuf buf1, buf2;
    if (JS_IsString(ctx, argv[0])) {
        layer_id = JS_ToCString(ctx, argv[0], &buf1);
    }
    if (JS_IsString(ctx, argv[1])) {
        text = JS_ToCString(ctx, argv[1], &buf2);
    }
    
    if (layer_id && text && g_ui_root && g_find_layer_func) {
        struct Layer* layer = g_find_layer_func(g_ui_root, layer_id);
        if (layer) {
            strncpy(layer->text, text, MAX_TEXT - 1);
            layer->text[MAX_TEXT - 1] = '\0';
            printf("JS: Set text for layer '%s': %s\n", layer_id, text);
        }
    }
    
    return JS_UNDEFINED;
}

// 获取文本
static JSValue js_get_text(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    if (argc < 1) return JS_UNDEFINED;
    
    const char* layer_id = NULL;
    JSCStringBuf buf;
    if (JS_IsString(ctx, argv[0])) {
        layer_id = JS_ToCString(ctx, argv[0], &buf);
    }
    
    if (layer_id && g_ui_root && g_find_layer_func) {
        struct Layer* layer = g_find_layer_func(g_ui_root, layer_id);
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
    
    const char* layer_id = NULL;
    const char* color_hex = NULL;
    
    JSCStringBuf buf1, buf2;
    if (JS_IsString(ctx, argv[0])) {
        layer_id = JS_ToCString(ctx, argv[0], &buf1);
    }
    if (JS_IsString(ctx, argv[1])) {
        color_hex = JS_ToCString(ctx, argv[1], &buf2);
    }
    
    if (layer_id && color_hex && g_ui_root && g_find_layer_func) {
        struct Layer* layer = g_find_layer_func(g_ui_root, layer_id);
        if (layer) {
            // 解析十六进制颜色 #RRGGBB
            if (strlen(color_hex) == 7 && color_hex[0] == '#') {
                layer->bg_color.r = (hex_to_int(color_hex[1]) * 16 + hex_to_int(color_hex[2]));
                layer->bg_color.g = (hex_to_int(color_hex[3]) * 16 + hex_to_int(color_hex[4]));
                layer->bg_color.b = (hex_to_int(color_hex[5]) * 16 + hex_to_int(color_hex[6]));
                layer->bg_color.a = 255;
                printf("JS: Set bg_color for layer '%s': %s\n", layer_id, color_hex);
            }
        }
    }
    
    return JS_UNDEFINED;
}

// 隐藏图层
static JSValue js_hide(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    if (argc < 1) return JS_UNDEFINED;
    
    const char* layer_id = NULL;
    JSCStringBuf buf;
    if (JS_IsString(ctx, argv[0])) {
        layer_id = JS_ToCString(ctx, argv[0], &buf);
    }
    
    if (layer_id && g_ui_root && g_find_layer_func) {
        struct Layer* layer = g_find_layer_func(g_ui_root, layer_id);
        if (layer) {
            layer->visible = 0; // IN_VISIBLE
            printf("JS: Hide layer '%s'\n", layer_id);
        }
    }
    
    return JS_UNDEFINED;
}

// 显示图层
static JSValue js_show(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    if (argc < 1) return JS_UNDEFINED;
    
    const char* layer_id = NULL;
    JSCStringBuf buf;
    if (JS_IsString(ctx, argv[0])) {
        layer_id = JS_ToCString(ctx, argv[0], &buf);
    }
    
    if (layer_id && g_ui_root && g_find_layer_func) {
        struct Layer* layer = g_find_layer_func(g_ui_root, layer_id);
        if (layer) {
            layer->visible = 1; // VISIBLE
            printf("JS: Show layer '%s'\n", layer_id);
        }
    }
    
    return JS_UNDEFINED;
}

// 打印日志
static JSValue js_log(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    for (int i = 0; i < argc; i++) {
        if (i != 0) printf(" ");
        if (JS_IsString(ctx, argv[i])) {
            JSCStringBuf buf;
            const char* str = JS_ToCString(ctx, argv[i], &buf);
            printf("%s", str);
        } else {
            JS_PrintValue(ctx, argv[i]);
        }
    }
    printf("\n");
    return JS_UNDEFINED;
}

// ====================== 初始化和清理 ======================

// 定义这个宏以避免编译mqjs_stdlib.c中的main函数
#define NO_MAIN
#include "../mquickjs/mqjs_stdlib.c"

static void js_log_func(void *opaque, const void *buf, size_t buf_len)
{
    fwrite(buf, 1, buf_len, stdout);
}

static uint8_t* load_file(const char *filename, int *plen)
{
    FILE *f;
    uint8_t *buf;
    int buf_len;
    
    f = fopen(filename, "rb");
    if (!f) {
        printf("JS: Cannot open file %s\n", filename);
        return NULL;
    }
    
    fseek(f, 0, SEEK_END);
    buf_len = ftell(f);
    fseek(f, 0, SEEK_SET);
    buf = malloc(buf_len + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    fread(buf, 1, buf_len, f);
    buf[buf_len] = '\0';
    fclose(f);
    
    if (plen) *plen = buf_len;
    return buf;
}

// 初始化 JS 引擎
int js_module_init(void)
{
    printf("JS: Initializing JavaScript engine...\n");
    
    g_js_mem = malloc(g_js_mem_size);
    if (!g_js_mem) {
        fprintf(stderr, "JS: Failed to allocate memory\n");
        return -1;
    }
    
    g_js_ctx = JS_NewContext(g_js_mem, g_js_mem_size, &js_stdlib);
    if (!g_js_ctx) {
        fprintf(stderr, "JS: Failed to create context\n");
        free(g_js_mem);
        return -1;
    }
    
    JS_SetLogFunc(g_js_ctx, js_log_func);

    js_module_register_api();

    // 注册更新回调（非侵入式设计）
    backend_register_update_callback(check_timers);

    printf("JS: JavaScript engine initialized\n");
    return 0;
}

// 清理 JS 引擎
void js_module_cleanup(void)
{
    if (g_js_ctx) {
        JS_FreeContext(g_js_ctx);
        g_js_ctx = NULL;
    }
    if (g_js_mem) {
        free(g_js_mem);
        g_js_mem = NULL;
    }
}

// 注册 C API 到 JS
void js_module_register_api(void)
{
    if (!g_js_ctx) return;

    JSValue global_obj = JS_GetGlobalObject(g_js_ctx);

    // 创建 YUI API 对象
    JSValue yui_obj = JS_NewObject(g_js_ctx);

    // 注册所有 API 函数 - 使用 JS_NewCFunctionParams
    JS_SetPropertyStr(g_js_ctx, yui_obj, "setText", JS_NewCFunctionParams(g_js_ctx, (int)js_set_text, JS_NULL));
    JS_SetPropertyStr(g_js_ctx, yui_obj, "getText", JS_NewCFunctionParams(g_js_ctx, (int)js_get_text, JS_NULL));
    JS_SetPropertyStr(g_js_ctx, yui_obj, "setBgColor", JS_NewCFunctionParams(g_js_ctx, (int)js_set_bg_color, JS_NULL));
    JS_SetPropertyStr(g_js_ctx, yui_obj, "hide", JS_NewCFunctionParams(g_js_ctx, (int)js_hide, JS_NULL));
    JS_SetPropertyStr(g_js_ctx, yui_obj, "show", JS_NewCFunctionParams(g_js_ctx, (int)js_show, JS_NULL));
    JS_SetPropertyStr(g_js_ctx, yui_obj, "log", JS_NewCFunctionParams(g_js_ctx, (int)js_log, JS_NULL));

    JS_SetPropertyStr(g_js_ctx, global_obj, "YUI", yui_obj);

    // 添加 Date API
    JSValue date_obj = JS_NewObject(g_js_ctx);
    JS_SetPropertyStr(g_js_ctx, date_obj, "now", JS_NewCFunctionParams(g_js_ctx, (int)js_date_now, JS_NULL));
    JS_SetPropertyStr(g_js_ctx, global_obj, "Date", date_obj);

    // 添加 Math.random
    JSValue math_obj = JS_NewObject(g_js_ctx);
    JS_SetPropertyStr(g_js_ctx, math_obj, "random", JS_NewCFunctionParams(g_js_ctx, (int)js_math_random, JS_NULL));
    JS_SetPropertyStr(g_js_ctx, math_obj, "floor", JS_NewCFunctionParams(g_js_ctx, (int)js_math_floor, JS_NULL));
    JS_SetPropertyStr(g_js_ctx, global_obj, "Math", math_obj);

    // 添加 setTimeout 和 timers 数组到全局
    JSValue timers_obj = JS_NewArray(g_js_ctx, 0);
    JS_SetPropertyStr(g_js_ctx, global_obj, "_timers", timers_obj);
    JS_SetPropertyStr(g_js_ctx, global_obj, "_timerIdCounter", JS_NewInt32(g_js_ctx, 0));
    JS_SetPropertyStr(g_js_ctx, global_obj, "setTimeout", JS_NewCFunctionParams(g_js_ctx, (int)js_set_timeout, JS_NULL));
}

// 加载并执行 JS 文件
int js_module_load_file(const char* filename)
{
    if (!g_js_ctx) {
        fprintf(stderr, "JS: Engine not initialized\n");
        return -1;
    }
    
    printf("JS: Loading file %s...\n", filename);
    
    int len = 0;
    uint8_t* buf = load_file(filename, &len);
    if (!buf) {
        return -1;
    }
    
    JSValue val = JS_Eval(g_js_ctx, (const char*)buf, len, filename, 0);
    free(buf);
    
    if (JS_IsException(val)) {
        JSValue exc = JS_GetException(g_js_ctx);
        fprintf(stderr, "JS: Error executing %s:\n", filename);
        //JS_PrintValue(g_js_ctx, exc);
        JS_PrintValueF(g_js_ctx, exc, JS_DUMP_LONG);
        printf("\n");
        return -1;
    }
    
    printf("JS: Successfully loaded %s\n", filename);
    return 0;
}

// 设置 UI 根图层
void js_module_set_ui_root(Layer* root)
{
    g_ui_root = root;
}

// 辅助函数：获取文件所在的目录
static void get_file_dir(const char* filepath, char* dir, size_t max_len)
{
    if (!filepath || !dir) {
        dir[0] = '\0';
        return;
    }

    // 查找最后一个 '/' 或 '\' 的位置
    const char* last_sep = strrchr(filepath, '/');
    const char* last_sep_win = strrchr(filepath, '\\');

    // 使用最后的分隔符
    const char* sep = (last_sep_win > last_sep) ? last_sep_win : last_sep;

    if (sep) {
        size_t dir_len = sep - filepath;
        if (dir_len >= max_len) dir_len = max_len - 1;
        strncpy(dir, filepath, dir_len);
        dir[dir_len] = '\0';
    } else {
        dir[0] = '.';  // 当前目录
        dir[1] = '\0';
    }
}

// 辅助函数：构建完整的 JS 文件路径（相对于 JSON 文件目录）
static void build_js_path(const char* js_path, const char* json_dir, char* full_path, size_t max_len)
{
    if (js_path[0] == '/' || (js_path[0] == '.' && (js_path[1] == '/' || js_path[1] == '.'))) {
        // 绝对路径或 ./ 开头的路径，直接使用
        strncpy(full_path, js_path, max_len - 1);
    } else {
        // 相对路径，拼接 JSON 文件所在目录
        snprintf(full_path, max_len, "%s/%s", json_dir, js_path);
    }
    full_path[max_len - 1] = '\0';
}

// 递归遍历 JSON 并加载所有 JS 文件
static int load_js_recursive(cJSON* json, const char* json_dir)
{
    if (!json) return 0;

    int loaded_count = 0;

    // 检查当前节点是否有 "js" 字段
    cJSON* js_file = cJSON_GetObjectItem(json, "js");
    if (js_file && cJSON_IsString(js_file)) {
        const char* js_path = js_file->valuestring;
        char full_path[MAX_PATH];
        build_js_path(js_path, json_dir, full_path, MAX_PATH);

        printf("JS: Loading JS file from config: %s -> %s\n", js_path, full_path);
        if (js_module_load_file(full_path) == 0) {
            loaded_count++;
        }
    }

    // 递归遍历子对象
    cJSON* child = json->child;
    while (child) {
        // 递归处理子节点
        loaded_count += load_js_recursive(child, json_dir);
        child = child->next;
    }

    return loaded_count;
}

// 从 JSON 加载 JS 文件（递归遍历整个 JSON 树）
int js_module_load_from_json(cJSON* root_json, const char* json_file_path)
{
    if (!root_json) {
        printf("JS: root_json is NULL\n");
        return 0;
    }

    // 获取 JSON 文件所在目录
    char json_dir[MAX_PATH];
    if (json_file_path && json_file_path[0] != '\0') {
        get_file_dir(json_file_path, json_dir, MAX_PATH);
    } else {
        // 默认目录：app/mquickjs/
        strcpy(json_dir, "app/mquickjs");
    }

    printf("JS: Loading JS from JSON directory: %s\n", json_dir);

    int total_loaded = load_js_recursive(root_json, json_dir);
    printf("JS: Total %d JS file(s) loaded from JSON\n", total_loaded);
    return total_loaded;
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

    JSValue global_obj = JS_GetGlobalObject(g_js_ctx);
    JSValue func = JS_GetPropertyStr(g_js_ctx, global_obj, func_name);

    if (JS_IsUndefined(func) || !JS_IsFunction(g_js_ctx, func)) {
        return -1;
    }

    // 调用函数
    if (JS_StackCheck(g_js_ctx, 1)) {
        return -1;
    }

    JSValue layer_id_val = JS_NULL;
    if (layer) {
        layer_id_val = JS_NewString(g_js_ctx, layer->id);
    }

    JS_PushArg(g_js_ctx, layer_id_val);
    JS_PushArg(g_js_ctx, JS_NULL); // this
    JSValue result = JS_Call(g_js_ctx, 1);

    if (JS_IsException(result)) {
        JSValue exc = JS_GetException(g_js_ctx);
        fprintf(stderr, "JS: Error calling event %s:\n", event_name);
        JS_PrintValue(g_js_ctx, exc);
        fprintf(stderr, "\n");
        return -1;
    }

    return 0;
}

// 检查并触发定时器（内部静态函数）
static void check_timers(void)
{
    if (!g_js_ctx) return;

    time_t now_ms = time(NULL) * 1000;

    JSValue global_obj = JS_GetGlobalObject(g_js_ctx);
    JSValue timers_arr = JS_GetPropertyStr(g_js_ctx, global_obj, "_timers");

    if (!JS_IsArray(timers_arr)) return;

    int len = 0;
    JS_ToInt32(g_js_ctx, &len, timers_arr);

    for (int i = 0; i < len; i++) {
        JSValue timer = JS_GetPropertyUint32(g_js_ctx, timers_arr, i);

        JSValue triggered_val = JS_GetPropertyStr(g_js_ctx, timer, "triggered");
        if (JS_IsBool(triggered_val) && triggered_val == JS_FALSE) {
            JSValue trigger_time_val = JS_GetPropertyStr(g_js_ctx, timer, "triggerTime");
            double trigger_time = 0;
            if (JS_ToNumber(g_js_ctx, &trigger_time, trigger_time_val) == 0) {
                if (now_ms >= trigger_time) {
                    // 触发定时器
                    JSValue callback = JS_GetPropertyStr(g_js_ctx, timer, "callback");

                    if (JS_IsFunction(g_js_ctx, callback)) {
                        if (JS_StackCheck(g_js_ctx, 0)) {
                            continue;
                        }

                        JS_PushArg(g_js_ctx, JS_NULL); // this
                        JSValue result = JS_Call(g_js_ctx, 0);

                        if (JS_IsException(result)) {
                            JSValue exc = JS_GetException(g_js_ctx);
                            fprintf(stderr, "JS: Error in timer callback:\n");
                            JS_PrintValue(g_js_ctx, exc);
                            fprintf(stderr, "\n");
                        }
                    }

                    // 标记为已触发
                    JS_SetPropertyStr(g_js_ctx, timer, "triggered", JS_TRUE);
                }
            }
        }
    }
}

// 更新图层文本
void js_module_update_layer_text(Layer* layer, const char* text)
{
    if (layer && text) {
        strncpy(layer->text, text, MAX_TEXT - 1);
        layer->text[MAX_TEXT - 1] = '\0';
    }
}

// 设置按钮样式
void js_module_set_button_style(Layer* layer, const char* bg_color)
{
    if (layer && bg_color) {
        // 解析十六进制颜色
        if (strlen(bg_color) == 7 && bg_color[0] == '#') {
            layer->bg_color.r = (hex_to_int(bg_color[1]) * 16 + hex_to_int(bg_color[2]));
            layer->bg_color.g = (hex_to_int(bg_color[3]) * 16 + hex_to_int(bg_color[4]));
            layer->bg_color.b = (hex_to_int(bg_color[5]) * 16 + hex_to_int(bg_color[6]));
            layer->bg_color.a = 255;
        }
    }
}
