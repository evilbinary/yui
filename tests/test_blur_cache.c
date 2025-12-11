#include <stdio.h>
#include <stdlib.h>
#include <SDL.h>
#include <SDL_image.h>
#include "../src/backend.h"

#if defined(_WIN32)
#include <windows.h>
int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    int argc = __argc;
    char** argv = __argv;
    return main(argc, argv);
}
#endif

// 简单的测试函数
void test_blur_cache() {
    printf("=== 测试毛玻璃效果缓存清理功能 ===\n");
    
    // 初始化后端
    if (backend_init() != 0) {
        printf("后端初始化失败\n");
        return;
    }
    
    // 创建一个简单的图层
    Layer ui_root;
    ui_root.rect.x = 0;
    ui_root.rect.y = 0;
    ui_root.rect.w = 800;
    ui_root.rect.h = 600;
    ui_root.child_count = 0;
    ui_root.children = NULL;
    ui_root.sub = NULL;
    
    printf("初始化完成，开始测试毛玻璃效果...\n");
    
    // 创建几个矩形区域用于测试毛玻璃效果
    Rect rect1 = {100, 100, 200, 150};
    Rect rect2 = {150, 120, 200, 150};  // 与rect1部分重叠
    Rect rect3 = {100, 100, 200, 150};  // 与rect1完全相同
    
    // 渲染背景
    backend_render_clear_color(50, 50, 80, 255);
    
    // 渲染几个矩形作为背景
    Rect bg_rect1 = {50, 50, 300, 200};
    Rect bg_rect2 = {200, 150, 300, 200};
    backend_render_fill_rect_color(&bg_rect1, 200, 100, 100, 255);
    backend_render_fill_rect_color(&bg_rect2, 100, 200, 100, 255);
    
    // 第一次渲染毛玻璃效果
    printf("第一次渲染毛玻璃效果 (rect1)...\n");
    backend_render_backdrop_filter(&rect1, 10, 1.0f, 1.0f);
    
    // 第二次渲染相同参数的毛玻璃效果（应该使用缓存）
    printf("第二次渲染相同参数的毛玻璃效果 (rect3)...\n");
    backend_render_backdrop_filter(&rect3, 10, 1.0f, 1.0f);
    
    // 第三次渲染不同参数的毛玻璃效果
    printf("第三次渲染不同参数的毛玻璃效果 (rect2)...\n");
    backend_render_backdrop_filter(&rect2, 15, 0.8f, 1.2f);
    
    // 显示结果
    backend_render_present();
    
    // 等待一段时间
    backend_delay(2000);
    
    // 测试缓存清理
    printf("测试缓存清理...\n");
    backend_quit();
    
    printf("测试完成\n");
}

int main(int argc, char *argv[]) {
    test_blur_cache();
    return 0;
}