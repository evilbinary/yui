// yui_stdlib_stubs.c
// 为 yui-stdlib-host 工具提供的存根函数
// 这个工具只是用来生成头文件，不需要实际的 YUI 运行时功能

#include "../../src/layer.h"


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
