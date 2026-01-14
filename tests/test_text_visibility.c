#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/ytype.h"
#include "../src/layer.h"
#include "../src/components/text_component.h"
#include "../src/backend.h"
#include "../src/render.h"

#if defined(_WIN32)
#include <windows.h>
int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    extern int __argc;
    extern char** __argv;
    return main(__argc, __argv);
}
#endif

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    printf("=== 测试文本可见性 ===\n");
    
    // 初始化后端（简化版本，不创建窗口）
    printf("初始化后端...\n");
    if (!backend_init(800, 600)) {
        printf("错误：无法初始化后端\n");
        return 1;
    }
    
    // 创建一个测试图层
    Layer test_layer;
    memset(&test_layer, 0, sizeof(Layer));
    strcpy(test_layer.id, "testText");
    test_layer.rect.x = 50;
    test_layer.rect.y = 50;
    test_layer.rect.w = 400;
    test_layer.rect.h = 300;
    test_layer.type = TEXT;
    test_layer.bg_color = (Color){255, 255, 255, 255};
    test_layer.color = (Color){0, 0, 0, 255};
    
    // 设置文本
    const char* test_text = "这是第一行测试文本。\n这是第二行测试文本。\nThird line of text.\nThis is fourth line.\n这是第五行，用来测试多行文本渲染是否正常工作。";
    layer_set_text(&test_layer, test_text);
    
    // 创建文本组件
    TextComponent* text_component = text_component_create(&test_layer);
    if (!text_component) {
        printf("错误：无法创建文本组件\n");
        backend_cleanup();
        return 1;
    }
    
    // 设置为多行模式
    text_component_set_multiline(text_component, 1);
    
    printf("测试文本设置和获取...\n");
    const char* retrieved_text = layer_get_text(&test_layer);
    printf("设置的文本：\n%s\n", retrieved_text);
    printf("文本长度：%zu 字符\n", strlen(retrieved_text));
    
    // 模拟渲染（不需要窗口）
    printf("模拟渲染文本组件...\n");
    backend_render_clear();
    
    // 设置全局字体（简化测试）
    Font font;
    memset(&font, 0, sizeof(Font));
    font.default_font = backend_load_font("Roboto-Regular.ttf", 14);
    test_layer.font = &font;
    
    // 渲染文本组件（测试我们的修复）
    text_component_render(&test_layer);
    backend_render_present();
    
    printf("渲染完成。如果修复成功，文本应该可见。\n");
    
    // 清理资源
    text_component_destroy(text_component);
    backend_cleanup();
    
    printf("测试完成。\n");
    return 0;
}