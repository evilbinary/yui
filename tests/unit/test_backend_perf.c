/*
 * Perf gate: backend drawing APIs must stay under per-op time budgets.
 * Each op runs PERF_ITERS (default 10000, override YUI_PERF_ITER) iterations,
 * timing is sampled in chunks, a statistics report (min/avg/max, p50/p90/p99,
 * ops/s) is printed, and the test fails when an avg budget is exceeded.
 * Budgets are deliberately soft to stay stable on slow CI / software renderers.
 */
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
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

#ifndef YUI_PERF_ITER_ENV
#define YUI_PERF_ITER_ENV "YUI_PERF_ITER"
#endif
#define PERF_DEFAULT_ITERS 10000
#define MAX_SAMPLES 256

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

typedef struct {
    int iterations;
    int samples;
    double min_us;
    double max_us;
    double avg_us;
    double p50_us;
    double p90_us;
    double p99_us;
    double ops_per_sec;
} PerfStats;

static int perf_iterations(void)
{
    const char *e = getenv(YUI_PERF_ITER_ENV);
    if (e) {
        int v = atoi(e);
        if (v > 0) {
            return v;
        }
    }
    return PERF_DEFAULT_ITERS;
}

static int cmp_double(const void *a, const void *b)
{
    double da = *(const double *)a;
    double db = *(const double *)b;
    return (da > db) - (da < db);
}

static double calc_percentile(const double *arr, int n, double p)
{
    double pos = p * (double)(n - 1);
    int lo = (int)pos;
    int hi = lo + 1;
    double frac = pos - (double)lo;
    if (hi >= n) {
        return arr[lo];
    }
    return arr[lo] * (1.0 - frac) + arr[hi] * frac;
}

static PerfStats run_bench(PerfOpFn fn, void *ctx, int iterations, int warmup)
{
    PerfStats st;
    double samples[MAX_SAMPLES];
    double sum = 0.0;
    int i, n_samples = 0;
    int sample_every = iterations / 100;
    double chunk_start;
    double t1;

    memset(&st, 0, sizeof(st));
    st.iterations = iterations;
    if (sample_every < 1) {
        sample_every = 1;
    }

    for (i = 0; i < warmup; i++) {
        fn(ctx);
    }

    chunk_start = perf_now_us();
    for (i = 0; i < iterations; i++) {
        fn(ctx);
        if ((i + 1) % sample_every == 0) {
            t1 = perf_now_us();
            if (n_samples < MAX_SAMPLES) {
                samples[n_samples] = (t1 - chunk_start) / (double)sample_every;
                n_samples++;
            }
            chunk_start = t1;
        }
    }

    if (n_samples < 1) {
        n_samples = 1;
        samples[0] = 0.0;
    }

    st.samples = n_samples;
    st.min_us = samples[0];
    st.max_us = samples[0];
    for (i = 0; i < n_samples; i++) {
        double v = samples[i];
        sum += v;
        if (v < st.min_us) st.min_us = v;
        if (v > st.max_us) st.max_us = v;
    }
    st.avg_us = sum / (double)n_samples;
    st.ops_per_sec = (st.avg_us > 0.0) ? 1e6 / st.avg_us : 0.0;

    qsort(samples, (size_t)n_samples, sizeof(double), cmp_double);
    st.p50_us = calc_percentile(samples, n_samples, 0.50);
    st.p90_us = calc_percentile(samples, n_samples, 0.90);
    st.p99_us = calc_percentile(samples, n_samples, 0.99);

    return st;
}

static int check_budget(const char *name, const PerfStats *st, double budget_us)
{
    int ok = st->avg_us <= budget_us;
    printf("[perf] %-30s %6d iters %6d samples  min=%9.2f  avg=%9.2f  max=%9.2f"
           "  p50=%8.2f  p90=%8.2f  p99=%8.2f us/op  %9.0f ops/s  %s  (budget %.0f)\n",
           name, st->iterations, st->samples,
           st->min_us, st->avg_us, st->max_us,
           st->p50_us, st->p90_us, st->p99_us,
           st->ops_per_sec, ok ? "PASS" : "FAIL", budget_us);
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

/* ---------- perf case table ---------- */
typedef struct {
    const char *name;
    PerfOpFn fn;
    double budget_us;
    int skip_if_no_texture;
} PerfCase;

static PerfCase g_cases[] = {
    {"clear_color", op_clear, 100.0, 0},
    {"fill_rect", op_fill_rect, 100.0, 0},
    {"fill_rect_color", op_fill_rect_color, 100.0, 0},
    {"rect", op_rect, 100.0, 0},
    {"rect_color", op_rect_color, 100.0, 0},
    {"rounded_rect", op_rounded_rect, 800.0, 0},
    {"rounded_rect_color", op_rounded_rect_color, 800.0, 0},
    {"rounded_rect_with_border", op_rounded_rect_border, 1400.0, 0},
    {"rounded_gradient", op_rounded_gradient, 800.0, 0},
    {"line", op_line, 1800.0, 0},
    {"bezier_cubic", op_bezier, 1800.0, 0},
    {"arc", op_arc, 900.0, 0},
    {"shadow", op_shadow, 1500.0, 0},
    {"backdrop_filter", op_backdrop_filter, 25000.0, 0},
    {"text_copy", op_text_copy, 100.0, 1},
    {"full_frame (10 fills)", op_frame, 50000.0, 0},
};

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

    DFont *font = backend_load_font("app/assets/Roboto-Regular.ttf", 14);
    if (font) {
        g_ctx.texture = backend_render_texture(font, "PerfGate", g_ctx.color);
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

static void test_backend_draw_stats(void **state)
{
    int iters = perf_iterations();
    int warmup = iters / 100;
    int i, n = (int)(sizeof(g_cases) / sizeof(g_cases[0]));
    int failures = 0;
    (void)state;

    if (warmup < 10) {
        warmup = 10;
    }

    printf("[perf] === backend drawing stats (iters=%d, samples per op ~%d, env %s to override) ===\n",
           iters, iters / 100, YUI_PERF_ITER_ENV);
    for (i = 0; i < n; i++) {
        PerfCase *c = &g_cases[i];
        if (c->skip_if_no_texture && !g_ctx.texture) {
            printf("[perf] %-30s SKIP (no font/texture)\n", c->name);
            continue;
        }
        PerfStats st = run_bench(c->fn, &g_ctx, iters, warmup);
        if (!check_budget(c->name, &st, c->budget_us)) {
            failures++;
        }
    }
    printf("[perf] === %s (%d/%d ops within budget) ===\n",
           failures == 0 ? "ALL PASS" : "FAILED", n - failures, n);
    assert_int_equal(failures, 0);
}

int main(int argc, char **argv)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_backend_draw_stats, setup_backend, teardown_backend),
    };
    (void)argc;
    (void)argv;
    return cmocka_run_group_tests(tests, NULL, NULL);
}
