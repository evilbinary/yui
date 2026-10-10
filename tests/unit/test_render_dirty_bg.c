/*
 * Regression test for DIRTY-mode opaque View background repaint.
 *
 * When an ancestor repaints its opaque background (scroll / layout / child
 * change), every opaque descendant it covers must repaint its own background
 * too.  The draw_cover_bg optimization used to skip that whenever the child's
 * own dirty_flags were DIRTY_NONE, so scrolling a list left the row/card
 * backgrounds transparent (only labels were redrawn on top of the page bg).
 */
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <cmocka.h>
#include <SDL.h>

#include "ytype.h"
#include "layer.h"
#include "render.h"
#include "layer_update.h"
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

#define TEX_W 200
#define TEX_H 200

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

static void test_ancestor_repaint_redraws_child_bg(void **state)
{
    Layer *root;
    Layer *child;
    Pix p;

    (void)state;

    backend_set_render_mode(YUI_RENDER_MODE_DIRTY);

    root = layer_create(NULL, 0, 0, TEX_W, TEX_H);
    assert_non_null(root);
    root->type = VIEW;
    root->bg_color = (Color){200, 0, 0, 255};

    child = layer_create(root, 50, 50, 100, 100);
    assert_non_null(child);
    child->type = VIEW;
    child->bg_color = (Color){0, 0, 200, 255};

    root->children = (Layer **)malloc(sizeof(Layer *));
    assert_non_null(root->children);
    root->children[0] = child;
    root->child_count = 1;

    /* first frame: everything is drawn */
    render_layer(root);
    assert_int_equal(read_pixel(100, 100, &p), 0);
    expect_close("child_first", 100, 100, p, 0, 0, 200, 8);
    assert_int_equal(read_pixel(10, 10, &p), 0);
    expect_close("root_first", 10, 10, p, 200, 0, 0, 8);

    /* simulate a scroll/layout refresh on the ancestor only.  Its opaque
     * background is repainted over the whole area, so the child must repaint
     * its own background as well. */
    root->dirty_flags |= DIRTY_LAYOUT;
    render_layer(root);
    assert_int_equal(read_pixel(100, 100, &p), 0);
    expect_close("child_after_repaint", 100, 100, p, 0, 0, 200, 8);
    assert_int_equal(read_pixel(10, 10, &p), 0);
    expect_close("root_after_repaint", 10, 10, p, 200, 0, 0, 8);

    free(root->children);
    render_ctx_free(root);
    free(child);
    free(root);
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
        cmocka_unit_test_setup_teardown(test_ancestor_repaint_redraws_child_bg,
                                        setup_target, teardown_target),
    };
    int rc = cmocka_run_group_tests(tests, NULL, NULL);
    backend_quit();
    return rc;
}
