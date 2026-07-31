/*
 * Pixel-level functional tests for core backend drawing APIs:
 *   - backend_render_fill_rect / fill_rect_color
 *   - backend_render_rect / rect_color (border only)
 *   - backend_render_rounded_rect (fill, texture-cache path)
 *   - backend_render_line (h/v fast path + diagonal AA path)
 *   - backend_render_shadow (offset/blur/spread)
 *
 * Each test draws onto a private offscreen render-target texture and reads
 * pixels back with SDL_RenderReadPixels. Using an offscreen target avoids the
 * window's GL double-buffering (which makes post-present readbacks return the
 * previous frame), so assertions are deterministic.
 */
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <cmocka.h>
#include <SDL.h>

#include "ytype.h"
#include "backend.h"

extern SDL_Renderer *renderer;
extern SDL_Window *window;

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

#define TEX_W 400
#define TEX_H 400

typedef struct { Uint8 r, g, b, a; } Pix;

static SDL_Texture *g_tex;

static int setup_target(void **state)
{
    (void)state;
    g_tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888,
                              SDL_TEXTUREACCESS_TARGET, TEX_W, TEX_H);
    if (!g_tex) {
        printf("CreateTexture failed: %s\n", SDL_GetError());
        return -1;
    }
    if (SDL_SetRenderTarget(renderer, g_tex) != 0) {
        printf("SetRenderTarget failed: %s\n", SDL_GetError());
        SDL_DestroyTexture(g_tex);
        g_tex = NULL;
        return -1;
    }
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    return 0;
}

static int teardown_target(void **state)
{
    (void)state;
    SDL_SetRenderTarget(renderer, NULL);
    if (g_tex) {
        SDL_DestroyTexture(g_tex);
        g_tex = NULL;
    }
    return 0;
}

static int read_pixel(int x, int y, Pix *out)
{
    Uint8 buf[4] = {0};
    SDL_Rect r = {x, y, 1, 1};
    if (SDL_RenderReadPixels(renderer, &r, SDL_PIXELFORMAT_ABGR8888, buf, 4) != 0) {
        printf("ReadPixels failed: %s\n", SDL_GetError());
        return -1;
    }
    out->r = buf[0];
    out->g = buf[1];
    out->b = buf[2];
    out->a = buf[3];
    return 0;
}

static void expect_close(const char *what, int x, int y, Pix p,
                         int er, int eg, int eb, int tol)
{
    int dr = abs((int)p.r - er);
    int dg = abs((int)p.g - eg);
    int db = abs((int)p.b - eb);
    if (dr > tol || dg > tol || db > tol) {
        printf("FAIL %s @(%d,%d): got (%d,%d,%d) want (%d,%d,%d) tol %d\n",
               what, x, y, p.r, p.g, p.b, er, eg, eb, tol);
        fail_msg("%s mismatch at (%d,%d)", what, x, y);
    }
}

static void expect_any_color(const char *what, int x, int y, Pix p, int tol)
{
    if (p.r <= tol && p.g <= tol && p.b <= tol) {
        printf("FAIL %s @(%d,%d): got (%d,%d,%d) expected non-background\n",
               what, x, y, p.r, p.g, p.b);
        fail_msg("%s is empty at (%d,%d)", what, x, y);
    }
}

/* ---------- fill rect ---------- */

static void test_fill_rect(void **state)
{
    (void)state;
    Color c = {30, 200, 90, 255};
    Rect rc = {60, 50, 120, 80};

    backend_render_fill_rect(&rc, c);

    Pix p;
    assert_int_equal(read_pixel(100, 70, &p), 0);
    expect_close("fill_in", 100, 70, p, 30, 200, 90, 10);
    assert_int_equal(read_pixel(60, 50, &p), 0);
    expect_close("fill_edge", 60, 50, p, 30, 200, 90, 10);
    assert_int_equal(read_pixel(10, 10, &p), 0);
    expect_close("fill_out", 10, 10, p, 0, 0, 0, 10);
}

static void test_fill_rect_color_alpha(void **state)
{
    (void)state;
    /* translucent blue over black -> scaled channel values */
    Rect rc = {200, 150, 100, 60};

    backend_render_fill_rect_color(&rc, 0, 128, 255, 128);

    Pix p;
    assert_int_equal(read_pixel(250, 180, &p), 0);
    /* 0.5 blend over black: expect ~(0, 64, 127) */
    expect_close("alpha", 250, 180, p, 0, 64, 127, 20);
    assert_int_equal(read_pixel(30, 40, &p), 0);
    expect_close("alpha_out", 30, 40, p, 0, 0, 0, 10);
}

