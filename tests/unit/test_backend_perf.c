/*
 * Perf gate: backend drawing APIs must stay under per-op time budgets.
 * Each test warms up the renderer, measures average us/op over N iterations,
 * prints a result table and fails when a budget is exceeded.
 * Budgets are deliberately soft to stay stable on slow CI / software renderers.
 */
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <cmocka.h>

#include "ytype.h"
#include "backend.h"

int main(int argc, char **argv);

#if defined(_WIN32)
#include <windows.h>
int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    (void)hInstance;
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;
    return main(__argc, __argv);
}
#endif

/* ---------- high resolution timer ---------- */
static double perf_now_us(void)
{
#if defined(_WIN32)
    static LARGE_INTEGER freq = {0};
    LARGE_INTEGER cnt;
    if (freq.QuadPart == 0) {
        QueryPerformanceFrequency(&freq);
    }
    QueryPerformanceCounter(&cnt);
    return (double)cnt.QuadPart * 1000000.0 / (double)freq.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000000.0 + (double)ts.tv_nsec / 1000.0;
#endif
}

/* ---------- benchmark plumbing ---------- */
typedef void (*PerfOpFn)(void *ctx);

static double run_bench(PerfOpFn fn, void *ctx, int iterations, int warmup)
{
    int i;
    double t0, t1;
    for (i = 0; i < warmup; i++) {
        fn(ctx);
    }
    t0 = perf_now_us();
    for (i = 0; i < iterations; i++) {
        fn(ctx);
    }
    t1 = perf_now_us();
    return (t1 - t0) / (double)iterations;
}

static int check_budget(const char *name, double us_per_op, double budget_us)
{
    int ok = us_per_op <= budget_us;
    printf("[perf] %-28s %10.2f us/op  (budget %9.0f us/op) %s\n",
           name, us_per_op, budget_us, ok ? "PASS" : "FAIL");
    return ok;
}

typedef struct {
    Rect rect;
    Rect src;
    Rect dst;
    Color color;
    Color color2;
    Texture *texture;
    int radius;
    int border_width;
    int backdrop_rect_i;
} PerfCtx;

static PerfCtx g_ctx;

/* 8 distinct rects so the backdrop blur cache (5 entries) always misses,
 * exercising the real blur cost rather than the cached blit path. */
static Rect g_backdrop_rects[8];

/* ---------- per-op callbacks ---------- */
static void op_clear(void *ctx)
{
    (void)ctx;
    backend_render_clear_color(30, 30, 40, 255);
}

static void op_fill_rect(void *ctx)
{
    PerfCtx *c = ctx;
    backend_render_fill_rect(&c->rect, c->color);
}

static void op_fill_rect_color(void *ctx)
{
    PerfCtx *c = ctx;
    backend_render_fill_rect_color(&c->rect, 200, 100, 100, 255);
}

static void op_rect(void *ctx)
{
    PerfCtx *c = ctx;
    backend_render_rect(&c->rect, c->color);
}

static void op_rect_color(void *ctx)
{
    PerfCtx *c = ctx;
    backend_render_rect_color(&c->rect, 0, 128, 255, 255);
}

static void op_rounded_rect(void *ctx)
{
    PerfCtx *c = ctx;
    backend_render_rounded_rect(&c->rect, c->color, c->radius);
}

static void op_rounded_rect_color(void *ctx)
{
    PerfCtx *c = ctx;
    backend_render_rounded_rect_color(&c->rect, 90, 200, 120, 255, c->radius);
}

static void op_rounded_rect_border(void *ctx)
{
    PerfCtx *c = ctx;
    backend_render_rounded_rect_with_border(&c->rect, c->color, c->radius, c->border_width, c->color2);
}

static void op_rounded_gradient(void *ctx)
{
    PerfCtx *c = ctx;
    Color colors[2];
    colors[0] = c->color;
    colors[1] = c->color2;
    backend_render_rounded_gradient(&c->rect, c->radius, 1, colors, 2);
}

static void op_line(void *ctx)
{
    PerfCtx *c = ctx;
    backend_render_line(10, 10, 200, 120, c->color);
}

static void op_bezier(void *ctx)
{
    PerfCtx *c = ctx;
    backend_render_bezier_cubic(10, 100, 60, 10, 140, 190, 200, 100, c->color, 2);
}

