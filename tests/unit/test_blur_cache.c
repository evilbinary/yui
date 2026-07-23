#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <cmocka.h>

#include "ytype.h"
#include "backend.h"

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
        return -1; /* skip group if SDL/backend unavailable */
    }
    return 0;
}

static int teardown_backend(void **state)
{
    (void)state;
    backend_quit();
    return 0;
}

static void test_backdrop_filter_smoke(void **state)
{
    Rect rect1 = {100, 100, 200, 150};
    Rect rect2 = {150, 120, 200, 150};
    Rect rect3 = {100, 100, 200, 150};
    Rect bg1 = {50, 50, 300, 200};
    Rect bg2 = {200, 150, 300, 200};

    (void)state;
    backend_render_clear_color(50, 50, 80, 255);
    backend_render_fill_rect_color(&bg1, 200, 100, 100, 255);
    backend_render_fill_rect_color(&bg2, 100, 200, 100, 255);

    backend_render_backdrop_filter(&rect1, 10, 1.0f, 1.0f);
    backend_render_backdrop_filter(&rect3, 10, 1.0f, 1.0f); /* cache hit path */
    backend_render_backdrop_filter(&rect2, 15, 0.8f, 1.2f);
    backend_render_present();
}

int main(int argc, char **argv)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_backdrop_filter_smoke, setup_backend, teardown_backend),
    };
    (void)argc;
    (void)argv;
    return cmocka_run_group_tests(tests, NULL, NULL);
}
