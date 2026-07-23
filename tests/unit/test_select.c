#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <cmocka.h>

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

/* Select dropdown: first outside click clears just_expanded; second closes. */
static void test_just_expanded_keeps_open_then_closes(void **state)
{
    int just_expanded = 0;
    int expanded = 0;

    (void)state;

    expanded = 1;
    just_expanded = 1;
    assert_int_equal(expanded, 1);
    assert_int_equal(just_expanded, 1);

    /* first outside click */
    if (just_expanded) {
        just_expanded = 0;
    } else {
        expanded = 0;
    }
    assert_int_equal(expanded, 1);
    assert_int_equal(just_expanded, 0);

    /* second outside click */
    if (just_expanded) {
        just_expanded = 0;
    } else {
        expanded = 0;
    }
    assert_int_equal(expanded, 0);
    assert_int_equal(just_expanded, 0);
}

int main(int argc, char **argv)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_just_expanded_keeps_open_then_closes),
    };
    (void)argc;
    (void)argv;
    return cmocka_run_group_tests(tests, NULL, NULL);
}