static void op_arc(void *ctx)
{
    PerfCtx *c = ctx;
    backend_render_arc(100, 80, 60, 0.0f, 6.28f, c->color, 3);
}

static void op_shadow(void *ctx)
{
    PerfCtx *c = ctx;
    backend_render_shadow(&c->rect, 12, 0, 4, 8, 0, c->color2);
}

static void op_backdrop_filter(void *ctx)
{
    PerfCtx *c = ctx;
    Rect *r = &g_backdrop_rects[c->backdrop_rect_i];
    c->backdrop_rect_i = (c->backdrop_rect_i + 1) & 7;
    backend_render_backdrop_filter(r, 10, 1.0f, 1.0f);
}

static void op_text_copy(void *ctx)
{
    PerfCtx *c = ctx;
    if (c->texture) {
        backend_render_text_copy(c->texture, &c->src, &c->dst);
    }
}

static void op_frame(void *ctx)
{
    PerfCtx *c = ctx;
    int i;
    backend_render_clear_color(30, 30, 40, 255);
    for (i = 0; i < 10; i++) {
        Rect r;
        r.x = 10 + i * 18;
        r.y = 20;
        r.w = 60;
        r.h = 40;
        backend_render_fill_rect_color(&r, 50, 120, 220, 255);
    }
    backend_render_rounded_rect_color(&c->rect, 90, 200, 120, 255, c->radius);
    backend_render_line(10, 10, 200, 120, c->color);
    if (c->texture) {
        backend_render_text_copy(c->texture, &c->src, &c->dst);
    }
    backend_render_present();
}

/* ---------- test cases ---------- */
static int setup_backend(void **state)
{
    (void)state;
    if (backend_init() != 0) {
        return -1; /* skip if SDL/backend unavailable */
    }

    memset(&g_ctx, 0, sizeof(g_ctx));
    g_ctx.rect = (Rect){40, 30, 200, 120};
    g_ctx.src = (Rect){0, 0, 80, 24};
    g_ctx.dst = (Rect){20, 60, 160, 48};
    g_ctx.color = (Color){220, 120, 60, 255};
    g_ctx.color2 = (Color){40, 90, 200, 255};
    g_ctx.radius = 12;
    g_ctx.border_width = 2;

    for (int i = 0; i < 8; i++) {
        g_backdrop_rects[i] = (Rect){30 + i * 12, 40 + i * 8, 160, 120};
    }

    DFont *font = backend_load_font("Roboto-Regular.ttf", 14);
    if (font) {
        g_ctx.texture = backend_render_texture(font, "PerfGate", g_ctx.color);
    }else{
        printf("load font failed\n");
        return -1;
    }
    return 0;
}

static int teardown_backend(void **state)
{
    (void)state;
    if (g_ctx.texture) {
        backend_render_text_destroy(g_ctx.texture);
        g_ctx.texture = NULL;
    }
    backend_quit();
    return 0;
}

static void test_clear_color_budget(void **state)
{
    (void)state;
    double us = run_bench(op_clear, &g_ctx, 2000, 20);
    assert_true(check_budget("clear_color", us, 100.0));
}

static void test_fill_rect_budget(void **state)
{
    (void)state;
    double us = run_bench(op_fill_rect, &g_ctx, 2000, 20);
    assert_true(check_budget("fill_rect", us, 100.0));
}

static void test_fill_rect_color_budget(void **state)
{
    (void)state;
    double us = run_bench(op_fill_rect_color, &g_ctx, 2000, 20);
    assert_true(check_budget("fill_rect_color", us, 100.0));
}

static void test_rect_budget(void **state)
{
    (void)state;
    double us = run_bench(op_rect, &g_ctx, 2000, 20);
    assert_true(check_budget("rect", us, 100.0));
}

static void test_rect_color_budget(void **state)
{
    (void)state;
    double us = run_bench(op_rect_color, &g_ctx, 2000, 20);
    assert_true(check_budget("rect_color", us, 100.0));
}

static void test_rounded_rect_budget(void **state)
{
    (void)state;
    double us = run_bench(op_rounded_rect, &g_ctx, 1000, 10);
    assert_true(check_budget("rounded_rect", us, 800.0));
}