/* ---------- border rect ---------- */

static void test_render_rect_border(void **state)
{
    (void)state;
    Color c = {255, 180, 0, 255};
    Rect rc = {80, 90, 100, 70};

    backend_render_rect(&rc, c);

    Pix p;
    /* border pixels on each edge (border is drawn inside the rect) */
    assert_int_equal(read_pixel(80, 100, &p), 0);
    expect_close("border_top", 80, 100, p, 255, 180, 0, 10);
    assert_int_equal(read_pixel(170, 90, &p), 0);
    expect_close("border_left", 170, 90, p, 255, 180, 0, 10);
    assert_int_equal(read_pixel(150, 159, &p), 0);
    expect_close("border_bottom", 150, 159, p, 255, 180, 0, 10);
    assert_int_equal(read_pixel(179, 125, &p), 0);
    expect_close("border_right", 179, 125, p, 255, 180, 0, 10);
    /* interior must be hollow */
    assert_int_equal(read_pixel(130, 125, &p), 0);
    expect_close("border_inside", 130, 125, p, 0, 0, 0, 10);
}

static void test_render_rect_color(void **state)
{
    (void)state;
    Rect rc = {40, 200, 80, 50};

    backend_render_rect_color(&rc, 0, 255, 0, 255);

    Pix p;
    assert_int_equal(read_pixel(60, 200, &p), 0);
    expect_close("rc_top", 60, 200, p, 0, 255, 0, 10);
    assert_int_equal(read_pixel(119, 220, &p), 0);
    expect_close("rc_right", 119, 220, p, 0, 255, 0, 10);
    assert_int_equal(read_pixel(80, 220, &p), 0);
    expect_close("rc_inside", 80, 220, p, 0, 0, 0, 10);
}

/* ---------- rounded rect ---------- */

static void test_rounded_rect_fill(void **state)
{
    (void)state;
    Color c = {120, 40, 220, 255};
    Rect rc = {60, 60, 140, 100};
    int radius = 20;

    backend_render_rounded_rect(&rc, c, radius);

    Pix p;
    assert_int_equal(read_pixel(130, 110, &p), 0);
    expect_close("rr_center", 130, 110, p, 120, 40, 220, 10);
    assert_int_equal(read_pixel(70, 100, &p), 0);
    expect_close("rr_edge_mid", 70, 100, p, 120, 40, 220, 10);
    assert_int_equal(read_pixel(72, 72, &p), 0);
    expect_close("rr_corner", 72, 72, p, 120, 40, 220, 30);
    assert_int_equal(read_pixel(61, 61, &p), 0);
    expect_close("rr_corner_out", 61, 61, p, 0, 0, 0, 10);
    assert_int_equal(read_pixel(130, 165, &p), 0);
    expect_close("rr_below", 130, 165, p, 0, 0, 0, 10);
}

static void test_rounded_rect_with_border(void **state)
{
    (void)state;
    Color bg = {10, 10, 200, 255};
    Color border = {255, 255, 255, 255};
    Rect rc = {100, 80, 120, 90};
    int radius = 12, bw = 5;

    backend_render_rounded_rect_with_border(&rc, bg, radius, bw, border);

    Pix p;
    /* border band (5px inside each edge) */
    assert_int_equal(read_pixel(110, 82, &p), 0);
    expect_close("rwb_border_top", 110, 82, p, 255, 255, 255, 30);
    assert_int_equal(read_pixel(218, 120, &p), 0);
    expect_close("rwb_border_right", 218, 120, p, 255, 255, 255, 30);
    /* inner fill (bg color) */
    assert_int_equal(read_pixel(160, 120, &p), 0);
    expect_close("rwb_fill", 160, 120, p, 10, 10, 200, 10);
    /* outside */
    assert_int_equal(read_pixel(90, 75, &p), 0);
    expect_close("rwb_out", 90, 75, p, 0, 0, 0, 10);
}

/* ---------- line ---------- */

