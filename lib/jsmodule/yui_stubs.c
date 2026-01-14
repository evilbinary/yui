// YUI stub functions for yui-stdlib-host build
// This file provides empty implementations of YUI functions
// so that yui-stdlib-host can compile without linking the full YUI library

#include "../yui/layer.h"

// Global variables
Layer* g_layer_root = NULL;

// Stub implementations
Layer* find_layer_by_id(Layer* root, const char* id) {
    return NULL;
}

Layer* parse_layer_from_string(const char* json_str, Layer* parent) {
    return NULL;
}

void destroy_layer(Layer* layer) {
    // Empty stub
}

void layout_layer(Layer* layer) {
    // Empty stub
}

void load_all_fonts(Layer* layer) {
    // Empty stub
}

void layer_set_text(Layer* layer, const char* text) {
    // Empty stub
}

const char* layer_get_text(Layer* layer) {
    return "";
}
