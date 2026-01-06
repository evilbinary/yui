#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/ytype.h"
#include "../src/layer.h"
#include "../src/components/text_component.h"

#if defined(_WIN32)
#include <windows.h>
int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    int argc = __argc;
    char** argv = __argv;
    return main(argc, argv);
}
#endif

// 简单的文本渲染测试
void test_multiline_text_rendering() {
    printf("=== 测试多行文本渲染 ===\n");
    
    // 初始化后端（简化的测试，不使用实际渲染）
    printf("初始化文本组件...\n");
    
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
    layer_set_text(&test_layer, "这是第一行测试文本。\n这是第二行测试文本。\nThird line of text.\nThis is the fourth line.\n这是第五行，用来测试多行文本渲染是否正常工作。");
    
    // 创建文本组件
    TextComponent* text_component = text_component_create(&test_layer);
    if (!text_component) {
        printf("错误：无法创建文本组件\n");
        return;
    }
    
    // 设置为多行模式
    text_component_set_multiline(text_component, 1);
    
    // 获取文本内容
    const char* text = layer_get_text(&test_layer);
    printf("设置的文本内容：\n%s\n", text);
    printf("文本长度：%zu 字符\n", strlen(text));
    
    // 计算行数
    int line_count = 1;
    for (size_t i = 0; i < strlen(text); i++) {
        if (text[i] == '\n') {
            line_count++;
        }
    }
    printf("文本行数：%d 行\n", line_count);
    
    // 测试文本设置函数
    printf("\n测试文本设置函数...\n");
    text_component_set_text(text_component, "新设置的测试文本\n第二行");
    
    // 验证文本是否正确更新
    const char* updated_text = layer_get_text(&test_layer);
    printf("更新后的文本内容：\n%s\n", updated_text);
    printf("更新后文本长度：%zu 字符\n", strlen(updated_text));
    
    // 计算更新后的行数
    line_count = 1;
    for (size_t i = 0; i < strlen(updated_text); i++) {
        if (updated_text[i] == '\n') {
            line_count++;
        }
    }
    printf("更新后文本行数：%d 行\n", line_count);
    
    // 清理资源
    text_component_destroy(text_component);
    printf("测试完成。\n");
}

int main() {
    printf("开始测试文本组件渲染...\n\n");
    
    test_multiline_text_rendering();
    
    printf("\n所有测试完成。\n");
    return 0;
}