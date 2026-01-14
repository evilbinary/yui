#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/ytype.h"
#include "../src/layer.h"
#include "../src/components/text_component.h"

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
    printf("=== 简单测试文本设置和获取 ===\n");
    
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
    
    // 初始化文本字段
    layer_set_text(&test_layer, "");
    
    // 创建文本组件
    TextComponent* text_component = text_component_create(&test_layer);
    if (!text_component) {
        printf("错误：无法创建文本组件\n");
        return 1;
    }
    
    // 设置为多行模式
    text_component_set_multiline(text_component, 1);
    
    // 测试设置和获取文本
    const char* test_text = "这是第一行测试文本。\n这是第二行测试文本。\nThird line of text.\nThis is fourth line.\n这是第五行，用来测试多行文本渲染是否正常工作。";
    
    printf("设置文本...\n");
    text_component_set_text(text_component, test_text);
    
    // 获取文本并验证
    const char* retrieved_text = layer_get_text(&test_layer);
    printf("获取的文本：\n%s\n", retrieved_text);
    printf("文本长度：%zu 字符\n", strlen(retrieved_text));
    
    // 计算行数
    int line_count = 1;
    for (size_t i = 0; i < strlen(retrieved_text); i++) {
        if (retrieved_text[i] == '\n') {
            line_count++;
        }
    }
    printf("文本行数：%d 行\n", line_count);
    
    // 验证文本是否正确设置
    if (strcmp(test_text, retrieved_text) == 0) {
        printf("✅ 测试通过：文本设置和获取正确\n");
    } else {
        printf("❌ 测试失败：文本设置和获取不匹配\n");
    }
    
    // 清理资源
    text_component_destroy(text_component);
    
    printf("测试完成。\n");
    return 0;
}