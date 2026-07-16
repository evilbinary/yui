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

static Layer* g_root = NULL;
static char assets_dir_from_init[MAX_PATH];

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

static void yui_apply_root_layout(Layer* root, const char* assets_dir) {
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
        backend_set_windowsize(root->rect.w, root->rect.h);
    }

    if (root->text != NULL) {
        backend_set_window_size(root->text);
    }

    if (root->font == NULL) {
        root->font = malloc(sizeof(Font));
        if (root->font) {
            snprintf(root->font->path, sizeof(root->font->path), "%s", "Roboto-Regular.ttf");
        }
    }
    if (root->assets == NULL) {
        root->assets = malloc(sizeof(Assets));
        if (root->assets) {
            snprintf(root->assets->path, sizeof(root->assets->path), "%s", "assets");
        }
    }

    if (root->assets && assets_dir && assets_dir[0]) {
        snprintf(root->assets->path, sizeof(root->assets->path), "%s", assets_dir);
    }

    load_all_fonts(root);
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

int yui_init(const char* json_path, const char* assets_dir) {
    cJSON* root_json = NULL;

    if (!json_path) {
        return -1;
    }

    if (assets_dir && assets_dir[0]) {
        snprintf(assets_dir_from_init, sizeof(assets_dir_from_init), "%s", assets_dir);
    } else {
        assets_dir_from_init[0] = '\0';
        assets_dir = "assets";
    }

    if (backend_init() != 0) {
        return -1;
    }
    popup_manager_init();

    root_json = yui_parse_ui_file(json_path);
    if (!root_json) {
        return -1;
    }

    g_root = layer_create_from_json(root_json, NULL);
    cJSON_Delete(root_json);
    if (!g_root) {
        return -1;
    }

    yui_apply_root_layout(g_root, assets_dir);
    return 0;
}

void yui_resize(int width, int height) {
    backend_set_windowsize(width, height);
    if (g_root) {
        layout_layer(g_root);
    }
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
    popup_manager_cleanup();
    backend_quit();
    g_root = NULL;
    assets_dir_from_init[0] = '\0';
}

Layer* yui_get_root(void) {
    return g_root;
}
