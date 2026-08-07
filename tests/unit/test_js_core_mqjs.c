// test_js_core.c
// mquickjs 核心 JS 能力测试：
// 语法（闭包/函数）、内置对象（String/Array/Math/JSON/Date）、
// 严格模式行为。JS 脚本将断言结果写入 __r[name]，C 侧逐项校验。
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <cmocka.h>

#include "js_module.h"
#include "mquickjs.h"

int main(int argc, char **argv);

#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif

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

static const char *kTestScript =
    "var __r = {};\n"
    "function T(name, cond) { __r[name] = cond ? 1 : 0; }\n"
    "\n"
    "/* 基础语法 */\n"
    "T('arith', (1 + 2 * 3) === 7);\n"
    "var x = 10;\n"
    "T('var', x === 10);\n"
    "function add(a, b) { return a + b; }\n"
    "T('func', add(2, 3) === 5);\n"
    "var counter = (function () { var n = 0; return function () { return ++n; }; })();\n"
    "T('closure', counter() === 1 && counter() === 2);\n"
    "var s = 'hello';\n"
    "T('ternary', s.length === 5 ? true : false);\n"
    "\n"
    "/* 字符串 */\n"
    "T('str_slice', 'hello'.slice(1, 3) === 'el');\n"
    "T('str_split', 'a,b,c'.split(',').length === 3);\n"
    "T('str_concat', 'a'.concat('b') === 'ab');\n"
    "T('str_repeat', 'ab'.repeat(3) === 'ababab');\n"
    "T('str_index', 'hello'.indexOf('l') === 2);\n"
    "\n"
    "/* 数组 */\n"
    "var arr = [3, 1, 2];\n"
    "arr.sort();\n"
    "T('arr_sort', arr[0] === 1 && arr[2] === 3);\n"
    "arr.push(4);\n"
    "T('arr_push_len', arr.length === 4);\n"
    "T('arr_join', [1, 2].join('-') === '1-2');\n"
    "T('arr_pop', [1, 2, 3].pop() === 3);\n"
    "T('arr_reverse', [1, 2, 3].reverse()[0] === 3);\n"
    "T('arr_isarray', Array.isArray([1]) === true);\n"
    "T('arr_len', [1, [2, 3]].length === 2);\n"
    "\n"
    "/* Math */\n"
    "T('math_pow', Math.pow(2, 10) === 1024);\n"
    "T('math_abs', Math.abs(-5) === 5);\n"
    "T('math_floor', Math.floor(3.9) === 3);\n"
    "T('math_min', Math.min(3, 1, 2) === 1);\n"
    "\n"
    "/* JSON */\n"
    "var obj = JSON.parse('{\"a\":1,\"b\":[1,2]}');\n"
    "T('json_parse', obj.a === 1 && obj.b.length === 2);\n"
    "var round = JSON.parse(JSON.stringify({ k: 42 }));\n"
    "T('json_roundtrip', round.k === 42);\n"
    "\n"
    "/* Date / performance */\n"
    "var now = Date.now();\n"
    "T('date_now', typeof now === 'number' && now > 0);\n"
    "var perf = performance.now();\n"
    "T('perf_now', typeof perf === 'number' && perf >= 0);\n"
    "\n"
    "/* 严格模式行为：未声明变量赋值应抛 ReferenceError */\n"
    "var strict_ok = false;\n"
    "try { __undeclaredVar = 1; } catch (e) { strict_ok = true; }\n"
    "T('strict_undeclared', strict_ok);\n";

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

/* 读取 JS 侧全局对象 __r 的属性（0/1） */
static int read_result(JSContext *ctx, const char *name)
{
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue obj = JS_GetPropertyStr(ctx, global, "__r");
    JS_FreeValue(ctx, global);
    if (JS_IsException(obj)) {
        JS_GetException(ctx);
        return -1;
    }
    JSValue v = JS_GetPropertyStr(ctx, obj, name);
    JS_FreeValue(ctx, obj);
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

static const char *kResultNames[] = {
    "arith", "var", "func", "closure", "ternary",
    "str_slice", "str_split", "str_concat", "str_repeat", "str_index",
    "arr_sort", "arr_push_len", "arr_join", "arr_pop", "arr_reverse",
    "arr_isarray", "arr_len",
    "math_pow", "math_abs", "math_floor", "math_min",
    "json_parse", "json_roundtrip",
    "date_now", "perf_now",
    "strict_undeclared",
    NULL,
};

static void test_js_core_features(void **state)
{
    const char *script_path = "build/js_core_test.js";
    int i;

    (void)state;
    assert_int_equal(write_script(script_path, kTestScript), 0);

    assert_int_equal(js_module_init(), 0);
    assert_int_equal(js_module_load_file(script_path), 0);

    for (i = 0; kResultNames[i]; i++) {
        int got = read_result((JSContext *)js_module_get_context(), kResultNames[i]);
        if (got != 1) {
            fail_msg("JS check '%s' = %d, want 1", kResultNames[i], got);
        }
    }

    js_module_cleanup();
    remove(script_path);
}

int main(int argc, char **argv)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_js_core_features),
    };
    (void)argc;
    (void)argv;
    return cmocka_run_group_tests(tests, NULL, NULL);
}
