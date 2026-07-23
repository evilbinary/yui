#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <cmocka.h>

#include "ytype.h"
#include "layer.h"
#include "backend.h"
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

static int setup_backend(void **state)
{
    (void)state;
    if (backend_init() != 0) {
        return -1; /* skip if SDL/backend unavailable */
    }
    return 0;
}

static int teardown_backend(void **state)
{
    (void)state;
    backend_quit();
    return 0;
}

static void test_text_visible_after_set(void **state)
{
    Layer layer;
    TextComponent *comp;
    const char *sample =
        "这是第一行测试文本。\n这是第二行测试文本。\nThird line of text.\nThis is fourth line.\n这是第五行，用来测试多行文本渲染是否正常工作。";
    Font font;
    DFont *dfont;

    (void)state;
    memset(&layer, 0, sizeof(layer));
    strcpy(layer.id, "testText");
    layer.rect = (Rect){50, 50, 400, 300};
    layer.type = TEXT;
    layer.visible = VISIBLE;
    layer.bg_color = (Color){255, 255, 255, 255};
    layer.color = (Color){0, 0, 0, 255};
    layer_set_text(&layer, sample);

    comp = text_component_create(&layer);
    assert_non_null(comp);
    text_component_set_multiline(comp, 1);
    assert_string_equal(layer_get_text(&layer), sample);

    memset(&font, 0, sizeof(font));
    dfont = backend_load_font("Roboto-Regular.ttf", 14);
    if (dfont) {
        font.default_font = dfont;
        layer.font = &font;
        backend_render_clear_color(255, 255, 255, 255);
        text_component_render(&layer);
        backend_render_present();
    }

    assert_int_equal(layer.visible, VISIBLE);
    text_component_destroy(comp);
    layer_set_text(&layer, NULL);
}

int main(int argc, char **argv)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_text_visible_after_set, setup_backend, teardown_backend),
    };
    (void)argc;
    (void)argv;
    return cmocka_run_group_tests(tests, NULL, NULL);
}
