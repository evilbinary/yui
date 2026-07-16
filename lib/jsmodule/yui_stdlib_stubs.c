// yui_stdlib_stubs.c
// 为 yui-stdlib-host 工具提供的存根函数
// 这个工具只是用来生成头文件，不需要实际的 YUI 运行时功能

#include "../../src/layer.h"
#include "../../src/layer_lifecycle.h"
#include "../../src/theme_manager.h"
#include "../../lib/cjson/cJSON.h"
#include "mquickjs.h"
#include <stdlib.h>

// 全局变量存根
Layer* g_layer_root = NULL;

int yui_inspect_mode_enabled = 0;
int yui_inspect_show_bounds = 0;
int yui_inspect_show_info = 0;

// 图层操作函数存根
Layer* find_layer_by_id(Layer* root, const char* id) {
    return NULL;
}

Layer* parse_layer_from_string(const char* json_str, Layer* parent) {
    return NULL;
}

void destroy_layer(Layer* layer) {
}

void layout_layer(Layer* layer) {
}

void load_all_fonts(Layer* layer) {
}

void layer_set_text(Layer* layer, const char* text) {
}

const char* layer_get_text(const Layer* layer) {
    return "";
}

int layer_show(Layer* layer) {
    return 0;
}

int layer_hide(Layer* layer) {
    return 0;
}

void layer_lifecycle_before_destroy(Layer* layer) {
}

// JSON 更新存根
int yui_update(Layer* root, const char* update_json) {
    return -1;
}

// 主题管理器存根函数
ThemeManager* theme_manager_get_instance(void) {
    return NULL;
}

Theme* theme_manager_load_theme(const char* theme_path) {
    return NULL;
}

Theme* theme_manager_load_theme_from_json(const char* json_str) {
    return NULL;
}

int theme_manager_set_current(const char* theme_name) {
    return 0;
}

void theme_manager_unload_theme(const char* theme_name) {
}

Theme* theme_manager_get_current(void) {
    return NULL;
}

void theme_manager_apply_to_tree(Layer* root) {
}

// js_module 桥接存根
const char* js_module_get_property_value(const char* layer_id, const char* property_name) {
    return NULL;
}

int js_module_load_from_json(cJSON* root_json, const char* json_file_path, int append) {
    return -1;
}

char* js_module_read_file(const char* file_path) {
    return NULL;
}

int js_module_resize_root(int width, int height) {
    return -1;
}

int js_module_set_event(const char* layer_id, const char* event_name, const char* event_func_name) {
    return -1;
}

// mquickjs 无 JS_ToBool，yui_stdlib.c 中 renderFromJson 需要
int JS_ToBool(JSContext* ctx, JSValue val) {
    if (JS_IsBool(val)) {
        return JS_VALUE_GET_SPECIAL_VALUE(val) != 0;
    }
    if (JS_IsInt(val)) {
        return JS_VALUE_GET_INT(val) != 0;
    }
    return !JS_IsNull(val) && !JS_IsUndefined(val);
}
