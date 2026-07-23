#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <cmocka.h>

#include "ytype.h"
#include "layout.h"
#include "layer.h"

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

typedef struct {
    Layer parent;
    LayoutManager layout_mgr;
    Layer child1;
    Layer child2;
    Layer child3;
    Layer *children[3];
} ContentSizeFixture;

static int setup_vertical_tree(void **state)
{
    ContentSizeFixture *fx = calloc(1, sizeof(*fx));
    assert_non_null(fx);

    strcpy(fx->parent.id, "parent");
    fx->parent.rect = (Rect){100, 100, 400, 300};
    fx->parent.type = VIEW;

    fx->layout_mgr.type = LAYOUT_VERTICAL;
    fx->layout_mgr.spacing = 10;
    fx->layout_mgr.padding[0] = 10;
    fx->layout_mgr.padding[1] = 10;
    fx->layout_mgr.padding[2] = 10;
    fx->layout_mgr.padding[3] = 10;
    fx->parent.layout_manager = &fx->layout_mgr;

    strcpy(fx->child1.id, "child1");
    fx->child1.rect = (Rect){0, 0, 300, 50};
    fx->child1.fixed_width = 300;
    fx->child1.fixed_height = 50;
    fx->child1.type = VIEW;

    strcpy(fx->child2.id, "child2");
    fx->child2.rect = (Rect){0, 60, 280, 80};
    fx->child2.fixed_width = 280;
    fx->child2.fixed_height = 80;
    fx->child2.type = VIEW;

    strcpy(fx->child3.id, "child3");
    fx->child3.rect = (Rect){0, 150, 320, 60};
    fx->child3.fixed_width = 320;
    fx->child3.fixed_height = 60;
    fx->child3.type = VIEW;

    fx->children[0] = &fx->child1;
    fx->children[1] = &fx->child2;
    fx->children[2] = &fx->child3;
    fx->parent.children = fx->children;
    fx->parent.child_count = 3;
    fx->child1.parent = &fx->parent;
    fx->child2.parent = &fx->parent;
    fx->child3.parent = &fx->parent;

    *state = fx;
    return 0;
}

static int teardown_tree(void **state)
{
    free(*state);
    *state = NULL;
    return 0;
}

static void test_vertical_content_size(void **state)
{
    ContentSizeFixture *fx = *state;
    int expected_width;
    int expected_height;

    layout_layer(&fx->parent);

    expected_width = 320 + 20; /* widest child + horizontal padding */
    expected_height = 50 + 80 + 60 + 20 + 20; /* heights + spacings + padding */
    assert_int_equal(fx->parent.content_width, expected_width);
    assert_int_equal(fx->parent.content_height, expected_height);
}

static void test_horizontal_content_size(void **state)
{
    ContentSizeFixture *fx = *state;
    int expected_width;
    int expected_height;

    fx->layout_mgr.type = LAYOUT_HORIZONTAL;
    fx->parent.content_width = 0;
    fx->parent.content_height = 0;
    layout_layer(&fx->parent);

    expected_width = 300 + 280 + 320 + 20 + 20;
    expected_height = 80 + 20;
    assert_int_equal(fx->parent.content_width, expected_width);
    assert_int_equal(fx->parent.content_height, expected_height);
}

static void test_text_layer_layout_smoke(void **state)
{
    Layer text_layer;
    Font *font;

    (void)state;
    memset(&text_layer, 0, sizeof(text_layer));
    strcpy(text_layer.id, "text_layer");
    layer_set_text(&text_layer, "Hello World, this is a test text");
    text_layer.rect.w = 200;
    text_layer.rect.h = 50;
    text_layer.type = TEXT;
    font = calloc(1, sizeof(Font));
    assert_non_null(font);
    font->size = 16;
    text_layer.font = font;

    layout_layer(&text_layer);
    assert_non_null(layer_get_text(&text_layer));
    assert_true(strlen(layer_get_text(&text_layer)) > 0);

    layer_set_text(&text_layer, NULL);
    free(font);
}

int main(int argc, char **argv)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_vertical_content_size, setup_vertical_tree, teardown_tree),
        cmocka_unit_test_setup_teardown(test_horizontal_content_size, setup_vertical_tree, teardown_tree),
        cmocka_unit_test(test_text_layer_layout_smoke),
    };
    (void)argc;
    (void)argv;
    return cmocka_run_group_tests(tests, NULL, NULL);
}
