#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/ytype.h"
#include "../src/layout.h"
#include "../src/components/treeview_component.h"
#include "../src/components/scrollbar_component.h"

#if defined(_WIN32)
#include <windows.h>
int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    int argc = __argc;
    char** argv = __argv;
    return main(argc, argv);
}
#endif

// 模拟后端函数（简化测试）
void mock_backend_init() {
    printf("Mock backend initialized\n");
}

// 简单的TreeView滚动测试
void test_treeview_scroll() {
    printf("=== 测试TreeView滚动功能 ===\n");
    
    // 创建父layer
    Layer parent;
    memset(&parent, 0, sizeof(Layer));
    strcpy(parent.id, "treeview_parent");
    parent.rect.x = 50;
    parent.rect.y = 50;
    parent.rect.w = 300;
    parent.rect.h = 200;
    parent.type = TREEVIEW;
    parent.scrollable = 1; // 启用垂直滚动
    
    // 创建TreeView组件
    TreeViewComponent* treeview = treeview_component_create(&parent);
    if (!treeview) {
        printf("ERROR: Failed to create TreeView component\n");
        return;
    }
    
    // 添加多个测试节点
    for (int i = 0; i < 20; i++) {
        char node_text[50];
        sprintf(node_text, "Node %d", i);
        TreeNode* node = treeview_create_node(node_text);
        if (node) {
            treeview_add_root_node(treeview, node);
            
            // 为部分节点添加子节点
            if (i % 3 == 0) {
                for (int j = 0; j < 3; j++) {
                    char child_text[50];
                    sprintf(child_text, "Child %d-%d", i, j);
                    TreeNode* child = treeview_create_node(child_text);
                    if (child) {
                        treeview_add_child_node(node, child);
                    }
                }
                // 展开节点
                treeview_expand_node(node);
            }
        }
    }
    
    // 创建滚动条组件
    Layer scrollbar_layer;
    memset(&scrollbar_layer, 0, sizeof(Layer));
    strcpy(scrollbar_layer.id, "scrollbar");
    scrollbar_layer.rect.x = parent.rect.x + parent.rect.w - 8;
    scrollbar_layer.rect.y = parent.rect.y;
    scrollbar_layer.rect.w = 8;
    scrollbar_layer.rect.h = parent.rect.h;
    scrollbar_layer.type = SCROLLBAR;
    
    ScrollbarComponent* scrollbar = scrollbar_component_create(&scrollbar_layer, &parent, SCROLLBAR_DIRECTION_VERTICAL);
    if (!scrollbar) {
        printf("ERROR: Failed to create Scrollbar component\n");
        treeview_component_destroy(treeview);
        return;
    }
    
    // 设置滚动条关联
    parent.scrollbar_v = scrollbar;
    
    // 测试内容高度计算
    int content_height = treeview_calculate_content_height(treeview);
    printf("TreeView content height: %d\n", content_height);
    printf("TreeView visible height: %d\n", parent.rect.h);
    printf("Should show scrollbar: %s\n", content_height > parent.rect.h ? "YES" : "NO");
    
    // 测试滚动条更新
    treeview_update_scrollbar(treeview);
    printf("Scrollbar visible: %d\n", scrollbar->visible);
    
    // 测试滚动偏移
    printf("Initial scroll offset: %d\n", parent.scroll_offset);
    
    // 模拟滚动
    parent.scroll_offset = 50;
    treeview_update_scrollbar(treeview);
    printf("After scroll to 50, scrollbar thumb position: %d\n", scrollbar->thumb_rect.y);
    
    // 测试滚动到节点
    if (treeview->root_count > 10) {
        TreeNode* target_node = treeview->root_nodes[10];
        treeview_scroll_to_node(treeview, target_node);
        printf("After scroll to node 10, scroll offset: %d\n", parent.scroll_offset);
    }
    
    // 测试边界情况
    parent.scroll_offset = 1000; // 超出范围
    treeview_scroll_to_node(treeview, treeview->root_count > 15 ? treeview->root_nodes[15] : treeview->root_nodes[0]);
    printf("After attempting over-scroll, scroll offset: %d\n", parent.scroll_offset);
    
    // 清理
    treeview_component_destroy(treeview);
    printf("TreeView scroll test completed\n");
}

int main(int argc, char *argv[]) {
    mock_backend_init();
    test_treeview_scroll();
    return 0;
}