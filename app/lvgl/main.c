/**
 * YUI LVGL backend demo
 *
 * Build (MSYS2 / ya):
 *   ya -p lvgl -b lvgl-sdl
 *   ya -p lvgl -r lvgl-sdl
 */

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "event.h"
#include "layer.h"
#include "layout.h"
#include "render.h"
#include "backend.h"
#include "popup_manager.h"
#include "util.h"
#include "yaml_cjson.h"
#include "../lib/jsmodule/js_module.h"
#include "../lib/lvglmodule/lvgl_component.h"
#include "log.h"

#if defined(_WIN32)
#include <windows.h>
int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    int argc = __argc;
    char** argv = __argv;
    return main(argc, argv);
}
#endif

static bool is_yaml_file(const char* filename)
{
    const char* ext;

    if (!filename) {
        return false;
    }

    ext = strrchr(filename, '.');
    if (!ext) {
        return false;
    }

    return (strcmp(ext, ".yaml") == 0 || strcmp(ext, ".yml") == 0);
}

static cJSON* parse_yaml_json_file(const char* file_path)
{
    if (!file_path) {
        return NULL;
    }

    if (is_yaml_file(file_path)) {
        char* error = NULL;
        cJSON* json = yaml_file2cjson(file_path, &error);

        if (!json) {
            fprintf(stderr, "Failed to parse YAML file: %s\n", file_path);
            if (error) {
                fprintf(stderr, "Error: %s\n", error);
                free(error);
            }
            return NULL;
        }
        return json;
    }

    return parse_json((char*)file_path);
}

int main(int argc, char* argv[])
{
    char* json_path = "app/lvgl/app.json";
    cJSON* root_json;
    Layer* ui_root;
    int js_count;

    if (argc > 1) {
        json_path = argv[1];
    }

    LOGI("lvgl", "YUI LVGL demo starting");
    LOGI("lvgl", "loading %s", json_path);

    if (backend_init() != 0) {
        fprintf(stderr, "backend_init failed\n");
        return 1;
    }

    popup_manager_init();

    if (js_module_init() != 0) {
        fprintf(stderr, "Failed to initialize JavaScript engine\n");
        backend_quit();
        return -1;
    }

    root_json = parse_yaml_json_file(json_path);
    if (!root_json) {
        fprintf(stderr, "Failed to parse file: %s\n", json_path);
        js_module_cleanup();
        backend_quit();
        return -1;
    }

    ui_root = layer_create_from_json(root_json, NULL);
    if (!ui_root) {
        fprintf(stderr, "Failed to create UI from JSON: %s\n", json_path);
        cJSON_Delete(root_json);
        js_module_cleanup();
        backend_quit();
        return -1;
    }

    js_module_init_layer(ui_root);
    load_all_fonts(ui_root);

    if (ui_root->rect.w <= 0 || ui_root->rect.h <= 0) {
        int window_width;
        int window_height;

        backend_get_windowsize(&window_width, &window_height);
        if (ui_root->rect.w <= 0) {
            ui_root->rect.w = window_width;
        }
        if (ui_root->rect.h <= 0) {
            ui_root->rect.h = window_height;
        }
    } else {
        backend_set_windowsize(ui_root->rect.w, ui_root->rect.h);
    }

    if (!ui_root->font) {
        ui_root->font = malloc(sizeof(Font));
        if (ui_root->font) {
            strcpy(ui_root->font->path, "Roboto-Regular.ttf");
            ui_root->font->size = 16;
            strcpy(ui_root->font->weight, "normal");
            ui_root->font->default_font = NULL;
        }
    }

    if (!ui_root->assets) {
        ui_root->assets = malloc(sizeof(Assets));
        if (ui_root->assets) {
            strcpy(ui_root->assets->path, "app/assets");
        }
    }

    load_textures(ui_root);
    layout_layer(ui_root);

    fprintf(stdout, "===== layer dump (after first layout) =====\n");
    layer_dump(ui_root, 0);
    fprintf(stdout, "===========================================\n");

    printf("加载并执行 JS 文件...\n");
    js_count = js_module_load_from_json(root_json, json_path, 0);
    if (js_count <= 0) {
        cJSON* source = cJSON_GetObjectItem(root_json, "source");
        if (source && cJSON_IsString(source)) {
            const char* source_path = source->valuestring;
            cJSON* source_json = parse_yaml_json_file(source_path);

            if (source_json) {
                int source_count = js_module_load_from_json(source_json, source_path, 0);
                cJSON_Delete(source_json);
                js_count += source_count;
            }
        }
    }
    print_registered_events();
    lvgl_module_init_layer(ui_root);

    if (ui_root->text) {
        backend_set_window_size(ui_root->text);
    }

    cJSON_Delete(root_json);

    layout_layer(ui_root);

    fprintf(stdout, "===== layer dump (after second layout) =====\n");
    layer_dump(ui_root, 0);
    fprintf(stdout, "============================================\n");

    lvgl_apply_layer_rects(ui_root);
    fprintf(stdout, "===== lvgl dump (after sync) =====\n");
    lvgl_dump_layer_rects(ui_root, 0);
    fprintf(stdout, "==================================\n");

    LOGI("lvgl", "entering main loop (%dx%d)", ui_root->rect.w, ui_root->rect.h);
    backend_run(ui_root);

    destroy_layer(ui_root);
    js_module_cleanup();
    popup_manager_cleanup();
    backend_quit();
    return 0;
}