static void test_rounded_rect_color_budget(void **state)
{
    (void)state;
    double us = run_bench(op_rounded_rect_color, &g_ctx, 1000, 10);
    assert_true(check_budget("rounded_rect_color", us, 800.0));
}

static void test_rounded_rect_border_budget(void **state)
{
    (void)state;
    double us = run_bench(op_rounded_rect_border, &g_ctx, 1000, 10);
    assert_true(check_budget("rounded_rect_with_border", us, 1400.0));
}

static void test_rounded_gradient_budget(void **state)
{
    (void)state;
    double us = run_bench(op_rounded_gradient, &g_ctx, 300, 5);
    assert_true(check_budget("rounded_gradient", us, 800.0));
}

static void test_line_budget(void **state)
{
    (void)state;
    double us = run_bench(op_line, &g_ctx, 2000, 20);
    assert_true(check_budget("line", us, 1800.0));
}

static void test_bezier_budget(void **state)
{
    (void)state;
    double us = run_bench(op_bezier, &g_ctx, 800, 10);
    assert_true(check_budget("bezier_cubic", us, 1800.0));
}

static void test_arc_budget(void **state)
{
    (void)state;
    double us = run_bench(op_arc, &g_ctx, 800, 10);
    assert_true(check_budget("arc", us, 900.0));
}

static void test_shadow_budget(void **state)
{
    (void)state;
    double us = run_bench(op_shadow, &g_ctx, 300, 5);
    assert_true(check_budget("shadow", us, 1500.0));
}

static void test_backdrop_filter_budget(void **state)
{
    (void)state;
    double us = run_bench(op_backdrop_filter, &g_ctx, 40, 8);
    assert_true(check_budget("backdrop_filter", us, 25000.0));
}

static void test_text_copy_budget(void **state)
{
    (void)state;
    if (!g_ctx.texture) {
        print_message("[perf] text_copy skipped (no font/texture)\n");
        return;
    }
    double us = run_bench(op_text_copy, &g_ctx, 1000, 10);
    assert_true(check_budget("text_copy", us, 100.0));
}

static void test_full_frame_budget(void **state)
{
    (void)state;
    /* Busy headless frame: clear + draws + present must stay well under 16.6ms
     * of slack; 50ms budget tolerates software renderers / slow CI. */
    double us = run_bench(op_frame, &g_ctx, 120, 5);
    assert_true(check_budget("full_frame (10 fills)", us, 50000.0));
}

int main(int argc, char **argv)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_clear_color_budget, setup_backend, teardown_backend),
        cmocka_unit_test_setup_teardown(test_fill_rect_budget, setup_backend, teardown_backend),
        cmocka_unit_test_setup_teardown(test_fill_rect_color_budget, setup_backend, teardown_backend),
        cmocka_unit_test_setup_teardown(test_rect_budget, setup_backend, teardown_backend),
        cmocka_unit_test_setup_teardown(test_rect_color_budget, setup_backend, teardown_backend),
        cmocka_unit_test_setup_teardown(test_rounded_rect_budget, setup_backend, teardown_backend),
        cmocka_unit_test_setup_teardown(test_rounded_rect_color_budget, setup_backend, teardown_backend),
        cmocka_unit_test_setup_teardown(test_rounded_rect_border_budget, setup_backend, teardown_backend),
        cmocka_unit_test_setup_teardown(test_rounded_gradient_budget, setup_backend, teardown_backend),
        cmocka_unit_test_setup_teardown(test_line_budget, setup_backend, teardown_backend),
        cmocka_unit_test_setup_teardown(test_bezier_budget, setup_backend, teardown_backend),
        cmocka_unit_test_setup_teardown(test_arc_budget, setup_backend, teardown_backend),
        cmocka_unit_test_setup_teardown(test_shadow_budget, setup_backend, teardown_backend),
        cmocka_unit_test_setup_teardown(test_backdrop_filter_budget, setup_backend, teardown_backend),
        cmocka_unit_test_setup_teardown(test_text_copy_budget, setup_backend, teardown_backend),
        cmocka_unit_test_setup_teardown(test_full_frame_budget, setup_backend, teardown_backend),
    };
    (void)argc;
    (void)argv;
    return cmocka_run_group_tests(tests, NULL, NULL);
}
