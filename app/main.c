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

#if defined(_WIN32)
#include <windows.h>
int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    int argc = __argc;
    char** argv = __argv;
    return main(argc, argv);
}
#endif

void hello_world(Layer* layer) {
    printf("你好，世界！ %s\n",layer->text);
}

void hello_touch(Layer* layer) {
    printf("touch %s\n",layer->text);
}

// ====================== 主入口 ======================
int main(int argc, char* argv[]) {

    backend_init();
    popup_manager_init();
    
    
    char* json_path="app/app.json";
    // 加载UI描述文件
    if(argc>1){
        json_path=argv[1];
    }
    
    printf("DEBUG: Loading JSON from path: %s\n", json_path);

    //注册事件
    register_event_handler("@hello", hello_world);
    register_event_handler("@helloTouch", hello_touch);

    cJSON* root_json=parse_json(json_path);
    Layer* ui_root = parse_layer(root_json,NULL);

    
    if (ui_root->rect.w <= 0 || ui_root->rect.h <= 0) {
        int window_width, window_height;
        backend_get_windowsize(&window_width, &window_height);
        if (ui_root->rect.w <= 0) {
            ui_root->rect.w = window_width;
        }
        if (ui_root->rect.h <= 0) {
            ui_root->rect.h = window_height;
        }
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

    // 加载字体资源（使用字体缓存，递归加载所有图层的字体）
    load_all_fonts(ui_root);

    // 加载纹理资源
    load_textures(ui_root);
    layout_layer(ui_root);
    
    backend_run(ui_root);  
    
    // 清理资源
    // destroy_layer(ui_root);  // 暂时注释掉以避免内存问题
    popup_manager_cleanup();
    backend_quit();
    return 0;
}