static void test_line_horizontal(void **state)
{
    (void)state;
    Color c = {0, 200, 0, 255};

    backend_render_line(50, 120, 250, 120, c);

    Pix p;
    assert_int_equal(read_pixel(100, 120, &p), 0);
    expect_close("hline_mid", 100, 120, p, 0, 200, 0, 10);
    assert_int_equal(read_pixel(50, 120, &p), 0);
    expect_close("hline_start", 50, 120, p, 0, 200, 0, 10);
    assert_int_equal(read_pixel(250, 120, &p), 0);
    expect_close("hline_end", 250, 120, p, 0, 200, 0, 10);
    assert_int_equal(read_pixel(100, 121, &p), 0);
    expect_close("hline_off", 100, 121, p, 0, 0, 0, 10);
}

static void test_line_vertical(void **state)
{
    (void)state;
    Color c = {0, 0, 220, 255};

    backend_render_line(180, 40, 180, 200, c);

    Pix p;
    assert_int_equal(read_pixel(180, 100, &p), 0);
    expect_close("vline_mid", 180, 100, p, 0, 0, 220, 10);
    assert_int_equal(read_pixel(179, 100, &p), 0);
    expect_close("vline_off", 179, 100, p, 0, 0, 0, 10);
}

static void test_line_diagonal(void **state)
{
    (void)state;
    Color c = {255, 0, 0, 255};
    int x1 = 100, y1 = 100, x2 = 200, y2 = 200;

    backend_render_line(x1, y1, x2, y2, c);

    Pix p;
    /* Bresenham main points on the 45-degree diagonal */
    assert_int_equal(read_pixel(130, 130, &p), 0);
    expect_any_color("diag_main", 130, 130, p, 40);
    assert_int_equal(read_pixel(170, 170, &p), 0);
    expect_any_color("diag_main2", 170, 170, p, 40);
    /* penultimate point (Bresenham loop stops before writing (x2,y2)) */
    assert_int_equal(read_pixel(199, 199, &p), 0);
    expect_any_color("diag_penult", 199, 199, p, 40);
}

/* ---------- shadow ---------- */

static void test_shadow_offset_no_blur(void **state)
{
    (void)state;
    Color c = {80, 80, 80, 255};
    Rect rc = {60, 60, 100, 60};
    backend_render_shadow(&rc, 8, 10, 15, 0, 0, c);

    Pix p;
    /* shadow body: base = rect + offset = (70,75,100,60) */
    assert_int_equal(read_pixel(120, 100, &p), 0);
    expect_close("sh_body", 120, 100, p, 80, 80, 80, 20);
    /* top-left corner of original rect (outside shadow base) stays clear */
    assert_int_equal(read_pixel(62, 70, &p), 0);
    expect_close("sh_orig_empty", 62, 70, p, 0, 0, 0, 10);
}

static void test_shadow_blur(void **state)
{
    (void)state;
    Color c = {120, 120, 120, 255};
    Rect rc = {150, 150, 80, 50};
    backend_render_shadow(&rc, 6, 0, 0, 6, 0, c);

    Pix p;
    /* body center: strong */
    assert_int_equal(read_pixel(190, 175, &p), 0);
    expect_any_color("shb_center", 190, 175, p, 30);
    /* far above the blurred region stays clear */
    assert_int_equal(read_pixel(190, 100, &p), 0);
    expect_close("shb_far", 190, 100, p, 0, 0, 0, 15);
}

static void test_shadow_spread(void **state)
{
    (void)state;
    Color c = {200, 100, 0, 255};
    Rect rc = {40, 240, 60, 40};
    backend_render_shadow(&rc, 4, 0, 0, 0, 8, c);

    Pix p;
    /* inside the spread region but outside the original rect */
    assert_int_equal(read_pixel(36, 250, &p), 0);
    expect_close("spr_left", 36, 250, p, 200, 100, 0, 20);
    /* far outside */
    assert_int_equal(read_pixel(20, 260, &p), 0);
    expect_close("spr_out", 20, 260, p, 0, 0, 0, 10);
}

/* ---------- logical (non-pixel) API ---------- */

static void test_density(void **state)
{
    (void)state;
    backend_set_density(2.5f);
    assert_true(fabsf(backend_get_density() - 2.5f) < 1e-4f);
    /* invalid values are ignored: density keeps its previous value */
    backend_set_density(0.0f);
    assert_true(fabsf(backend_get_density() - 2.5f) < 1e-4f);
    backend_set_density(-3.0f);
    assert_true(fabsf(backend_get_density() - 2.5f) < 1e-4f);
    backend_set_density(1.0f);
    assert_true(fabsf(backend_get_density() - 1.0f) < 1e-4f);
}

