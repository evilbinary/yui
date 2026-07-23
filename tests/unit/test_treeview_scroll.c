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

static void test_treeview_content_and_scroll_to_node(void **state)
{
    Layer parent;
    TreeViewComponent *treeview;
    int content_height;
    int i;
    int before_offset;
    TreeNode *target;

    (void)state;
    memset(&parent, 0, sizeof(parent));
    strcpy(parent.id, "treeview_parent");
    parent.rect = (Rect){50, 50, 300, 200};
    parent.type = TREEVIEW;
    parent.scrollable = 1;

    treeview = treeview_component_create(&parent);
    assert_non_null(treeview);

    for (i = 0; i < 20; i++) {
        char node_text[50];
        TreeNode *node;
        sprintf(node_text, "Node %d", i);
        node = treeview_create_node(node_text);
        assert_non_null(node);
        assert_true(treeview_add_root_node(treeview, node) >= 0);

        if (i % 3 == 0) {
            int j;
            for (j = 0; j < 3; j++) {
                char child_text[50];
                TreeNode *child;
                sprintf(child_text, "Child %d-%d", i, j);
                child = treeview_create_node(child_text);
                assert_non_null(child);
                treeview_add_child_node(node, child);
            }
            treeview_expand_node(node);
        }
    }

    content_height = treeview_calculate_content_height(treeview);
    assert_true(content_height > parent.rect.h);
    assert_int_equal(treeview->root_count, 20);

    parent.scroll_offset = 0;
    treeview_update_scrollbar(treeview);

    parent.scroll_offset = 50;
    treeview_update_scrollbar(treeview);
    assert_int_equal(parent.scroll_offset, 50);

    parent.scroll_offset = 0;
    target = treeview->root_nodes[10];
    treeview_scroll_to_node(treeview, target);
    {
        int max_offset = content_height - parent.rect.h;
        if (max_offset < 0) {
            max_offset = 0;
        }
        assert_true(parent.scroll_offset >= 0);
        assert_true(parent.scroll_offset <= max_offset);
        before_offset = parent.scroll_offset;
    }

    /* target further down should not decrease offset when starting from 0 path above */
    treeview_scroll_to_node(treeview, treeview->root_nodes[15]);
    assert_true(parent.scroll_offset >= before_offset);

    treeview_component_destroy(treeview);
}

int main(int argc, char **argv)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_treeview_content_and_scroll_to_node),
    };
    (void)argc;
    (void)argv;
    return cmocka_run_group_tests(tests, NULL, NULL);
}
