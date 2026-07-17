#include "yui_boot.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "cJSON.h"
#include "backend.h"
#include "layer.h"
#include "layout.h"
#include "popup_manager.h"
#include "render.h"
#include "yaml_cjson.h"

#ifdef HAS_JS_MODULE
#include "js_module.h"
#endif

static Layer* g_root = NULL;
static char g_json_path[MAX_PATH];
static char g_assets_override[MAX_PATH];

static const char* yui_default_assets_dir(const char* json_path, const char* assets_override) {
    if (assets_override && assets_override[0]) {
        return assets_override;
    }
    if (json_path && (strncmp(json_path, "app/", 4) == 0 || strncmp(json_path, "app\\", 4) == 0)) {
        return "app/assets";
    }
    return "assets";
}

static void yui_ensure_root_assets(Layer* root, const char* json_path, const char* assets_override) {
    const char* assets_dir;

    if (!root) {
        return;
    }
    if (assets_override && assets_override[0]) {
        assets_dir = assets_override;
    } else if (root->assets != NULL && root->assets->path[0] != '\0') {
        return;
    } else {
        assets_dir = yui_default_assets_dir(json_path, NULL);
    }
    if (root->assets == NULL) {
        root->assets = malloc(sizeof(Assets));
    }
    if (root->assets) {
        snprintf(root->assets->path, sizeof(root->assets->path), "%s", assets_dir);
    }
}

static void yui_propagate_assets(Layer* layer) {
    int i;

    if (!layer || !layer->assets) {
        return;
    }
    if (layer->children) {
        for (i = 0; i < layer->child_count; i++) {
            Layer* child = layer->children[i];
            if (child && child->assets == NULL) {
                child->assets = layer->assets;
            }
            yui_propagate_assets(child);
        }
    }
    if (layer->sub) {
        if (layer->sub->assets == NULL) {
            layer->sub->assets = layer->assets;
        }
        yui_propagate_assets(layer->sub);
    }
    if (layer->item_template) {
        if (layer->item_template->assets == NULL) {
            layer->item_template->assets = layer->assets;
        }
        yui_propagate_assets(layer->item_template);
    }
}

static bool yui_is_yaml_file(const char* filename) {
    const char* ext;
    if (!filename) {
        return false;
    }
    ext = strrchr(filename, '.');
    if (!ext) {
        return false;
    }
    return strcmp(ext, ".yaml") == 0 || strcmp(ext, ".yml") == 0;
}

static cJSON* yui_parse_ui_file(const char* file_path) {
    if (!file_path) {
        return NULL;
    }
    if (yui_is_yaml_file(file_path)) {
        char* error = NULL;
        cJSON* json = yaml_file2cjson(file_path, &error);
        if (!json) {
            fprintf(stderr, "yui_boot: failed to parse YAML: %s\n", file_path);
            if (error) {
                fprintf(stderr, "  %s\n", error);
                free(error);
            }
            return NULL;
        }
        return json;
    }
    return parse_json((char*)file_path);
}

#ifdef HAS_JS_MODULE
static void yui_load_js_scripts(cJSON* root_json, const char* json_path) {
    int count = 0;
    cJSON* source;

    if (!root_json || !json_path) {
        return;
    }

    source = cJSON_GetObjectItem(root_json, "source");
    if (source && cJSON_IsString(source)) {
        const char* source_path = source->valuestring;
        cJSON* source_json = yui_parse_ui_file(source_path);
        if (source_json) {
            count = js_module_load_from_json(source_json, source_path, 0);
            cJSON_Delete(source_json);
        } else {
            fprintf(stderr, "yui_boot: failed to load source JSON: %s\n", source_path);
        }
    }
    if (count <= 0) {
        js_module_load_from_json(root_json, json_path, 0);
    } else {
        js_module_load_from_json(root_json, json_path, 1);
    }
}
#endif