static void test_windowsize_roundtrip(void **state)
{
    (void)state;
    int w = 0, h = 0;
    backend_get_windowsize(&w, &h);
    assert_true(w > 0 && h > 0);
    /* must stay above the 900x720 minimum enforced in backend_init */
    backend_set_windowsize(1000, 800);
    backend_get_windowsize(&w, &h);
    assert_int_equal(w, 1000);
    assert_int_equal(h, 800);
}

static void test_window_title(void **state)
{
    (void)state;
    backend_set_window_title("yui test");
    assert_string_equal(SDL_GetWindowTitle(window), "yui test");
}

static void test_window_resizable(void **state)
{
    (void)state;
    backend_set_resizable(1);
    assert_true((SDL_GetWindowFlags(window) & SDL_WINDOW_RESIZABLE) != 0);
    backend_set_resizable(0);
    assert_int_equal(SDL_GetWindowFlags(window) & SDL_WINDOW_RESIZABLE, 0);
}

static void test_window_minimum_size(void **state)
{
    (void)state;
    int w = 0, h = 0;
    backend_set_minimum_windowsize(320, 240);
    SDL_GetWindowMinimumSize(window, &w, &h);
    assert_int_equal(w, 320);
    assert_int_equal(h, 240);
}

static void test_query_texture(void **state)
{
    (void)state;
    SDL_Texture *tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888,
                                         SDL_TEXTUREACCESS_TARGET, 37, 23);
    assert_non_null(tex);
    Uint32 fmt = 0;
    int access = 0, w = 0, h = 0;
    assert_int_equal(backend_query_texture(tex, &fmt, &access, &w, &h), 0);
    assert_int_equal(w, 37);
    assert_int_equal(h, 23);
    assert_int_equal(access, SDL_TEXTUREACCESS_TARGET);
    SDL_DestroyTexture(tex);
}

static void test_measure_text_width(void **state)
{
    (void)state;
    /* degenerate inputs return 0 without a font */
    assert_int_equal(backend_measure_text_width(NULL, "abc"), 0);
    assert_int_equal(backend_measure_text_width(NULL, NULL), 0);
    char path[] = "app/assets/Roboto-Regular.ttf";
    DFont *font = backend_load_font(path, 16);
    if (!font) {
        /* font asset unavailable in this environment; nothing else to assert */
        print_message("skip: Roboto font not found\n");
        return;
    }
    assert_int_equal(backend_measure_text_width(font, NULL), 0);
    assert_int_equal(backend_measure_text_width(font, ""), 0);
    int w1 = backend_measure_text_width(font, "a");
    int w2 = backend_measure_text_width(font, "abc");
    assert_true(w1 > 0);
    assert_true(w2 > w1);
    /* font is cached and released by the backend at shutdown */
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    if (backend_init() != 0) {
        printf("backend_init failed\n");
        return 1;
    }
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_fill_rect, setup_target, teardown_target),
        cmocka_unit_test_setup_teardown(test_fill_rect_color_alpha, setup_target, teardown_target),
        cmocka_unit_test_setup_teardown(test_render_rect_border, setup_target, teardown_target),
        cmocka_unit_test_setup_teardown(test_render_rect_color, setup_target, teardown_target),
        cmocka_unit_test_setup_teardown(test_rounded_rect_fill, setup_target, teardown_target),
        cmocka_unit_test_setup_teardown(test_rounded_rect_with_border, setup_target, teardown_target),
        cmocka_unit_test_setup_teardown(test_line_horizontal, setup_target, teardown_target),
        cmocka_unit_test_setup_teardown(test_line_vertical, setup_target, teardown_target),
        cmocka_unit_test_setup_teardown(test_line_diagonal, setup_target, teardown_target),
        cmocka_unit_test_setup_teardown(test_shadow_offset_no_blur, setup_target, teardown_target),
        cmocka_unit_test_setup_teardown(test_shadow_blur, setup_target, teardown_target),
        cmocka_unit_test_setup_teardown(test_shadow_spread, setup_target, teardown_target),
        cmocka_unit_test(test_density),
        cmocka_unit_test(test_windowsize_roundtrip),
        cmocka_unit_test(test_window_title),
        cmocka_unit_test(test_window_resizable),
        cmocka_unit_test(test_window_minimum_size),
        cmocka_unit_test(test_query_texture),
        cmocka_unit_test(test_measure_text_width),
    };
    int rc = cmocka_run_group_tests(tests, NULL, NULL);
    backend_quit();
    return rc;
}
