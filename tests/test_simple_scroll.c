#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/ytype.h"
#include "../src/layout.h"
#include "../src/components/treeview_component.h"
#include "../src/components/scrollbar_component.h"

// 简单测试TreeView滚动
void test_simple_scroll() {
    printf("=== 简单TreeView滚动测试 ===\n");
    
    // 创建父layer
    Layer parent;
    memset(&parent, 0, sizeof(Layer));
    strcpy(parent.id, "test_treeview");
    parent.rect.x = 100;
    parent.rect.y = 100;
    parent.rect.w = 200;
    parent.rect.h = 150;
    parent.type = TREEVIEW;
    parent.scrollable = 1;
    parent.scroll_offset = 0;
    
    // 创建TreeView组件
    TreeViewComponent* treeview = treeview_component_create(&parent);
    if (!treeview) {
        printf("ERROR: 无法创建TreeView组件\n");
        return;
    }
    
    // 添加10个节点
    for (int i = 0; i < 10; i++) {
        char text[32];
        sprintf(text, "项目 %d", i);
        TreeNode* node = treeview_create_node(text);
        if (node) {
            treeview_add_root_node(treeview, node);
        }
    }
    
    // 计算内容高度
    int content_height = treeview_calculate_content_height(treeview);
    printf("内容高度: %d\n", content_height);
    printf("可见高度: %d\n", parent.rect.h);
    printf("需要滚动条: %s\n", content_height > parent.rect.h ? "是" : "否");
    
    // 测试不同滚动偏移
    for (int offset = 0; offset < content_height - parent.rect.h; offset += 30) {
        printf("\n--- 滚动偏移: %d ---\n", offset);
        parent.scroll_offset = offset;
        
        // 更新滚动条
        treeview_update_scrollbar(treeview);
        
        // 计算可见节点
        int visible_start = offset / treeview->item_height;
        int visible_end = (offset + parent.rect.h) / treeview->item_height;
        if (visible_end >= treeview->root_count) visible_end = treeview->root_count - 1;
        
        printf("可见节点范围: %d - %d\n", visible_start, visible_end);
        
        for (int i = visible_start; i <= visible_end && i < treeview->root_count; i++) {
            TreeNode* node = treeview->root_nodes[i];
            int node_y = parent.rect.y + i * treeview->item_height - offset;
            printf("  节点 %s: Y=%d\n", node->text, node_y);
        }
    }
    
    // 清理
    treeview_component_destroy(treeview);
    printf("简单滚动测试完成\n");
}

int main() {
    test_simple_scroll();
    return 0;
}