#include "js_module.h"


#include "mquickjs.h"
#include "../../src/ytype.h"

#include "event.h"
#define CONFIG_CLASS_SOCKET
#include "js_socket.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Layer 结构的最小定义（只使用我们需要的字段）
#define MAX_TEXT 256
#define MAX_PATH 1024

// 全局 JS 上下文
static JSContext* g_js_ctx = NULL;
static uint8_t* g_js_mem = NULL;
static size_t g_js_mem_size = 256 * 1024; // 256KB 内存

// 全局 UI 根图层
extern struct Layer* g_layer_root;


static void check_timers(void);


extern const JSSTDLibraryDef js_yuistdlib;
  
extern uint8_t* load_file(const char *filename, int *plen);
extern int hex_to_int(char c);


// 初始化 JS 引擎
int js_module_init(void)
{
    printf("JS: Initializing JavaScript engine...\n");

    g_js_mem = malloc(g_js_mem_size);
    if (!g_js_mem) {
        fprintf(stderr, "JS: Failed to allocate memory\n");
        return -1;
    }

    g_js_ctx = JS_NewContext(g_js_mem, g_js_mem_size, &js_yuistdlib);
    if (!g_js_ctx) {
        fprintf(stderr, "JS: Failed to create context\n");
        free(g_js_mem);
        return -1;
    }

    js_module_register_api();

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

// 统一的 Socket API 注册函数
#ifdef CONFIG_CLASS_SOCKET
extern void js_module_register_socket_api(JSContext* ctx);
#endif
extern void js_module_register_yui_api(JSContext* ctx);

// 注册 C API 到 JS
void js_module_register_api(void)
{
    if (!g_js_ctx) return;
    
    // YUI API 
    js_module_register_yui_api(g_js_ctx);
    
#ifdef CONFIG_CLASS_SOCKET
    // 调用统一的 Socket API 注册函数
    js_module_register_socket_api(g_js_ctx);
#endif
    
    printf("JS(Socket): Registered API function\n");
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
        JS_PrintValueF(g_js_ctx, exc, JS_DUMP_LONG);
        printf("\n");
        return -1;
    }

    printf("JS: Successfully loaded %s\n", filename);
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

    JSValue global_obj = JS_GetGlobalObject(g_js_ctx);
    JSValue func = JS_GetPropertyStr(g_js_ctx, global_obj, func_name);

    if (JS_IsUndefined(func) || !JS_IsFunction(g_js_ctx, func)) {
        return -1;
    }

    // 调用函数
    if (JS_StackCheck(g_js_ctx, 3)) {
        return -1;
    }

    JSValue layer_id_val = JS_NULL;
    if (layer) {
        layer_id_val = JS_NewString(g_js_ctx, layer->id);
    }

    // 按照正确的顺序 push: 参数 -> 函数 -> this
    JS_PushArg(g_js_ctx, layer_id_val); // 参数
    JS_PushArg(g_js_ctx, func); // 函数对象
    JS_PushArg(g_js_ctx, JS_NULL); // this
    JSValue result = JS_Call(g_js_ctx, 1); // 1 个参数

    if (JS_IsException(result)) {
        JSValue exc = JS_GetException(g_js_ctx);
        fprintf(stderr, "JS: Error calling event %s:\n", event_name);
        JS_PrintValueF(g_js_ctx, exc, JS_DUMP_LONG);
        fprintf(stderr, "\n");
        return -1;
    }

    return 0;
}


// 检查并触发定时器（内部静态函数）
static void check_timers(void)
{
    if (!g_js_ctx) return;
}

