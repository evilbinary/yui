#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/ytype.h"
#include "../src/layout.h"

#if defined(_WIN32)
#include <windows.h>
int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    int argc = __argc;
    char** argv = __argv;
    return main(argc, argv);
}
#endif
// 简单的测试函数
void test_content_size_calculation() {
    printf("=== 测试内容尺寸计算算法 ===\n");
    
    // 创建一个父layer
    Layer parent;
    memset(&parent, 0, sizeof(Layer));
    strcpy(parent.id, "parent");
    parent.rect.x = 100;
    parent.rect.y = 100;
    parent.rect.w = 400;
    parent.rect.h = 300;
    parent.type = VIEW;
    
    // 设置布局管理器
    LayoutManager layout_mgr;
    memset(&layout_mgr, 0, sizeof(LayoutManager));
    layout_mgr.type = LAYOUT_VERTICAL;
    layout_mgr.spacing = 10;
    layout_mgr.padding[0] = 10; // top
    layout_mgr.padding[1] = 10; // right
    layout_mgr.padding[2] = 10; // bottom
    layout_mgr.padding[3] = 10; // left
    parent.layout_manager = &layout_mgr;
    
    // 创建子layers
    Layer child1, child2, child3;
    memset(&child1, 0, sizeof(Layer));
    memset(&child2, 0, sizeof(Layer));
    memset(&child3, 0, sizeof(Layer));
    
    strcpy(child1.id, "child1");
    child1.rect.x = 0;
    child1.rect.y = 0;
    child1.rect.w = 300;
    child1.rect.h = 50;
    child1.type = VIEW;
    
    strcpy(child2.id, "child2");
    child2.rect.x = 0;
    child2.rect.y = 60;
    child2.rect.w = 280;
    child2.rect.h = 80;
    child2.type = VIEW;
    
    strcpy(child3.id, "child3");
    child3.rect.x = 0;
    child3.rect.y = 150;
    child3.rect.w = 320;
    child3.rect.h = 60;
    child3.type = VIEW;
    
    // 设置父layer的children
    parent.child_count = 3;
    parent.children = malloc(3 * sizeof(Layer*));
    parent.children[0] = &child1;
    parent.children[1] = &child2;
    parent.children[2] = &child3;
    
    // 设置子layer的parent指针
    child1.parent = &parent;
    child2.parent = &parent;
    child3.parent = &parent;
    
    printf("布局前:\n");
    printf("  父layer: %d x %d\n", parent.rect.w, parent.rect.h);
    printf("  子元素数量: %d\n", parent.child_count);
    printf("  子元素尺寸:\n");
    for (int i = 0; i < parent.child_count; i++) {
        printf("    %s: %d x %d\n", 
               parent.children[i]->id,
               parent.children[i]->rect.w,
               parent.children[i]->rect.h);
    }
    
    // 执行布局
    layout_layer(&parent);
    
    printf("\n布局后:\n");
    printf("  父layer content_size: %d x %d\n", parent.content_width, parent.content_height);
    printf("  子元素位置:\n");
    for (int i = 0; i < parent.child_count; i++) {
        printf("    %s: pos=(%d,%d), size=%dx%d\n", 
               parent.children[i]->id,
               parent.children[i]->rect.x,
               parent.children[i]->rect.y,
               parent.children[i]->rect.w,
               parent.children[i]->rect.h);
    }
    
    // 验证计算结果
    int expected_width = 300 + 20; // 最宽子元素 + 左右padding
    int expected_height = 50 + 80 + 60 + 20 + 20; // 所有子元素高度 + spacing + padding
    
    printf("\n验证结果:\n");
    printf("  期望宽度: %d, 实际宽度: %d %s\n", 
           expected_width, parent.content_width,
           (expected_width == parent.content_width) ? "✓" : "✗");
    printf("  期望高度: %d, 实际高度: %d %s\n", 
           expected_height, parent.content_height,
           (expected_height == parent.content_height) ? "✓" : "✗");
    
    // 测试水平布局
    printf("\n=== 测试水平布局 ===\n");
    layout_mgr.type = LAYOUT_HORIZONTAL;
    parent.content_width = 0;
    parent.content_height = 0;
    
    layout_layer(&parent);
    
    expected_width = 300 + 280 + 320 + 20 + 20; // 所有子元素宽度 + spacing + padding
    expected_height = 80 + 20; // 最高子元素 + padding
    
    printf("水平布局结果:\n");
    printf("  期望宽度: %d, 实际宽度: %d %s\n", 
           expected_width, parent.content_width,
           (expected_width == parent.content_width) ? "✓" : "✗");
    printf("  期望高度: %d, 实际高度: %d %s\n", 
           expected_height, parent.content_height,
           (expected_height == parent.content_height) ? "✓" : "✗");
    
    // 测试文本组件
    printf("\n=== 测试文本组件 ===\n");
    Layer text_layer;
    memset(&text_layer, 0, sizeof(Layer));
    strcpy(text_layer.id, "text_layer");
    strcpy(text_layer.text, "Hello World, this is a test text");
    text_layer.rect.w = 200;
    text_layer.rect.h = 50;
    text_layer.type = TEXT;
    text_layer.font = malloc(sizeof(Font));
    text_layer.font->size = 16;
    
    layout_layer(&text_layer);
    
    printf("文本组件: '%s'\n", text_layer.text);
    printf("  内容尺寸: %d x %d\n", text_layer.content_width, text_layer.content_height);
    printf("  文本长度: %zu 字符\n", strlen(text_layer.text));
    
    // 清理内存
    free(parent.children);
    free(text_layer.font);
    
    printf("\n测试完成!\n");
}

int main(int argc, char *argv[]) {
    test_content_size_calculation();
    return 0;
}