#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <cmocka.h>

#include "ytype.h"
#include "components/treeview_component.h"

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

static void test_simple_treeview_scroll_math(void **state)
{
    Layer parent;
    TreeViewComponent *treeview;
    int content_height;
    int i;
    int offset;

    (void)state;
    memset(&parent, 0, sizeof(parent));
    strcpy(parent.id, "test_treeview");
    parent.rect = (Rect){100, 100, 200, 150};
    parent.type = TREEVIEW;
    parent.scrollable = 1;

    treeview = treeview_component_create(&parent);
    assert_non_null(treeview);

    for (i = 0; i < 10; i++) {
        char text[32];
        TreeNode *node;
        sprintf(text, "项目 %d", i);
        node = treeview_create_node(text);
        assert_non_null(node);
        assert_true(treeview_add_root_node(treeview, node) >= 0);
    }

    content_height = treeview_calculate_content_height(treeview);
    assert_true(content_height > parent.rect.h);
    assert_int_equal(treeview->root_count, 10);

    for (offset = 0; offset < content_height - parent.rect.h; offset += 30) {
        int visible_start;
        int visible_end;

        parent.scroll_offset = offset;
        treeview_update_scrollbar(treeview);

        visible_start = offset / treeview->item_height;
        visible_end = (offset + parent.rect.h) / treeview->item_height;
        if (visible_end >= treeview->root_count) {
            visible_end = treeview->root_count - 1;
        }
        assert_true(visible_start >= 0);
        assert_true(visible_end >= visible_start);
        assert_true(visible_end < treeview->root_count);
    }

    treeview_component_destroy(treeview);
}

int main(int argc, char **argv)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_simple_treeview_scroll_math),
    };
    (void)argc;
    (void)argv;
    return cmocka_run_group_tests(tests, NULL, NULL);
}
