// test_js_timer.c
// mquickjs setTimeout 驱动测试：
// 验证 js_module_pump_timers() 能驱动 setTimeout 回调执行。
// 依赖 jsmodule-mquickjs（js_module.h / js_run_timers）。
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <cmocka.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "js_module.h"
#include "mquickjs.h"

int main(int argc, char **argv);

#if defined(_WIN32)
int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    (void)hInstance;
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;
    return main(__argc, __argv);
}
#endif

/* 供 JS 侧回调写入的全局标记（YUI 对象经 yui_stdlib 暴露给 JS） */

static const char *kTestScript =
    "var __timerFired = 42;\n"
    "setTimeout(function(){ __timerFired = 99; }, 30);\n"
    "var __timerFired2 = 7;\n"
    "setTimeout(function(){ __timerFired2 = 77; }, 1000);\n";

static int write_script(const char *path, const char *src)
{
    FILE *f = fopen(path, "wb");
    if (!f) {
        return -1;
    }
    fwrite(src, 1, strlen(src), f);
    fclose(f);
    return 0;
}

/* 从 JS 侧读取全局变量（由 JS 写回 globalThis） */
static int read_global_int(JSContext *ctx, const char *name)
{
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue v = JS_GetPropertyStr(ctx, global, name);
    JS_FreeValue(ctx, global);
    if (JS_IsException(v)) {
        JS_GetException(ctx);
        return -1;
    }
    int val = 0;
    if (JS_ToInt32(ctx, &val, v) != 0) {
        return -1;
    }
    return val;
}

static void test_settimeout_fires(void **state)
{
    const char *script_path = "build/js_timer_test.js";
    int fired;
    int i;

    (void)state;
    assert_int_equal(write_script(script_path, kTestScript), 0);

    assert_int_equal(js_module_init(), 0);

    /* JS 单线程顺序执行：脚本顶层 __timerFired=42 */
    assert_int_equal(js_module_load_file(script_path), 0);
    assert_int_equal(read_global_int((JSContext *)js_module_get_context(), "__timerFired"), 42);

    /* 30ms 定时器：轮询 pump 直到回调执行（__timerFired 变 99）或超时（5s）。
     * 注意：pump 依赖真实时钟，循环必须 sleep 让时间推进。 */
    fired = -1;
    for (i = 0; i < 500; i++) { /* 500 * 10ms = 5s */
        js_module_pump_timers();
        fired = read_global_int((JSContext *)js_module_get_context(), "__timerFired");
        if (fired == 99) {
            break;
        }
#ifdef _WIN32
        Sleep(10);
#else
        usleep(10000);
#endif
    }

    assert_int_equal(fired, 99);

    js_module_cleanup();
    remove(script_path);
}

static void test_settimeout_respects_delay(void **state)
{
    const char *script_path = "build/js_timer_test2.js";
    int fired2;
    int i;

    (void)state;
    assert_int_equal(write_script(script_path, kTestScript), 0);

    assert_int_equal(js_module_init(), 0);
    assert_int_equal(js_module_load_file(script_path), 0);
    assert_int_equal(read_global_int((JSContext *)js_module_get_context(), "__timerFired2"), 7);

    /* 1000ms 定时器：在 500ms 窗口内不应触发（仍为 7，非 77） */
    fired2 = -1;
    for (i = 0; i < 50; i++) { /* 50 * 10ms = 0.5s 窗口，短于 1000ms */
        js_module_pump_timers();
        fired2 = read_global_int((JSContext *)js_module_get_context(), "__timerFired2");
        if (fired2 == 77) {
            break;
        }
#ifdef _WIN32
        Sleep(10);
#else
        usleep(10000);
#endif
    }
    assert_int_not_equal(fired2, 77);

    js_module_cleanup();
    remove(script_path);
}

int main(int argc, char **argv)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_settimeout_fires),
        cmocka_unit_test(test_settimeout_respects_delay),
    };
    (void)argc;
    (void)argv;
    return cmocka_run_group_tests(tests, NULL, NULL);
}
