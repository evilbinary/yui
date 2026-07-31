/*
 * Temporary pixel-verification for backend_render_arc after optimization.
 * Uses SDL_RenderReadPixels directly; renderer is a non-static global in
 * backend_sdl.c. Verifies full-circle ring, partial-arc wedge coverage,
 * and anti-aliasing edges.
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

#define PITCH 900 * 4

typedef struct { Uint8 r, g, b, a; } Pix;
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

static void fill_clear(Uint8 r, Uint8 g, Uint8 b)
{
    SDL_SetRenderDrawColor(renderer, r, g, b, 255);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);
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

static void test_arc_full_circle(void **state)
{
    (void)state;
    Color c = {255, 0, 128, 255};
    int cx = 250, cy = 250, r = 80, w = 4;

    fill_clear(0, 0, 0);
    backend_render_arc(cx, cy, r, 0.0f, 360.0f, c, w);
    backend_render_present();

    Pix p;
    /* on the ring: top, right, bottom, left, diagonals */
    int checks[][2] = {{cx, cy - r}, {cx + r, cy}, {cx, cy + r}, {cx - r, cy},
                       {cx + (int)(r * 0.7071f), cy - (int)(r * 0.7071f)},
                       {cx - (int)(r * 0.7071f), cy + (int)(r * 0.7071f)}};
    for (size_t i = 0; i < sizeof(checks) / sizeof(checks[0]); i++) {
        assert_int_equal(read_pixel(checks[i][0], checks[i][1], &p), 0);
        expect_close("ring", checks[i][0], checks[i][1], p, 255, 0, 128, 40);
    }
    /* center hollow */
    assert_int_equal(read_pixel(cx, cy, &p), 0);
    expect_close("center", cx, cy, p, 0, 0, 0, 10);
    /* inside hole */
    assert_int_equal(read_pixel(cx, cy - r + r / 2, &p), 0);
    expect_close("hole", cx, cy - r / 2, p, 0, 0, 0, 10);
    /* outside */
    assert_int_equal(read_pixel(cx + r + 30, cy, &p), 0);
    expect_close("outside", cx + r + 30, cy, p, 0, 0, 0, 10);
}

static void test_arc_partial(void **state)
{
    (void)state;
    Color c = {0, 200, 255, 255};
    int cx = 250, cy = 250, r = 80, w = 4;
    /* 0deg = top, clockwise. Sweep 90..180 = bottom-right quadrant. */
    float sa = 90.0f, ea = 180.0f;

    fill_clear(0, 0, 0);
    backend_render_arc(cx, cy, r, sa, ea, c, w);
    backend_render_present();

    Pix p;
    /* angle 90 = right (bottom of the sweep) */
    assert_int_equal(read_pixel(cx + r, cy, &p), 0);
    expect_close("wedge90", cx + r, cy, p, 0, 200, 255, 40);
    /* angle 180 = down */
    assert_int_equal(read_pixel(cx, cy + r, &p), 0);
    expect_close("wedge180", cx, cy + r, p, 0, 200, 255, 40);
    /* angle 135 = bottom-right diagonal */
    assert_int_equal(read_pixel(cx + (int)(r * 0.7071f), cy + (int)(r * 0.7071f), &p), 0);
    expect_close("wedge135", cx + (int)(r * 0.7071f), cy + (int)(r * 0.7071f), p, 0, 200, 255, 40);

    /* top must be empty (angle 0 not in 90..180) */
    assert_int_equal(read_pixel(cx, cy - r, &p), 0);
    expect_close("outside_top", cx, cy - r, p, 0, 0, 0, 10);
    /* left must be empty */
    assert_int_equal(read_pixel(cx - r, cy, &p), 0);
    expect_close("outside_left", cx - r, cy, p, 0, 0, 0, 10);
    /* just outside start edge: angle 90+12 = ~102 still inside, so check angle 60 -> empty */
    float a60 = 60.0f * M_PI / 180.0f;
    assert_int_equal(read_pixel(cx + (int)(r * cosf(a60)), cy + (int)(r * sinf(a60)), &p), 0);
    expect_close("outside_60", cx + (int)(r * cosf(a60)), cy + (int)(r * sinf(a60)), p, 0, 0, 0, 10);
}

static void test_arc_spinner(void **state)
{
    (void)state;
    Color c = {255, 220, 0, 255};
    int cx = 250, cy = 250, r = 60, w = 3;
    /* loading spinner style: 45..135 (top-right quadrant) */
    float sa = 45.0f, ea = 135.0f;

    fill_clear(0, 0, 0);
    backend_render_arc(cx, cy, r, sa, ea, c, w);
    backend_render_present();

    Pix p;
    /* angle 90 = right */
    assert_int_equal(read_pixel(cx + r, cy, &p), 0);
    expect_close("spin90", cx + r, cy, p, 255, 220, 0, 40);
    /* angle 45 = top-right diagonal */
    float a45 = 45.0f * M_PI / 180.0f;
    assert_int_equal(read_pixel(cx + (int)(r * sinf(a45)), cy - (int)(r * cosf(a45)), &p), 0);
    expect_close("spin45", cx + (int)(r * sinf(a45)), cy - (int)(r * cosf(a45)), p, 255, 220, 0, 40);
    /* angle 135 = bottom-right diagonal */
    float a135 = 135.0f * M_PI / 180.0f;
    assert_int_equal(read_pixel(cx + (int)(r * sinf(a135)), cy - (int)(r * cosf(a135)), &p), 0);
    expect_close("spin135", cx + (int)(r * sinf(a135)), cy - (int)(r * cosf(a135)), p, 255, 220, 0, 40);

    /* bottom must be empty */
    assert_int_equal(read_pixel(cx, cy + r, &p), 0);
    expect_close("spin_bottom_empty", cx, cy + r, p, 0, 0, 0, 10);
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
        cmocka_unit_test(test_arc_full_circle),
        cmocka_unit_test(test_arc_partial),
        cmocka_unit_test(test_arc_spinner),
    };
    int rc = cmocka_run_group_tests(tests, NULL, NULL);
    backend_quit();
    return rc;
}
