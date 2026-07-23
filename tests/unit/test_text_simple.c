#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <cmocka.h>

#include "ytype.h"
#include "layer.h"
#include "components/text_component.h"

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

static const char *SAMPLE =
    "这是第一行测试文本。\n这是第二行测试文本。\nThird line of text.\nThis is fourth line.\n这是第五行，用来测试多行文本渲染是否正常工作。";

static int count_lines(const char *text)
{
    int lines = 1;
    size_t i;
    if (!text || !text[0]) {
        return 0;
    }
    for (i = 0; text[i]; i++) {
        if (text[i] == '\n') {
            lines++;
        }
    }
    return lines;
}

static void test_text_set_get_roundtrip(void **state)
{
    Layer layer;
    TextComponent *comp;
    const char *got;

    (void)state;
    memset(&layer, 0, sizeof(layer));
    strcpy(layer.id, "testText");
    layer.rect = (Rect){50, 50, 400, 300};
    layer.type = TEXT;
    layer.bg_color = (Color){255, 255, 255, 255};
    layer.color = (Color){0, 0, 0, 255};
    layer_set_text(&layer, "");

    comp = text_component_create(&layer);
    assert_non_null(comp);
    text_component_set_multiline(comp, 1);
    text_component_set_text(comp, SAMPLE);

    got = layer_get_text(&layer);
    assert_non_null(got);
    assert_string_equal(got, SAMPLE);
    assert_int_equal(count_lines(got), 5);

    text_component_destroy(comp);
}

int main(int argc, char **argv)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_text_set_get_roundtrip),
    };
    (void)argc;
    (void)argv;
    return cmocka_run_group_tests(tests, NULL, NULL);
}
