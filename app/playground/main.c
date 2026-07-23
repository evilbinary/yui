#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <limits.h>
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
int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    int argc = __argc;
    char** argv = __argv;
    return main(argc, argv);
}
#endif

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif


// 检查文件扩展名是否为 YAML
static bool is_yaml_file(const char* filename) {
    if (!filename) return false;
    
    const char* ext = strrchr(filename, '.');
    if (!ext) return false;
    
    return (strcmp(ext, ".yaml") == 0 || strcmp(ext, ".yml") == 0);
}


// 解析文件（支持 JSON 和 YAML）
cJSON* parse_yaml_json_file(const char* file_path) {
    if (!file_path) return NULL;
    
    // 检查是否为 YAML 文件
    if (is_yaml_file(file_path)) {
        printf("DEBUG: Detected YAML file: %s\n", file_path);
        
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
        
        printf("DEBUG: Successfully converted YAML to JSON\n");
        return json;
    } else {
        // 默认按 JSON 解析
        printf("DEBUG: Parsing as JSON file: %s\n", file_path);
        return parse_json((char*)file_path);
    }
}

// ====================== 主入口 ======================
int main(int argc, char* argv[]) {
    int auto_test = 0;
    int auto_frames = 90;
    int headless = -1; /* -1 unset, 0 show, 1 hide */
    char* json_path = "app/playground/app.json";
    const char* auto_env;
    const char* frames_env;
    const char* headless_env;
    int i;

    auto_env = getenv("YUI_AUTO_TEST");
    if (auto_env && auto_env[0] && strcmp(auto_env, "0") != 0) {
        auto_test = 1;
    }
    frames_env = getenv("YUI_AUTO_FRAMES");
    if (frames_env && frames_env[0]) {
        int n = atoi(frames_env);
        if (n > 0) auto_frames = n;
    }
    headless_env = getenv("YUI_HEADLESS");
    if (headless_env && headless_env[0]) {
        headless = (strcmp(headless_env, "0") == 0) ? 0 : 1;
    }

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--auto") == 0 || strcmp(argv[i], "--auto-test") == 0) {
            auto_test = 1;
            continue;
        }
        if (strcmp(argv[i], "--headless") == 0) {
            headless = 1;
            continue;
        }
        if (strcmp(argv[i], "--show") == 0) {
            headless = 0;
            continue;
        }
        if (strncmp(argv[i], "--frames=", 9) == 0) {
            int n = atoi(argv[i] + 9);
            if (n > 0) auto_frames = n;
            continue;
        }
        if (argv[i][0] != '-') {
            json_path = argv[i];
        }
    }

    /* Auto-test defaults to hidden window; --show overrides. */
    if (auto_test && headless < 0) {
        headless = 1;
    }
    if (headless > 0) {
        backend_set_headless(1);
    } else if (headless == 0) {
        backend_set_headless(0);
    }

    backend_init();
    popup_manager_init();

    // 初始化 JS 引擎
    if (js_module_init() != 0) {
        fprintf(stderr, "Failed to initialize JavaScript engine\n");
        return -1;
    }

    printf("DEBUG: Loading file from path: %s\n", json_path);
    if (auto_test) {
        printf("AUTO_TEST: enabled, frames=%d, headless=%d\n",
               auto_frames, backend_is_headless());
        backend_set_auto_frames(auto_frames);
    } else if (backend_is_headless()) {
        printf("HEADLESS: window hidden\n");
    }

    cJSON* root_json = parse_yaml_json_file(json_path);
    if (!root_json) {
        fprintf(stderr, "Failed to parse file: %s\n", json_path);
        return -1;
    }    
    Layer* ui_root = layer_create_from_json(root_json,NULL);
    if(ui_root==NULL){
        fprintf(stderr, "Failed to create UI from JSON: %s\n", json_path);
        return -1;
    }

    // 设置 UI 根图层（须在加载 JS 之前，以便事件绑定能找到图层）
    js_module_init_layer(ui_root);

    load_all_fonts(ui_root);

    // 先加载 source 中的 JS，再加载主 JSON，确保 onLoad 触发时脚本已就绪
    printf("加载并执行 JS 文件...\n");
    int count = 0;
    cJSON* source = cJSON_GetObjectItem(root_json, "source");
    if (source && cJSON_IsString(source)) {
        const char* source_path = source->valuestring;
        printf("DEBUG: Loading JS from source file: %s\n", source_path);
        cJSON* source_json = parse_yaml_json_file((char*)source_path);
        if (source_json) {
            count = js_module_load_from_json(source_json, source_path, 0);
            printf("DEBUG: Loaded %d JS file(s) from source JSON\n", count);
            cJSON_Delete(source_json);
        } else {
            printf("ERROR: Failed to load source JSON file: %s\n", source_path);
        }
    }
    if (count <= 0) {
        count = js_module_load_from_json(root_json, json_path, 0);
    } else {
        js_module_load_from_json(root_json, json_path, 1);
    }
    print_registered_events();

    // 如果根图层没有设置宽度和高度，则根据窗口大小设置
    if (ui_root->rect.w <= 0 || ui_root->rect.h <= 0) {
        int window_width, window_height;
        backend_get_windowsize(&window_width, &window_height);
        if (ui_root->rect.w <= 0) {
            ui_root->rect.w = window_width;
        }
        if (ui_root->rect.h <= 0) {
            ui_root->rect.h = window_height;
        }
    }else{
        backend_set_windowsize(ui_root->rect.w,ui_root->rect.h);
    }
    if(ui_root->text!=NULL){
        backend_set_window_size(ui_root->text);
    }

    // 设置窗口标题栏颜色（从 style 中读取）
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

    // 设置窗口图标
    cJSON* icon_path = cJSON_GetObjectItem(root_json, "icon");
    if (icon_path && cJSON_IsString(icon_path)) {
        backend_set_window_icon(icon_path->valuestring);
    }

    // 如果根图层没有设置宽度和高度，则根据窗口大小设置
    LOGD("playground", "ui_root %d,%d", ui_root->rect.w, ui_root->rect.h);


    cJSON_Delete(root_json);

    if(ui_root->font==NULL){
        ui_root->font=malloc(sizeof(Font));
        sprintf(ui_root->font->path,"%s","Roboto-Regular.ttf");
    }
    if(ui_root->assets==NULL){
        ui_root->assets=malloc(sizeof(Assets));
        sprintf(ui_root->assets->path,"%s","assets");
    }

    // 加载纹理资源
    // load_all_fonts 已经在 layer_create_from_json 之后调用过了

    load_textures(ui_root);
    layout_layer(ui_root);

    backend_run(ui_root);

    // 清理资源
    js_module_cleanup();  // 清理 JS 引擎
    // destroy_layer(ui_root);  // 暂时注释掉以避免内存问题
    popup_manager_cleanup();
    backend_quit();

    if (auto_test) {
        if (!backend_should_quit()) {
            fprintf(stderr, "AUTO_TEST: timeout (no YUI.exit)\n");
            return 2;
        }
        return backend_get_exit_code();
    }
    return 0;
}