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
#ifndef YUI_MAX_PATH
#define YUI_MAX_PATH 1024
#endif

// 全局 JS 上下文
static JSContext* g_js_ctx = NULL;
static uint8_t* g_js_mem = NULL;
#ifndef JS_MEM_POOL_SIZE
#define JS_MEM_POOL_SIZE (200 * 1024)
#endif
static size_t g_js_mem_size = JS_MEM_POOL_SIZE;

// 全局 UI 根图层
extern struct Layer* g_layer_root;


static void check_timers(void);
void js_module_pump_timers(void);

extern void backend_register_update_callback(void (*callback)(void));


extern const JSSTDLibraryDef js_yuistdlib;
  
extern uint8_t* load_file(const char *filename, int *plen);
extern int hex_to_int(char c);


// 初始化 JS 引擎
int js_module_init(void)
{
    printf("JS: Initializing JavaScript engine...\n");

#if defined(YUI_ESP_PLATFORM)
    extern size_t heap_caps_get_free_size(int caps);
    extern size_t heap_caps_get_largest_free_block(int caps);
    printf("JS: heap free=%u largest=%u\n",
           (unsigned)heap_caps_get_free_size(4),
           (unsigned)heap_caps_get_largest_free_block(4));
#endif

    g_js_mem = malloc(g_js_mem_size);
    if (!g_js_mem) {
        fprintf(stderr, "JS: Failed to allocate memory (%u bytes)\n", (unsigned)g_js_mem_size);
        return -1;
    }
    printf("JS: JS_MEM_POOL_SIZE=%u bytes\n", (unsigned)g_js_mem_size);
#if defined(YUI_ESP_PLATFORM)
    printf("JS: heap free after pool=%u largest=%u\n",
           (unsigned)heap_caps_get_free_size(4),
           (unsigned)heap_caps_get_largest_free_block(4));
#endif

    g_js_ctx = JS_NewContext(g_js_mem, g_js_mem_size, &js_yuistdlib);
    if (!g_js_ctx) {
        fprintf(stderr, "JS: Failed to create context\n");
        free(g_js_mem);
        return -1;
    }

    js_module_register_api();
    js_module_init_layer_lifecycle();

    /* 驱动 setTimeout/clearTimeout：注册到 backend 每帧 update 回调 */
    backend_register_update_callback(js_module_pump_timers);

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

    printf("JS: Successfully loaded %s, len=%d\n", filename, len);
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

    /* 拷到栈上：event_name / layer->id 常指向 Layer 内嵌字段；
     * GetProperty/Call 可能触发 mquickjs GC，踩坏相邻堆（曾见名变成 "ormal"）。 */
    char func_name_buf[128];
    char layer_id_buf[64];
    const char* src = event_name[0] == '@' ? event_name + 1 : event_name;
    strncpy(func_name_buf, src, sizeof(func_name_buf) - 1);
    func_name_buf[sizeof(func_name_buf) - 1] = '\0';

    layer_id_buf[0] = '\0';
    if (layer && layer->id[0]) {
        strncpy(layer_id_buf, layer->id, sizeof(layer_id_buf) - 1);
        layer_id_buf[sizeof(layer_id_buf) - 1] = '\0';
    }

    const PointerEvent* pe = get_current_pointer_event();
    int is_gesture = event_name_is_layer_touch(layer, event_name) && pe != NULL;
    int argc = is_gesture ? 2 : 1;

    JSValue global_obj = JS_GetGlobalObject(g_js_ctx);
    JSValue func = JS_GetPropertyStr(g_js_ctx, global_obj, func_name_buf);

    if (JS_IsUndefined(func) || !JS_IsFunction(g_js_ctx, func)) {
        printf("JS: call_event '%s' not on global, try map/eval\n", func_name_buf);
        JS_FreeValue(g_js_ctx, global_obj);
        JS_FreeValue(g_js_ctx, func);

        if (js_module_trigger_event(func_name_buf, layer) == 0) {
            return 0;
        }

        /* mquickjs：部分 function 声明对 GetProperty 不可见，用 eval 按名调用 */
        char expr[256];
        if (layer_id_buf[0]) {
            snprintf(expr, sizeof(expr),
                     "(function(){if(typeof %s!=='function')throw new Error('no %s');%s('%s');})()",
                     func_name_buf, func_name_buf, func_name_buf, layer_id_buf);
        } else {
            snprintf(expr, sizeof(expr),
                     "(function(){if(typeof %s!=='function')throw new Error('no %s');%s();})()",
                     func_name_buf, func_name_buf, func_name_buf);
        }
        JSValue v = JS_Eval(g_js_ctx, expr, strlen(expr), "<call_event>", 0);
        if (JS_IsException(v)) {
            JSValue exc = JS_GetException(g_js_ctx);
            printf("JS: Eval call %s failed: ", func_name_buf);
            JS_PrintValueF(g_js_ctx, exc, JS_DUMP_LONG);
            printf("\n");
            return -1;
        }
        return 0;
    }

    if (JS_StackCheck(g_js_ctx, (uint32_t)(argc + 2))) {
        printf("JS: call_event '%s' stack/oom\n", func_name_buf);
        JS_FreeValue(g_js_ctx, global_obj);
        JS_FreeValue(g_js_ctx, func);
        return -1;
    }

    JSValue layer_id_val = layer_id_buf[0] ? JS_NewString(g_js_ctx, layer_id_buf) : JS_NULL;

    if (is_gesture) {
        JS_PushArg(g_js_ctx, js_make_pointer_event_object(g_js_ctx, pe));
    }
    JS_PushArg(g_js_ctx, layer_id_val);
    JS_PushArg(g_js_ctx, func);
    JS_PushArg(g_js_ctx, JS_NULL);
    JSValue result = JS_Call(g_js_ctx, argc);

    JS_FreeValue(g_js_ctx, global_obj);

    if (JS_IsException(result)) {
        JSValue exc = JS_GetException(g_js_ctx);
        printf("JS: Error calling event %s:\n", func_name_buf);
        JS_PrintValueF(g_js_ctx, exc, JS_DUMP_LONG);
        printf("\n");
        return -1;
    }

    return 0;
}


// 检查并触发定时器（内部静态函数）
static int g_pump_calls = 0;
static void check_timers(void)
{
    if (!g_js_ctx) return;
    if (g_pump_calls++ < 3) printf("JS: pump_timers NODEBUG\n");
    js_run_timers(g_js_ctx);
}

// 供 UI 主循环周期性调用，驱动 setTimeout/setInterval
void js_module_pump_timers(void)
{
    check_timers();
}
