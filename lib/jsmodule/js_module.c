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
    js_module_init_layer_lifecycle();

    printf("JS: JavaScript engine initialized\n");
    return 0;
}

// 清理 JS 引擎
void js_module_cleanup(void)
{
    if (g_layer_root) {
        js_module_shutdown();
    }
    g_layer_root = NULL;

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
// onTouch: (layerId, event)  event = { type, deltaX, deltaY, pointerId, fingerCount, x, y }
// 其它:    (layerId)
static int event_name_is_layer_touch(const Layer* layer, const char* event_name)
{
    const char* tn;
    const char* en;
    if (!layer || !layer->event || !event_name || !event_name[0]) {
        return 0;
    }
    tn = layer->event->touch_name;
    if (!tn[0]) {
        return 0;
    }
    if (strcmp(tn, event_name) == 0) {
        return 1;
    }
    en = event_name[0] == '@' ? event_name + 1 : event_name;
    tn = tn[0] == '@' ? tn + 1 : tn;
    return strcmp(tn, en) == 0;
}

static JSValue js_make_pointer_event_object(JSContext* ctx, const PointerEvent* pe)
{
    JSValue ev = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, ev, "type",
                      JS_NewString(ctx, pointer_phase_to_string(pe->phase)));
    JS_SetPropertyStr(ctx, ev, "deltaX", JS_NewInt32(ctx, pe->delta_x));
    JS_SetPropertyStr(ctx, ev, "deltaY", JS_NewInt32(ctx, pe->delta_y));
    JS_SetPropertyStr(ctx, ev, "pointerId", JS_NewInt32(ctx, pe->pointer_id));
    JS_SetPropertyStr(ctx, ev, "fingerCount",
                      JS_NewInt32(ctx, pe->device == POINTER_DEVICE_TOUCH
                                          ? pe->finger_count
                                          : 1));
    JS_SetPropertyStr(ctx, ev, "x", JS_NewInt32(ctx, pe->x));
    JS_SetPropertyStr(ctx, ev, "y", JS_NewInt32(ctx, pe->y));
    return ev;
}

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
        JS_FreeValue(g_js_ctx, global_obj);
        JS_FreeValue(g_js_ctx, func);
        // 全局函数不存在时，回退到事件映射表查找
        return js_module_trigger_event(func_name, layer);
    }

    const PointerEvent* pe = get_current_pointer_event();
    int is_gesture = event_name_is_layer_touch(layer, event_name) && pe != NULL;
    int argc = is_gesture ? 2 : 1;

    if (JS_StackCheck(g_js_ctx, (uint32_t)(argc + 2))) {
        return -1;
    }

    JSValue layer_id_val = layer ? JS_NewString(g_js_ctx, layer->id) : JS_NULL;

    /* Push order: arg[n-1] ... arg[0], func, this */
    if (is_gesture) {
        JS_PushArg(g_js_ctx, js_make_pointer_event_object(g_js_ctx, pe));
    }
    JS_PushArg(g_js_ctx, layer_id_val);
    JS_PushArg(g_js_ctx, func);
    JS_PushArg(g_js_ctx, JS_NULL);
    JSValue result = JS_Call(g_js_ctx, argc);

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

