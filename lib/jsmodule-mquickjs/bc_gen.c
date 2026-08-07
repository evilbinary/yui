/* bc_gen.c: PC 端 mquickjs 字节码编译工具（独立实现，不依赖 mqjs_std.c）。
 *
 * 用 YUI 的 ROM 标准库表（js_yuistdlib，来自 yui_stdlib.c + mqjs_stdlib.c，
 * 含 YUI/Socket atom）编译 JS 为 32 位字节码，使编译期解析的全局标识符与
 * ESP32 运行时的 atom 表完全一致，字节码运行时才能解析到 YUI 等全局对象。
 *
 * 用法: bc_gen [-o out.bc] [-m32] [--base ADDR] script.js
 */
#define DBUILD_NO_MAIN 1
#define YUI_STDLIB_USE_SHIM 1
#include <stdint.h>
#include "mqjs_shim.c"
#include "yui_stdlib_stubs.c"
#include "yui_stdlib_build.c"
/* 64 位主机用 64 位表（yui_stdlib.h），32 位目标用 32 位表 */
#ifdef JS_PTR64
#include "yui_stdlib.h"
#else
#include "yui_stdlib_32.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ---- 编译工具不需要 YUI 运行时；提供 yui_stdlib.c 引用符号的空实现 ---- */
#include "perf.h"
int perf_is_enabled(void) { return 0; }
void perf_enable(int on) { (void)on; }
void perf_reset(void) {}
void perf_set_overlay(int on) { (void)on; }
int perf_overlay_enabled(void) { return 0; }
void perf_set_top_n(int n) { (void)n; }
void perf_set_log_interval(int frames) { (void)frames; }
int perf_watch(const char* layer_id) { (void)layer_id; return 0; }
int perf_unwatch(const char* layer_id) { (void)layer_id; return 0; }
void perf_clear_watch(void) {}
void perf_frame_begin(void) {}
void perf_frame_end(void) {}
void perf_render_tree_begin(void) {}
void perf_render_tree_end(void) {}
void perf_layer_tree_enter(Layer* layer) { (void)layer; }
void perf_layer_add_self_ns(Layer* layer, uint64_t ns) { (void)layer; (void)ns; }
uint64_t perf_now_ns(void) { return 0; }
void perf_layer_destroyed(Layer* layer) { (void)layer; }
void perf_draw_overlay(Layer* root) { (void)root; }
const PerfFrameStats* perf_get_frame_stats(void) { return NULL; }
int perf_get_layer_stats(PerfLayerStats* out, int max_count, PerfSortBy sort_by) {
    (void)out; (void)max_count; (void)sort_by; return 0;
}
cJSON* layer_to_json(const Layer* layer, int flags) { (void)layer; (void)flags; return NULL; }

/* ---- 编译工具不执行这些标准库运行时；提供空实现（仅满足链接）。
 *      这些函数定义在 mqjs_std.c（引擎 REPL），bc_gen 只编译引擎核心，
 *      不链接 REPL；表引用它们仅作为符号，编译字节码不会执行。 ---- */
JSValue js_gc(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) { (void)ctx;(void)this_val;(void)argc;(void)argv; return JS_UNDEFINED; }
JSValue js_setTimeout(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) { (void)ctx;(void)this_val;(void)argc;(void)argv; return JS_UNDEFINED; }
JSValue js_clearTimeout(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) { (void)ctx;(void)this_val;(void)argc;(void)argv; return JS_UNDEFINED; }
JSValue js_load(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) { (void)ctx;(void)this_val;(void)argc;(void)argv; return JS_UNDEFINED; }
JSValue js_clear(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) { (void)ctx;(void)this_val;(void)argc;(void)argv; return JS_UNDEFINED; }
JSValue js_set(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv) { (void)ctx;(void)this_val;(void)argc;(void)argv; return JS_UNDEFINED; }

static uint8_t *bc_read_file(const char *filename, int *plen)
{    FILE *f;
    uint8_t *buf;
    long sz;

    f = fopen(filename, "rb");
    if (!f)
        return NULL;
    fseek(f, 0, SEEK_END);
    sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 0) { fclose(f); return NULL; }
    buf = (uint8_t *)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        free(buf); fclose(f); return NULL;
    }
    buf[sz] = '\0';
    fclose(f);
    if (plen) *plen = (int)sz;
    return buf;
}

