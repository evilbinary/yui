/*
 * 回归：Label 擦除旧文字时若最近不透明祖先有圆角，
 * 应按祖先圆角形状擦除，不能把祖先圆角区域填成直角（会露出尖角、
 * 破坏焦点边框的圆角）。
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
#include "layer_update.h"
#include "render.h"
#include "backend.h"
#include "components/label_component.h"

extern SDL_Renderer *renderer;

int main(int argc, char **argv);

#if defined(_WIN32)
#include <windows.h>
int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    (void)hInstance; (void)hPrevInstance; (void)lpCmdLine; (void)nCmdShow;
    return main(__argc, __argv);
}
#endif

#define TEX_W 160
#define TEX_H 120

typedef struct { Uint8 r, g, b, a; } Pix;

static SDL_Texture *g_tex;

static int setup_target(void **state)
{
    (void)state;
    g_tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888,
                              SDL_TEXTUREACCESS_TARGET, TEX_W, TEX_H);
    if (!g_tex) return -1;
    if (SDL_SetRenderTarget(renderer, g_tex) != 0) {
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

static void test_label_erase_respects_parent_radius(void **state)
{
    Layer *parent;
    Layer *label;
    Pix p;

    (void)state;

    parent = layer_create(NULL, 0, 0, 100, 60);
    assert_non_null(parent);
    parent->type = VIEW;
    parent->visible = VISIBLE;
    parent->bg_color = (Color){0, 0, 200, 255};
    parent->radius = 16;

    /* Label 覆盖父 View 底部左右圆角区域 */
    label = layer_create(parent, 2, 40, 96, 20);
    assert_non_null(label);
    label->type = LABEL;
    label->visible = VISIBLE;
    label_component_create_from_json(label, NULL);
    layer_set_text(label, "x");

    parent->children = (Layer **)malloc(sizeof(Layer *));
    assert_non_null(parent->children);
    parent->children[0] = label;
    parent->child_count = 1;

    render_layer(parent);

    /* 左下圆角外（父圆角切掉的区域）应保持背景，不能是父蓝色 */
    assert_int_equal(read_pixel(2, 58, &p), 0);
    expect_close("corner_out_left", 2, 58, p, 0, 0, 0, 24);
    assert_int_equal(read_pixel(97, 58, &p), 0);
    expect_close("corner_out_right", 97, 58, p, 0, 0, 0, 24);

    /* 父内部仍是父蓝色 */
    assert_int_equal(read_pixel(50, 10, &p), 0);
    expect_close("inside", 50, 10, p, 0, 0, 200, 24);

    destroy_layer(parent);
}

/* Label 擦除不应覆盖祖先边框（焦点边框的直线/圆角） */
static void test_label_erase_respects_parent_border(void **state)
{
    Layer *parent;
    Layer *label;
    Pix p;

    (void)state;

    parent = layer_create(NULL, 0, 0, 100, 60);
    assert_non_null(parent);
    parent->type = VIEW;
    parent->visible = VISIBLE;
    parent->bg_color = (Color){0, 0, 200, 255};
    parent->radius = 16;
    parent->border.width = 2;
    parent->border.style = LAYER_BORDER_SOLID;
    parent->border.color = (Color){255, 200, 0, 255};

    label = layer_create(parent, 2, 40, 96, 20);
    assert_non_null(label);
    label->type = LABEL;
    label->visible = VISIBLE;
    label_component_create_from_json(label, NULL);
    layer_set_text(label, "x");

    parent->children = (Layer **)malloc(sizeof(Layer *));
    assert_non_null(parent->children);
    parent->children[0] = label;
    parent->child_count = 1;

    render_layer(parent);

    /* 底边中间应保留黄色边框 */
    assert_int_equal(read_pixel(50, 59, &p), 0);
    expect_close("border_bottom", 50, 59, p, 255, 200, 0, 40);
    /* 内部仍是父蓝色 */
    assert_int_equal(read_pixel(50, 20, &p), 0);
    expect_close("border_inside", 50, 20, p, 0, 0, 200, 30);

    destroy_layer(parent);
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
        cmocka_unit_test_setup_teardown(test_label_erase_respects_parent_radius,
                                        setup_target, teardown_target),
        cmocka_unit_test_setup_teardown(test_label_erase_respects_parent_border,
                                        setup_target, teardown_target),
    };
    int rc = cmocka_run_group_tests(tests, NULL, NULL);
    backend_quit();
    return rc;
}