static void yui_prepare_root_resources(Layer* root, const char* assets_override) {
    if (!root) {
        return;
    }
    if (root->font == NULL) {
        root->font = malloc(sizeof(Font));
        if (root->font) {
            snprintf(root->font->path, sizeof(root->font->path), "%s", "Roboto-Regular.ttf");
            root->font->size = 16;
            snprintf(root->font->weight, sizeof(root->font->weight), "%s", "normal");
        }
    }
    yui_ensure_root_assets(root, g_json_path, assets_override);
    yui_propagate_assets(root);
    load_all_fonts(root);
}

static void yui_apply_root_layout(Layer* root) {
    int window_width = 0;
    int window_height = 0;

    if (!root) {
        return;
    }

    backend_get_windowsize(&window_width, &window_height);
    if (root->rect.w <= 0 || root->rect.h <= 0) {
        if (root->rect.w <= 0) {
            root->rect.w = window_width > 0 ? window_width : 360;
        }
        if (root->rect.h <= 0) {
            root->rect.h = window_height > 0 ? window_height : 640;
        }
    } else {
#ifndef YUI_BACKEND_MOBILE
        backend_set_windowsize(root->rect.w, root->rect.h);
#endif
    }

    if (root->text != NULL) {
        backend_set_window_size(root->text);
    }

    load_textures(root);
    layout_layer(root);
    backend_set_ui_root(root);
}

void yui_set_native_surface(void* surface) {
#ifdef YUI_BACKEND_MOBILE
    extern void backend_mobile_set_native_surface(void* native_surface);
    backend_mobile_set_native_surface(surface);
#else
    (void)surface;
#endif
}

void yui_set_density(float density) {
    backend_set_density(density);
}

int yui_init(const char* json_path, const char* assets_dir) {
    cJSON* root_json = NULL;

    if (!json_path) {
        return -1;
    }

    snprintf(g_json_path, sizeof(g_json_path), "%s", json_path);
    if (assets_dir && assets_dir[0]) {
        snprintf(g_assets_override, sizeof(g_assets_override), "%s", assets_dir);
    } else {
        g_assets_override[0] = '\0';
    }

    if (backend_init() != 0) {
        return -1;
    }
    popup_manager_init();

#ifdef HAS_JS_MODULE
    if (js_module_init() != 0) {
        fprintf(stderr, "yui_boot: failed to initialize JavaScript engine\n");
        return -1;
    }
#endif

    root_json = yui_parse_ui_file(json_path);
    if (!root_json) {
        return -1;
    }

    g_root = layer_create_from_json(root_json, NULL);
    if (!g_root) {
        cJSON_Delete(root_json);
        return -1;
    }

#ifdef HAS_JS_MODULE
    js_module_init_layer(g_root);
#endif

    yui_prepare_root_resources(g_root, g_assets_override[0] ? g_assets_override : NULL);

#ifdef HAS_JS_MODULE
    yui_load_js_scripts(root_json, json_path);
#endif

    cJSON_Delete(root_json);

    yui_apply_root_layout(g_root);
    return 0;
}

void yui_resize(int width, int height) {
    float d = yui_density > 0.0f ? yui_density : 1.0f;

    backend_set_windowsize(width, height);
    // if (g_root) {
    //     g_root->rect.x = 0;
    //     g_root->rect.y = 0;
    //     g_root->rect.w = (int)(width / d);
    //     g_root->rect.h = (int)(height / d);
    //     layout_layer(g_root);
    // }
}

void yui_tick(void) {
    if (g_root) {
        backend_tick(g_root);
    }
}

void yui_on_touch(int pointer_id, int x, int y, int phase) {
#ifdef YUI_BACKEND_MOBILE
    extern void backend_mobile_on_touch(int pointer_id, int x, int y, int phase);
    backend_mobile_on_touch(pointer_id, x, y, phase);
#else
    (void)pointer_id;
    (void)x;
    (void)y;
    (void)phase;
#endif
}

void yui_shutdown(void) {
#ifdef HAS_JS_MODULE
    js_module_cleanup();
#endif
    popup_manager_cleanup();
    backend_quit();
    g_root = NULL;
    g_json_path[0] = '\0';
    g_assets_override[0] = '\0';
}

Layer* yui_get_root(void) {
    return g_root;
}