static int compile_to_file(const char *filename, const char *outfilename,
                           size_t mem_size, int force_32bit,
                           uintptr_t base_addr)
{
    uint8_t *mem_buf;
    JSContext *ctx;
    char *eval_str;
    JSValue val;
    union {
        JSBytecodeHeader hdr;
#if JSW == 8
        JSBytecodeHeader32 hdr32;
#endif
    } hdr_buf;
    int hdr_len;
    const uint8_t *data_buf;
    uint32_t data_len;
    FILE *f;
    int rc = 0;

    mem_buf = (uint8_t *)malloc(mem_size);
    if (!mem_buf) { fprintf(stderr, "out of memory\n"); return 1; }
    ctx = JS_NewContext2(mem_buf, mem_size, &js_yuistdlib, TRUE);
    JS_SetLogFunc(ctx, js_log_func);

    eval_str = (char *)bc_read_file(filename, NULL);
    if (!eval_str) {
        fprintf(stderr, "cannot open %s\n", filename);
        rc = 1;
        goto done;
    }

    val = JS_Parse(ctx, eval_str, strlen(eval_str), filename, 0);
    free(eval_str);
    if (JS_IsException(val)) {
        fprintf(stderr, "parse error in %s\n", filename);
        rc = 1;
        goto done;
    }

#if JSW == 8
    if (force_32bit) {
        if (JS_PrepareBytecode64to32(ctx, &hdr_buf.hdr32, &data_buf, &data_len, val)) {
            fprintf(stderr, "could not convert bytecode to 32-bit\n");
            rc = 1;
            goto done;
        }
        hdr_len = sizeof(JSBytecodeHeader32);
        hdr_buf.hdr32.base_addr = 0;
        if (base_addr != 0) {
            JS_RelocateBytecode2(ctx, (JSBytecodeHeader *)&hdr_buf.hdr32,
                                 (uint8_t *)data_buf, data_len,
                                 (uintptr_t)base_addr, 0);
        }
    } else
#endif
    {
        JS_PrepareBytecode(ctx, &hdr_buf.hdr, &data_buf, &data_len, val);
        if (base_addr != 0) {
            JS_RelocateBytecode2(ctx, &hdr_buf.hdr, (uint8_t *)data_buf,
                                 data_len, (uintptr_t)base_addr, 0);
        } else {
            JS_RelocateBytecode2(ctx, &hdr_buf.hdr, (uint8_t *)data_buf,
                                 data_len, 0, 0);
        }
        hdr_len = sizeof(JSBytecodeHeader);
    }

    f = fopen(outfilename, "wb");
    if (!f) {
        fprintf(stderr, "cannot open %s\n", outfilename);
        rc = 1;
        goto done;
    }
    fwrite(&hdr_buf, 1, (size_t)hdr_len, f);
    fwrite(data_buf, 1, data_len, f);
    fclose(f);
    fprintf(stderr, "wrote %s (%u bytes, base=0x%08lx)\n", outfilename,
            (unsigned)((uint32_t)hdr_len + data_len), (unsigned long)base_addr);

done:
    JS_FreeContext(ctx);
    free(mem_buf);
    return rc;
}

int main(int argc, const char **argv)
{
    const char *out = NULL;
    const char *input = NULL;
    int force_32bit = 0;
    uintptr_t base = 0;
    size_t mem_size = 16 << 20;
    int i;

    for (i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (!strcmp(a, "-o") && i + 1 < argc) {
            out = argv[++i];
        } else if (!strcmp(a, "-m32")) {
            force_32bit = 1;
        } else if (!strcmp(a, "--base") && i + 1 < argc) {
            base = (uintptr_t)strtoul(argv[++i], NULL, 0);
        } else if (!strcmp(a, "--mem") && i + 1 < argc) {
            mem_size = (size_t)strtoul(argv[++i], NULL, 0);
        } else if (a[0] != '-') {
            input = a;
        } else {
            fprintf(stderr, "usage: bc_gen [-o out.bc] [-m32] [--base ADDR] script.js\n");
            return 2;
        }
    }
    if (!input) {
        fprintf(stderr, "usage: bc_gen [-o out.bc] [-m32] [--base ADDR] script.js\n");
        return 2;
    }
    if (!out) {
        size_t n = strlen(input);
        char *buf = (char *)malloc(n + 4);
        if (n > 3 && !strcmp(input + n - 3, ".js"))
            snprintf(buf, n + 4, "%.*s.bc", (int)(n - 3), input);
        else
            snprintf(buf, n + 4, "%s.bc", input);
        out = buf;
    }
    return compile_to_file(input, out, mem_size, force_32bit, base);
}
