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
#include "yaml_cjson.h"

#if defined(_WIN32)
#include <windows.h>
int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    extern int __argc;
    extern char** __argv;
    return main(__argc, __argv);
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

    cJSON* root_json=parse_yaml_json_file(json_path);
    Layer* ui_root = layer_create_from_json(root_json,NULL);

    
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