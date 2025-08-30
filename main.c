#include <stdio.h>


#include <stdbool.h>
#include <limits.h>

#include "cJSON.h"
#include "event.h"
#include "layer.h"
#include "render.h"
#include "layout.h"
#include "backend.h"

void load_font(Layer* root){
    // 加载默认字体 (需要在项目目录下提供字体文件)
    char font_path[MAX_PATH];
    if(root->assets){
        snprintf(font_path, sizeof(font_path), "%s/%s", root->assets->path, root->font->path);
    }else{
        snprintf(font_path, sizeof(font_path), "%s", root->font->path);
    }
    if(root->font&& root->font->size==0){
        root->font->size=16;
    }

    DFont* default_font=backend_load_font(font_path, root->font->size*scale);

    root->font->default_font=default_font;
}

void hello_world(Layer* layer) {
    printf("你好，世界！ %s\n",layer->text);
}

// ====================== 主入口 ======================
int main(int argc, char* argv[]) {

    backend_init();
    
    
    char* json_path="app.json";
    // 加载UI描述文件
    if(argc>1){
        json_path=argv[1];
    }

    //注册事件
    register_event_handler("@hello", hello_world);
    
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

    // 加载纹理资源
    load_font(ui_root);
    TTF_Font* default_font=ui_root->font->default_font;

    load_textures(ui_root);
    layout_layer(ui_root);
    
    backend_run(ui_root);  

    backend_quit();
    return 0;
}