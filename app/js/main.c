#include <stdio.h>


#include <stdbool.h>
#include <limits.h>

#include "cJSON.h"
#include "event.h"
#include "layer.h"
#include "render.h"
#include "layout.h"
#include "backend.h"
#include "popup_manager.h"
#include "../lib/jsmodule/js_module.h"

#if defined(_WIN32)
#include <windows.h>
int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    int argc = __argc;
    char** argv = __argv;
    return main(argc, argv);
}
#endif


// ====================== 主入口 ======================
int main(int argc, char* argv[]) {

    backend_init();
    popup_manager_init();

    // 初始化 JS 引擎
    if (js_module_init() != 0) {
        fprintf(stderr, "Failed to initialize JavaScript engine\n");
        return -1;
    }


    char* json_path="app/js/app.json";
    // 加载UI描述文件
    if(argc>1){
        json_path=argv[1];
    }

    printf("DEBUG: Loading JSON from path: %s\n", json_path);


    cJSON* root_json=parse_json(json_path);    
    Layer* ui_root = layer_create_from_json(root_json,NULL);

    // 设置 UI 根图层到 JS 模块
    js_module_init_layer(ui_root);

    // 初始化字体缓存（backend已经初始化）
    // 在parse_layer后加载所有字体
    load_all_fonts(ui_root);

     // 加载并执行 JS 文件
     printf("加载并执行 JS 文件...\n");
     int count = js_module_load_from_json(root_json, json_path);
 
     // 如果主 JSON 中没有找到 js 文件，检查是否有 source 属性
     if (count <= 0) {
         cJSON* source = cJSON_GetObjectItem(root_json, "source");
         if (source && cJSON_IsString(source)) {
             const char* source_path = source->valuestring;
             printf("DEBUG: No JS found in main JSON, checking source file: %s\n", source_path);
 
             // 加载 source 指向的 JSON 文件
             cJSON* source_json = parse_json((char*)source_path);
             if (source_json) {
                 // 从 source JSON 中加载 JS（传递 source 文件路径）
                 int source_count = js_module_load_from_json(source_json, source_path);
                 printf("DEBUG: Loaded %d JS file(s) from source JSON\n", source_count);
                 cJSON_Delete(source_json);
                 count += source_count;
             } else {
                 printf("ERROR: Failed to load source JSON file: %s\n", source_path);
             }
         }
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

    // 如果根图层没有设置宽度和高度，则根据窗口大小设置
    printf("ui_root %d,%d\n",ui_root->rect.w,ui_root->rect.h);


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
    return 0;
}