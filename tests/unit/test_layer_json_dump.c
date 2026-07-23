#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <cmocka.h>

#include "ytype.h"
#include "layout.h"
#include "component_registry.h"
#include "cJSON.h"

int main(int argc, char **argv);

#if defined(_WIN32)
#include <windows.h>
int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    (void)hInstance;
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;
    return main(__argc, __argv);
}
#endif

static int setup_registry(void **state)
{
    (void)state;
    yui_component_registry_init();
    yui_components_register_builtin();
    return 0;
}

static void test_layer_to_json_null(void **state)
{
    cJSON *json;
    char *printed;

    (void)state;
    json = layer_to_json(NULL, 0);
    assert_non_null(json);
    assert_true(cJSON_IsNull(json));
    printed = cJSON_PrintUnformatted(json);
    assert_non_null(printed);
    assert_string_equal(printed, "null");
    free(printed);
    cJSON_Delete(json);
}

static void test_layer_to_json_basic_tree(void **state)
{
    Layer root, child, sub;
    Event ev;
    cJSON *json;
    cJSON *rect;
    cJSON *fixed;
    cJSON *children;
    cJSON *child0;
    cJSON *item;

    (void)state;
    memset(&root, 0, sizeof(root));
    memset(&child, 0, sizeof(child));
    memset(&sub, 0, sizeof(sub));
    memset(&ev, 0, sizeof(ev));

    strcpy(root.id, "root");
    root.type = VIEW;
    root.rect = (Rect){10, 20, 400, 300};
    root.fixed_width = 400;
    root.fixed_height = 300;
    root.visible = VISIBLE;
    root.scroll_offset = 12;
    root.color = (Color){1, 2, 3, 255};
    root.bg_color = (Color){10, 20, 30, 200};
    root.radius = 8;
    root.padding[0] = 1;
    root.padding[1] = 2;
    root.padding[2] = 3;
    root.padding[3] = 4;
    strcpy(root.lifecycle_on_load, "onRootLoad");
    strcpy(ev.click_name, "onRootClick");
    root.event = &ev;

    strcpy(child.id, "child_btn");
    child.type = BUTTON;
    child.rect = (Rect){30, 40, 100, 40};
    child.fixed_width = 100;
    child.fixed_height = 40;
    child.visible = VISIBLE;
    child.parent = &root;

    strcpy(sub.id, "sub_view");
    sub.type = VIEW;
    sub.rect = (Rect){0, 0, 50, 50};
    sub.visible = VISIBLE;
    root.sub = &sub;

    root.child_count = 1;
    root.children = malloc(sizeof(Layer *));
    assert_non_null(root.children);
    root.children[0] = &child;

    json = layer_to_json(&root, 0);
    assert_non_null(json);
    assert_true(cJSON_IsObject(json));
    assert_string_equal(cJSON_GetObjectItem(json, "id")->valuestring, "root");
    assert_string_equal(cJSON_GetObjectItem(json, "type")->valuestring, "View");
    assert_int_equal(cJSON_GetObjectItem(json, "visible")->valueint, VISIBLE);
    assert_int_equal(cJSON_GetObjectItem(json, "scroll")->valueint, 12);
    assert_null(cJSON_GetObjectItem(json, "style"));
    assert_null(cJSON_GetObjectItem(json, "events"));

    rect = cJSON_GetObjectItem(json, "rect");
    assert_non_null(rect);
    assert_int_equal(cJSON_GetObjectItem(rect, "x")->valueint, 10);
    assert_int_equal(cJSON_GetObjectItem(rect, "y")->valueint, 20);
    assert_int_equal(cJSON_GetObjectItem(rect, "w")->valueint, 400);
    assert_int_equal(cJSON_GetObjectItem(rect, "h")->valueint, 300);

    fixed = cJSON_GetObjectItem(json, "fixed");
    assert_non_null(fixed);
    assert_int_equal(cJSON_GetObjectItem(fixed, "w")->valueint, 400);
    assert_int_equal(cJSON_GetObjectItem(fixed, "h")->valueint, 300);

    children = cJSON_GetObjectItem(json, "children");
    assert_true(cJSON_IsArray(children));
    assert_int_equal(cJSON_GetArraySize(children), 1);
    child0 = cJSON_GetArrayItem(children, 0);
    assert_string_equal(cJSON_GetObjectItem(child0, "id")->valuestring, "child_btn");
    assert_string_equal(cJSON_GetObjectItem(child0, "type")->valuestring, "Button");
    assert_int_equal(cJSON_GetArraySize(cJSON_GetObjectItem(child0, "children")), 0);

    item = cJSON_GetObjectItem(json, "sub");
    assert_true(cJSON_IsObject(item));
    assert_string_equal(cJSON_GetObjectItem(item, "id")->valuestring, "sub_view");
    cJSON_Delete(json);

    json = layer_to_json(&root, LAYER_JSON_STYLE | LAYER_JSON_EVENTS);
    assert_non_null(json);
    assert_true(cJSON_IsObject(cJSON_GetObjectItem(json, "style")));
    assert_true(cJSON_IsObject(cJSON_GetObjectItem(json, "events")));
    assert_int_equal(cJSON_GetObjectItem(cJSON_GetObjectItem(json, "style"), "borderRadius")->valueint, 8);
    assert_string_equal(cJSON_GetObjectItem(cJSON_GetObjectItem(json, "style"), "color")->valuestring,
                        "#010203ff");
    assert_string_equal(cJSON_GetObjectItem(cJSON_GetObjectItem(json, "events"), "onClick")->valuestring,
                        "@onRootClick");
    assert_string_equal(cJSON_GetObjectItem(cJSON_GetObjectItem(json, "events"), "onLoad")->valuestring,
                        "@onRootLoad");
    cJSON_Delete(json);

    root.sub = NULL;
    json = layer_to_json(&root, 0);
    assert_true(cJSON_IsNull(cJSON_GetObjectItem(json, "sub")));
    cJSON_Delete(json);

    free(root.children);
}

static void test_layer_dump_json_smoke(void **state)
{
    Layer leaf;

    (void)state;
    memset(&leaf, 0, sizeof(leaf));
    strcpy(leaf.id, "leaf");
    leaf.type = VIEW;
    leaf.rect = (Rect){1, 2, 3, 4};
    leaf.visible = VISIBLE;

    layer_dump_json(&leaf, stdout, 0);
    layer_dump_json(NULL, stdout, 0);
}

int main(int argc, char **argv)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_layer_to_json_null),
        cmocka_unit_test_setup(test_layer_to_json_basic_tree, setup_registry),
        cmocka_unit_test_setup(test_layer_dump_json_smoke, setup_registry),
    };

    (void)argc;
    (void)argv;
    return cmocka_run_group_tests(tests, NULL, NULL);
}
