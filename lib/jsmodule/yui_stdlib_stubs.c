// yui_stdlib_stubs.c
// 为 yui-stdlib-host 工具提供的存根函数
// 这个工具只是用来生成头文件，不需要实际的 YUI 运行时功能

#include "../../src/layer.h"
#include "../../src/theme_manager.h"

// 全局变量存根
Layer* g_layer_root = NULL;

// 图层操作函数存根
Layer* find_layer_by_id(Layer* root, const char* id) {
    return NULL;
}

Layer* parse_layer_from_string(const char* json_str, Layer* parent) {
    return NULL;
}

void destroy_layer(Layer* layer) {
    // 空函数
}

void layout_layer(Layer* layer) {
    // 空函数
}

void load_all_fonts(Layer* layer) {
    // 空函数
}

void layer_set_text(Layer* layer, const char* text) {
    // 空函数
}

const char* layer_get_text(const Layer* layer) {
    return "";
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
    // 空函数
}

Theme* theme_manager_get_current(void) {
    return NULL;
}

void theme_manager_apply_to_tree(Layer* root) {
    // 空函数
}
