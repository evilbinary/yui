/**
 * YUI LVGL backend demo
 *
 * Build (MSYS2 / ya):
 *   set YUI_PLAT=lvgl
 *   python ya.py -b lvgl
 *   python ya.py -r lvgl
 *
 * Or with platform flag if supported:
 *   python ya.py -p lvgl -b lvgl
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "layer.h"
#include "layout.h"
#include "render.h"
#include "backend.h"
#include "popup_manager.h"
#include "log.h"

#if defined(_WIN32)
#include <windows.h>
int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    int argc = __argc;
    char** argv = __argv;
    return main(argc, argv);
}
#endif

int main(int argc, char* argv[])
{
    const char* json_path = "app/lvgl/app.json";
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

    cJSON* root_json = parse_json((char*)json_path);
    if (!root_json) {
        fprintf(stderr, "Failed to parse: %s\n", json_path);
        backend_quit();
        return 1;
    }

    Layer* ui_root = layer_create_from_json(root_json, NULL);
    if (!ui_root) {
        fprintf(stderr, "Failed to create UI from: %s\n", json_path);
        cJSON_Delete(root_json);
        backend_quit();
        return 1;
    }

    if (ui_root->rect.w <= 0 || ui_root->rect.h <= 0) {
        int w = 0;
        int h = 0;
        backend_get_windowsize(&w, &h);
        if (ui_root->rect.w <= 0) ui_root->rect.w = w;
        if (ui_root->rect.h <= 0) ui_root->rect.h = h;
    } else {
        backend_set_windowsize(ui_root->rect.w, ui_root->rect.h);
    }

    if (ui_root->text) {
        backend_set_window_size(ui_root->text);
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

    load_all_fonts(ui_root);
    load_textures(ui_root);
    layout_layer(ui_root);

    cJSON_Delete(root_json);

    LOGI("lvgl", "entering main loop (%dx%d)", ui_root->rect.w, ui_root->rect.h);
    backend_run(ui_root);

    destroy_layer(ui_root);
    popup_manager_cleanup();
    backend_quit();
    return 0;
}
