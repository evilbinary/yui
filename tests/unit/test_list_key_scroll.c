/*
 * List 键盘导航与滚轮滚动测试：
 *   - 方向键上下移动键盘焦点项，并在超出可视区时滚动列表
 *   - 鼠标滚轮滚动列表（List 不是 View/Grid，需自行处理 POINTER_WHEEL）
 */
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <cmocka.h>

#include "ytype.h"
#include "layer.h"
#include "render.h"
#include "layout.h"
#include "focus.h"
#include "components/list_component.h"
#include "cJSON.h"

int main(int argc, char **argv);

#if defined(_WIN32)
#include <windows.h>
int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    (void)hInstance; (void)hPrevInstance; (void)lpCmdLine; (void)nCmdShow;
    return main(__argc, __argv);
}
#endif

static Layer* make_list_root(Layer** list_out)
{
    Layer* root = layer_create(NULL, 0, 0, 400, 300);
    Layer* list;
    cJSON* data;
    int i;

    root->type = VIEW;
    root->visible = VISIBLE;

    list = layer_create(root, 0, 0, 400, 200);
    list->type = LAYER_LIST;
    list->scrollable = 1;
    list->visible = VISIBLE;
    list_component_create_from_json(list, NULL);
    ((ListComponent*)list->component)->item_height = 34;

    data = cJSON_CreateArray();
    for (i = 0; i < 20; i++) {
        cJSON* item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "line", "item");
        cJSON_AddItemToArray(data, item);
    }
    list->on_data_update(list, data);

    root->children = (Layer**)malloc(sizeof(Layer*));
    root->children[0] = list;
    root->child_count = 1;

    *list_out = list;
    return root;
}

static KeyEvent key_down(int code)
{
    KeyEvent ke;
    memset(&ke, 0, sizeof(ke));
    ke.type = KEY_EVENT_DOWN;
    ke.data.key.key_code = code;
    return ke;
}

static void test_arrow_moves_and_scrolls(void **state)
{
    Layer* root;
    Layer* list;
    ListComponent* comp;
    KeyEvent ke;
    int i;

    (void)state;
    focus_init();
    root = make_list_root(&list);
    comp = (ListComponent*)list->component;

    assert_int_equal(focus_set(list), 1);

    ke = key_down(SDLK_DOWN);
    for (i = 0; i < 3; i++) {
        assert_int_equal(list_component_handle_key_event(list, &ke), 1);
    }
    assert_int_equal(comp->focused_index, 2);
    assert_int_equal(list->scroll_offset, 0);

    /* 继续下移到超出可视区，应触发滚动 */
    for (i = 0; i < 8; i++) {
        list_component_handle_key_event(list, &ke);
    }
    assert_true(comp->focused_index > 2);
    assert_true(list->scroll_offset > 0);

    /* 到末项后继续按下：保持焦点、不越界 */
    ke = key_down(SDLK_DOWN);
    for (i = 0; i < 40; i++) {
        list_component_handle_key_event(list, &ke);
    }
    assert_int_equal(comp->focused_index, 19);

    /* 上移回到首项并滚回顶部 */
    ke = key_down(SDLK_UP);
    for (i = 0; i < 40; i++) {
        list_component_handle_key_event(list, &ke);
    }
    assert_int_equal(comp->focused_index, 0);
    assert_int_equal(list->scroll_offset, 0);

    cJSON_Delete(list->data->json);
    free(list->data);
    list->data = NULL;
    focus_clear();
    destroy_layer(root);
    render_ctx_free(root);
}

static void test_wheel_scrolls(void **state)
{
    Layer* root;
    Layer* list;
    PointerEvent pe;
    int before;

    (void)state;
    focus_init();
    root = make_list_root(&list);

    before = list->scroll_offset;

    memset(&pe, 0, sizeof(pe));
    pe.device = POINTER_DEVICE_MOUSE;
    pe.phase = POINTER_WHEEL;
    pe.button = SDL_BUTTON_LEFT;
    pe.x = 200;
    pe.y = 100;
    pe.delta_y = 1; /* 向下滚 */

    assert_int_equal(list_component_handle_pointer_event(list, &pe), 1);
    assert_true(list->scroll_offset > before);

    /* 向上滚回到顶 */
    pe.delta_y = -1;
    assert_int_equal(list_component_handle_pointer_event(list, &pe), 1);
    assert_int_equal(list->scroll_offset, before);

    cJSON_Delete(list->data->json);
    free(list->data);
    list->data = NULL;
    focus_clear();
    destroy_layer(root);
    render_ctx_free(root);
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_arrow_moves_and_scrolls),
        cmocka_unit_test(test_wheel_scrolls),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
