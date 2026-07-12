#include "js_timer.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#include <time.h>
#endif

#define MAX_JS_TIMERS 64

typedef struct {
    int active;
    int64_t expire_ms;
    JSValue func;
} JsTimerEntry;

static JsTimerEntry g_js_timers[MAX_JS_TIMERS];
static int g_js_timer_seq = 0;

static int64_t js_timer_now_ms(void)
{
#ifdef _WIN32
    return (int64_t)GetTickCount64();
#else
#if defined(__linux__) || defined(__APPLE__)
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
#endif
#endif
}

static void js_timer_dump_exception(JSContext* ctx)
{
    JSValue exc = JS_GetException(ctx);
    JSValue msg_val = JS_GetPropertyStr(ctx, exc, "message");
    const char* msg = JS_ToCString(ctx, msg_val);
    if (msg) {
        printf("JS(QuickJS): Timer callback error: %s\n", msg);
        JS_FreeCString(ctx, msg);
    }
    JS_FreeValue(ctx, msg_val);
    JS_FreeValue(ctx, exc);
}

static int js_timer_alloc_slot(void)
{
    for (int i = 0; i < MAX_JS_TIMERS; i++) {
        int slot = (g_js_timer_seq + i) % MAX_JS_TIMERS;
        if (!g_js_timers[slot].active) {
            g_js_timer_seq = (slot + 1) % MAX_JS_TIMERS;
            return slot;
        }
    }
    return -1;
}

JSValue js_qjs_setTimeout(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv)
{
    (void)this_val;
    if (argc < 2) {
        return JS_ThrowTypeError(ctx, "setTimeout expects (function, delay)");
    }
    if (!JS_IsFunction(ctx, argv[0])) {
        return JS_ThrowTypeError(ctx, "not a function");
    }

    int32_t delay = 0;
    if (JS_ToInt32(ctx, &delay, argv[1])) {
        return JS_EXCEPTION;
    }
    if (delay < 0) {
        delay = 0;
    }

    int id = js_timer_alloc_slot();
    if (id < 0) {
        return JS_ThrowInternalError(ctx, "too many timers");
    }

    g_js_timers[id].active = 1;
    g_js_timers[id].expire_ms = js_timer_now_ms() + delay;
    g_js_timers[id].func = JS_DupValue(ctx, argv[0]);
    return JS_NewInt32(ctx, id);
}

JSValue js_qjs_clearTimeout(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv)
{
    (void)this_val;
    if (argc < 1) {
        return JS_UNDEFINED;
    }

    int32_t id = -1;
    if (JS_ToInt32(ctx, &id, argv[0])) {
        return JS_EXCEPTION;
    }

    if (id >= 0 && id < MAX_JS_TIMERS && g_js_timers[id].active) {
        JS_FreeValue(ctx, g_js_timers[id].func);
        memset(&g_js_timers[id], 0, sizeof(g_js_timers[id]));
    }
    return JS_UNDEFINED;
}

void js_timer_register_globals(JSContext* ctx)
{
    JSValue global = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, global, "setTimeout",
                      JS_NewCFunction(ctx, js_qjs_setTimeout, "setTimeout", 2));
    JS_SetPropertyStr(ctx, global, "clearTimeout",
                      JS_NewCFunction(ctx, js_qjs_clearTimeout, "clearTimeout", 1));
    JS_FreeValue(ctx, global);
}

void js_timer_run(JSContext* ctx)
{
    if (!ctx) {
        return;
    }

    int64_t now = js_timer_now_ms();
    for (int i = 0; i < MAX_JS_TIMERS; i++) {
        if (!g_js_timers[i].active) {
            continue;
        }
        if (g_js_timers[i].expire_ms > now) {
            continue;
        }

        JSValue func = JS_DupValue(ctx, g_js_timers[i].func);
        g_js_timers[i].active = 0;
        JS_FreeValue(ctx, g_js_timers[i].func);
        memset(&g_js_timers[i], 0, sizeof(g_js_timers[i]));

        JSValue ret = JS_Call(ctx, func, JS_UNDEFINED, 0, NULL);
        JS_FreeValue(ctx, func);
        if (JS_IsException(ret)) {
            js_timer_dump_exception(ctx);
        }
        JS_FreeValue(ctx, ret);
    }
}

void js_timer_clear_all(JSContext* ctx)
{
    if (!ctx) {
        memset(g_js_timers, 0, sizeof(g_js_timers));
        g_js_timer_seq = 0;
        return;
    }

    for (int i = 0; i < MAX_JS_TIMERS; i++) {
        if (g_js_timers[i].active) {
            JS_FreeValue(ctx, g_js_timers[i].func);
        }
    }
    memset(g_js_timers, 0, sizeof(g_js_timers));
    g_js_timer_seq = 0;
}
