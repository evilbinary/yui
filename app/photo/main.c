#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "cJSON.h"
#include "event.h"
#include "layer.h"
#include "render.h"
#include "layout.h"
#include "backend.h"
#include "util.h"
#include "popup_manager.h"
#include "../lib/jsmodule/js_module.h"
#include "yaml_cjson.h"
#include "log.h"

#if defined(_WIN32)
#include <windows.h>
int main(int argc, char* argv[]);
int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    int argc = __argc;
    char** argv = __argv;
    return main(argc, argv);
}
#endif

static bool is_yaml_file(const char* filename) {
    if (!filename) return false;
    const char* ext = strrchr(filename, '.');
    if (!ext) return false;
    return (strcmp(ext, ".yaml") == 0 || strcmp(ext, ".yml") == 0);
}

static cJSON* parse_yaml_json_file(const char* file_path) {
    if (!file_path) return NULL;

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

int main(int argc, char* argv[]) {
    backend_init();
    popup_manager_init();

    if (js_module_init() != 0) {
        fprintf(stderr, "Failed to initialize JavaScript engine\n");
        return -1;
    }

    char* json_path = "app/photo/app.json";
    if (argc > 1) {
        json_path = argv[1];
    }

    printf("Photo app loading: %s\n", json_path);

    cJSON* root_json = parse_yaml_json_file(json_path);
    if (!root_json) {
        fprintf(stderr, "Failed to parse file: %s\n", json_path);
        return -1;
    }

    Layer* ui_root = layer_create_from_json(root_json, NULL);
    if (ui_root == NULL) {
        fprintf(stderr, "Failed to create UI from JSON: %s\n", json_path);
        return -1;
    }

    js_module_init_layer(ui_root);
    load_all_fonts(ui_root);

    int count = js_module_load_from_json(root_json, json_path, 0);
    if (count <= 0) {
        cJSON* source = cJSON_GetObjectItem(root_json, "source");
        if (source && cJSON_IsString(source)) {
            const char* source_path = source->valuestring;
            cJSON* source_json = parse_yaml_json_file(source_path);
            if (source_json) {
                js_module_load_from_json(source_json, source_path, 0);
                cJSON_Delete(source_json);
            }
        }
    }
    print_registered_events();

    if (ui_root->rect.w <= 0 || ui_root->rect.h <= 0) {
        int window_width, window_height;
        backend_get_windowsize(&window_width, &window_height);
        if (ui_root->rect.w <= 0) ui_root->rect.w = window_width;
        if (ui_root->rect.h <= 0) ui_root->rect.h = window_height;
    }
    if (ui_root->text != NULL) {
        backend_set_window_size(ui_root->text);
    }

    cJSON* root_style = cJSON_GetObjectItem(root_json, "style");
    if (root_style) {
        cJSON* title_bg = cJSON_GetObjectItem(root_style, "titleBarBgColor");
        cJSON* title_text = cJSON_GetObjectItem(root_style, "titleBarTextColor");
        if (title_bg && cJSON_IsString(title_bg)) {
            Color bg, text;
            parse_color(title_bg->valuestring, &bg);
            if (title_text && cJSON_IsString(title_text)) {
                parse_color(title_text->valuestring, &text);
            } else {
                text = (Color){255, 255, 255, 255};
            }
            backend_set_titlebar_color(bg, text);
        }
    }

    cJSON_Delete(root_json);

    if (ui_root->font == NULL) {
        ui_root->font = malloc(sizeof(Font));
        sprintf(ui_root->font->path, "%s", "simhei.ttf");
    }
    if (ui_root->assets == NULL) {
        ui_root->assets = malloc(sizeof(Assets));
        sprintf(ui_root->assets->path, "%s", "assets");
    }

    load_textures(ui_root);
    layout_layer(ui_root);
    layout_capture_base(ui_root);

    /* 最小尺寸不能大于 app.json 根 size，否则会把窗口强行拉宽 */
    backend_set_resizable(1);
    backend_set_minimum_windowsize(ui_root->rect.w, ui_root->rect.h);
    backend_set_windowsize(ui_root->rect.w, ui_root->rect.h);
    backend_set_resize_callback(layout_resize);

    backend_run(ui_root);

    js_module_cleanup();
    popup_manager_cleanup();
    backend_quit();
    return 0;
}
