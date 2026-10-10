/*
 * Spatial focus navigation tests: direction moves pick the nearest focusable
 * layer in the requested direction, outermost focusable containers are atomic,
 * focusChildren containers descend into children, and activation fires onClick.
 */
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <cmocka.h>

#include "ytype.h"
#include "layer.h"
#include "render.h"
#include "focus.h"

int main(int argc, char **argv);

#if defined(_WIN32)
#include <windows.h>
int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    (void)hInstance; (void)hPrevInstance; (void)lpCmdLine; (void)nCmdShow;
    return main(__argc, __argv);
}
#endif

static Layer* make_layer(Layer* parent, const char* id, int x, int y, int w, int h)
{
    Layer* layer = layer_create(parent, x, y, w, h);
    assert_non_null(layer);
    strncpy(layer->id, id, sizeof(layer->id) - 1);
    layer->type = VIEW;
    layer->focusable = 1;
    layer->visible = VISIBLE;
    return layer;
}

static void attach(Layer* parent, Layer* child)
{
    Layer** arr = (Layer**)realloc(parent->children,
                                   sizeof(Layer*) * (parent->child_count + 1));
    assert_non_null(arr);
    parent->children = arr;
    parent->children[parent->child_count++] = child;
}

static int g_clicked;
static void* on_click(void* data) { (void)data; g_clicked++; return NULL; }

static void test_direction_moves(void **state)
{
    Layer* root;
    Layer* a;
    Layer* b;
    Layer* c;
    Layer* d;

    (void)state;
    focus_init();

    root = layer_create(NULL, 0, 0, 200, 200);
    assert_non_null(root);
    root->type = VIEW;
    root->visible = VISIBLE;

    a = make_layer(root, "a", 10, 10, 40, 40);
    b = make_layer(root, "b", 70, 10, 40, 40);
    c = make_layer(root, "c", 10, 70, 40, 40);
    d = make_layer(root, "d", 70, 70, 40, 40);
    attach(root, a);
    attach(root, b);
    attach(root, c);
    attach(root, d);

    assert_int_equal(focus_set(a), 1);
    assert_ptr_equal(focus_get(), a);

    assert_int_equal(focus_move(root, FOCUS_DIR_RIGHT), 1);
    assert_ptr_equal(focus_get(), b);
    assert_int_equal(focus_move(root, FOCUS_DIR_DOWN), 1);
    assert_ptr_equal(focus_get(), d);
    assert_int_equal(focus_move(root, FOCUS_DIR_LEFT), 1);
    assert_ptr_equal(focus_get(), c);
    assert_int_equal(focus_move(root, FOCUS_DIR_UP), 1);
    assert_ptr_equal(focus_get(), a);

    /* 边界方向没有候选，焦点保持 */
    assert_int_equal(focus_move(root, FOCUS_DIR_LEFT), 0);
    assert_ptr_equal(focus_get(), a);
    assert_int_equal(focus_move(root, FOCUS_DIR_UP), 0);
    assert_ptr_equal(focus_get(), a);

    focus_clear();
    assert_null(focus_get());

    free(root->children);
    render_ctx_free(root);
    free(a); free(b); free(c); free(d); free(root);
}

static void test_atomic_container_blocks_child(void **state)
{
    Layer* root;
    Layer* box;
    Layer* inner;
    Layer* other;

    (void)state;
    focus_init();

    root = layer_create(NULL, 0, 0, 200, 200);
    root->type = VIEW;
    root->visible = VISIBLE;

    /* box 可聚焦且为原子容器：inner 不应成为候选 */
    box = make_layer(root, "box", 10, 10, 80, 80);
    inner = make_layer(box, "inner", 20, 20, 40, 40);
    attach(box, inner);
    other = make_layer(root, "other", 120, 10, 40, 40);
    attach(root, box);
    attach(root, other);

    assert_int_equal(focus_set(box), 1);
    assert_int_equal(focus_move(root, FOCUS_DIR_RIGHT), 1);
    assert_ptr_equal(focus_get(), other);

    focus_clear();
    free(box->children);
    free(root->children);
    render_ctx_free(root);
    free(inner); free(box); free(other); free(root);
}

static void test_focus_children_descends(void **state)
{
    Layer* root;
    Layer* box;
    Layer* first;
    Layer* second;

    (void)state;
    focus_init();

    root = layer_create(NULL, 0, 0, 200, 200);
    root->type = VIEW;
    root->visible = VISIBLE;

    box = make_layer(root, "box", 10, 10, 180, 40);
    box->focus_children = 1;
    first = make_layer(box, "first", 20, 15, 40, 30);
    second = make_layer(box, "second", 70, 15, 40, 30);
    attach(box, first);
    attach(box, second);
    attach(root, box);

    /* 分组容器自身不被选中，初始焦点落在第一个子元素 */
    assert_int_equal(focus_move(root, FOCUS_DIR_RIGHT), 1);
    assert_ptr_equal(focus_get(), first);
    assert_int_equal(focus_move(root, FOCUS_DIR_RIGHT), 1);
    assert_ptr_equal(focus_get(), second);

    focus_clear();
    free(box->children);
    free(root->children);
    render_ctx_free(root);
    free(first); free(second); free(box); free(root);
}

static void test_activate_fires_click(void **state)
{
    Layer* root;
    Layer* a;
    Event ev;

    (void)state;
    focus_init();
    g_clicked = 0;

    root = layer_create(NULL, 0, 0, 100, 100);
    root->type = VIEW;
    root->visible = VISIBLE;
    a = make_layer(root, "a", 10, 10, 40, 40);
    attach(root, a);

    memset(&ev, 0, sizeof(ev));
    ev.click = on_click;
    a->event = &ev;

    assert_int_equal(focus_set(a), 1);
    assert_int_equal(focus_activate(), 1);
    assert_int_equal(g_clicked, 1);

    focus_clear();
    free(root->children);
    render_ctx_free(root);
    free(a); free(root);
}

static void test_destroy_clears_focus(void **state)
{
    Layer* root;
    Layer* a;

    (void)state;
    focus_init();
    root = layer_create(NULL, 0, 0, 100, 100);
    root->type = VIEW;
    root->visible = VISIBLE;
    a = make_layer(root, "a", 10, 10, 40, 40);
    attach(root, a);

    assert_int_equal(focus_set(a), 1);
    focus_on_layer_destroy(a);
    assert_null(focus_get());

    free(root->children);
    render_ctx_free(root);
    free(a); free(root);
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_direction_moves),
        cmocka_unit_test(test_atomic_container_blocks_child),
        cmocka_unit_test(test_focus_children_descends),
        cmocka_unit_test(test_activate_fires_click),
        cmocka_unit_test(test_destroy_clears_focus),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
