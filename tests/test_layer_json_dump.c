#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/ytype.h"
#include "../src/layout.h"
#include "../src/component_registry.h"
#include "../lib/cjson/cJSON.h"

#if defined(_WIN32)
#include <windows.h>
int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    int argc = __argc;
    char** argv = __argv;
    return main(argc, argv);
}
#endif

static int g_failed = 0;

static void expect_true(int cond, const char* msg)
{
    if (cond) {
        printf("  PASS: %s\n", msg);
    } else {
        printf("  FAIL: %s\n", msg);
        g_failed++;
    }
}

static void expect_int(int got, int want, const char* msg)
{
    if (got == want) {
        printf("  PASS: %s (%d)\n", msg, got);
    } else {
        printf("  FAIL: %s (got %d, want %d)\n", msg, got, want);
        g_failed++;
    }
}

static void expect_str(const char* got, const char* want, const char* msg)
{
    if (got && want && strcmp(got, want) == 0) {
        printf("  PASS: %s (\"%s\")\n", msg, got);
    } else {
        printf("  FAIL: %s (got \"%s\", want \"%s\")\n",
               msg, got ? got : "(null)", want ? want : "(null)");
        g_failed++;
    }
}

static void test_null_layer(void)
{
    cJSON* json;
    char* printed;

    printf("\n=== null layer ===\n");
    json = layer_to_json(NULL, 0);
    expect_true(json != NULL && cJSON_IsNull(json), "layer_to_json(NULL) -> null");
    printed = cJSON_PrintUnformatted(json);
    expect_str(printed, "null", "printed null");
    free(printed);
    cJSON_Delete(json);
}

static void test_basic_tree(void)
{
    Layer root, child, sub;
    Event ev;
    cJSON* json;
    cJSON* rect;
    cJSON* fixed;
    cJSON* children;
    cJSON* child0;
    cJSON* style;
    cJSON* events;
    cJSON* item;
    char* printed;

    printf("\n=== basic tree + flags ===\n");

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
    root.children = malloc(sizeof(Layer*));
    root.children[0] = &child;

    /* default flags: no style / events */
    json = layer_to_json(&root, 0);
    expect_true(json != NULL && cJSON_IsObject(json), "root is object");
    expect_str(cJSON_GetObjectItem(json, "id")->valuestring, "root", "id");
    expect_str(cJSON_GetObjectItem(json, "type")->valuestring, "View", "type name");
    expect_int(cJSON_GetObjectItem(json, "visible")->valueint, VISIBLE, "visible");
    expect_int(cJSON_GetObjectItem(json, "scroll")->valueint, 12, "scroll");
    expect_true(cJSON_GetObjectItem(json, "style") == NULL, "no style without flag");
    expect_true(cJSON_GetObjectItem(json, "events") == NULL, "no events without flag");

    rect = cJSON_GetObjectItem(json, "rect");
    expect_int(cJSON_GetObjectItem(rect, "x")->valueint, 10, "rect.x");
    expect_int(cJSON_GetObjectItem(rect, "y")->valueint, 20, "rect.y");
    expect_int(cJSON_GetObjectItem(rect, "w")->valueint, 400, "rect.w");
    expect_int(cJSON_GetObjectItem(rect, "h")->valueint, 300, "rect.h");

    fixed = cJSON_GetObjectItem(json, "fixed");
    expect_int(cJSON_GetObjectItem(fixed, "w")->valueint, 400, "fixed.w");
    expect_int(cJSON_GetObjectItem(fixed, "h")->valueint, 300, "fixed.h");

    children = cJSON_GetObjectItem(json, "children");
    expect_true(cJSON_IsArray(children), "children is array");
    expect_int(cJSON_GetArraySize(children), 1, "children length");
    child0 = cJSON_GetArrayItem(children, 0);
    expect_str(cJSON_GetObjectItem(child0, "id")->valuestring, "child_btn", "child id");
    expect_str(cJSON_GetObjectItem(child0, "type")->valuestring, "Button", "child type");
    expect_true(cJSON_IsArray(cJSON_GetObjectItem(child0, "children")) &&
                cJSON_GetArraySize(cJSON_GetObjectItem(child0, "children")) == 0,
                "leaf children is []");

    item = cJSON_GetObjectItem(json, "sub");
    expect_true(cJSON_IsObject(item), "sub is object");
    expect_str(cJSON_GetObjectItem(item, "id")->valuestring, "sub_view", "sub id");

    printed = cJSON_PrintUnformatted(json);
    expect_true(printed != NULL && printed[0] == '{', "printable JSON");
    printf("  JSON sample: %.120s...\n", printed);
    free(printed);
    cJSON_Delete(json);

    /* with style + events */
    json = layer_to_json(&root, LAYER_JSON_STYLE | LAYER_JSON_EVENTS);
    style = cJSON_GetObjectItem(json, "style");
    events = cJSON_GetObjectItem(json, "events");
    expect_true(cJSON_IsObject(style), "style object with flag");
    expect_true(cJSON_IsObject(events), "events object with flag");
    expect_int(cJSON_GetObjectItem(style, "borderRadius")->valueint, 8, "style.borderRadius");
    expect_str(cJSON_GetObjectItem(style, "color")->valuestring, "#010203ff", "style.color");
    expect_str(cJSON_GetObjectItem(style, "bgColor")->valuestring, "#0a141ec8", "style.bgColor");
    expect_str(cJSON_GetObjectItem(events, "onClick")->valuestring, "@onRootClick", "events.onClick");
    expect_str(cJSON_GetObjectItem(events, "onLoad")->valuestring, "@onRootLoad", "events.onLoad");
    cJSON_Delete(json);

    /* no sub -> null */
    root.sub = NULL;
    json = layer_to_json(&root, 0);
    expect_true(cJSON_IsNull(cJSON_GetObjectItem(json, "sub")), "sub is null when absent");
    cJSON_Delete(json);

    free(root.children);
}

static void test_dump_json_smoke(void)
{
    Layer leaf;

    printf("\n=== layer_dump_json smoke ===\n");
    memset(&leaf, 0, sizeof(leaf));
    strcpy(leaf.id, "leaf");
    leaf.type = VIEW;
    leaf.rect = (Rect){1, 2, 3, 4};
    leaf.visible = VISIBLE;

    printf("  (printed below)\n");
    layer_dump_json(&leaf, stdout, 0);
    layer_dump_json(NULL, stdout, 0);
    expect_true(1, "layer_dump_json did not crash");
}

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    printf("=== layer JSON dump tests ===\n");
    yui_component_registry_init();
    yui_components_register_builtin();

    test_null_layer();
    test_basic_tree();
    test_dump_json_smoke();

    printf("\n=== summary: %s (%d failed) ===\n",
           g_failed == 0 ? "ALL PASS" : "FAILED", g_failed);
    return g_failed == 0 ? 0 : 1;
}
