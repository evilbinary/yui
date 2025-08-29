#include <stdio.h>


#include <stdbool.h>
#include <limits.h>

#include "cJSON.h"
#include "event.h"
#include "layer.h"
#include "render.h"
#include "layout.h"


// ====================== 事件处理器 ======================

void handle_scroll_event(Layer* layer, int scroll_delta) {
    if (layer->scrollable && layer->type == LIST) {
        layer->scroll_offset += scroll_delta * 20; // 每次滚动20像素
        
        // 限制滚动范围
        int content_height = 0;
        for (int i = 0; i < layer->child_count; i++) {
            content_height += layer->children[i]->rect.h;
            if (i > 0) content_height += layer->layout_manager->spacing;
        }
        
        int visible_height = layer->rect.h - layer->layout_manager->padding[0] - layer->layout_manager->padding[2];
        
        // 不允许向上滚动超过顶部
        if (layer->scroll_offset < 0) {
            layer->scroll_offset = 0;
        }
        // 不允许向下滚动超过底部
        else if (content_height > visible_height && layer->scroll_offset > content_height - visible_height) {
            layer->scroll_offset = content_height - visible_height;
        }
        // 如果内容高度小于可见高度，则不能滚动
        else if (content_height <= visible_height) {
            layer->scroll_offset = 0;
        }
        
        // 触发滚动事件回调
        if (layer->event && layer->event->scroll) {
            layer->event->scroll(layer);
        }
        
        // 重新布局子元素
        layout_layer(layer);
    }
}

void handle_event(Layer* root, SDL_Event* event) {
    if (event->type == SDL_MOUSEBUTTONDOWN) {
        SDL_Point mouse_pos = { event->button.x, event->button.y };
        
        // 检测点击图层 (实际应使用空间分割优化)
        if (SDL_PointInRect(&mouse_pos, &root->rect)) {
            if (root->event&& root->event->click){
                root->event->click(root);
            } 
            for (int i = 0; i < root->child_count; i++) {
                handle_event(root->children[i], event);
            }
            if(root->sub){
                handle_event(root->sub, event);
            }
        }
    }
    // 添加鼠标滚轮事件处理
    else if (event->type == SDL_MOUSEWHEEL) {
        // 处理鼠标滚轮事件，传递给所有支持滚动的图层
        handle_scroll_event(root, -event->wheel.y); // 反向滚动更符合用户习惯
        for (int i = 0; i < root->child_count; i++) {
            handle_scroll_event(root->children[i], -event->wheel.y);
        }
        if(root->sub){
            handle_scroll_event(root->sub, -event->wheel.y);
        }
    }
}

// 检查是否Retina显示屏并获取缩放因子
float getDisplayScale(SDL_Window* window) {
    int renderW, renderH;
    int windowW, windowH;
    
    SDL_GetWindowSize(window, &windowW, &windowH);
    SDL_GL_GetDrawableSize(window, &renderW, &renderH);
    
    return (renderW) / windowW;
}

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
    TTF_Font* default_font = TTF_OpenFont(font_path, root->font->size*scale);
    if (!default_font) {
        printf("Warning: Could not load font 'Roboto-Regular.ttf', trying other fonts\n");
        // 尝试加载其他西文字体
        default_font = TTF_OpenFont("arial.ttf", root->font->size*scale);
        if (!default_font) {
            default_font = TTF_OpenFont("Arial.ttf", root->font->size*scale);
        }
        if (!default_font) {
            default_font = TTF_OpenFont("assets/Roboto-Regular.ttf", root->font->size*scale);
        }
        if (!default_font) {
            default_font = TTF_OpenFont("app/assets/Roboto-Regular.ttf", root->font->size*scale);
        }
    }
    TTF_SetFontHinting(default_font, TTF_HINTING_LIGHT); 
    TTF_SetFontOutline(default_font, 0); // 无轮廓

    root->font->default_font=default_font;
}

void hello_world(Layer* layer) {
    printf("你好，世界！ %s\n",layer->text);
}

// ====================== 主入口 ======================
int main(int argc, char* argv[]) {
    // 初始化SDL
    SDL_Init(SDL_INIT_VIDEO);
    
    // 初始化SDL_image库，支持多种图片格式
    int imgFlags = IMG_INIT_PNG | IMG_INIT_JPG | IMG_INIT_TIF;
    if (!(IMG_Init(imgFlags) & imgFlags)) {
        printf("SDL_image initialization failed: %s\n", IMG_GetError());
        return -1;
    }
    
    // 初始化TTF
    if (TTF_Init() == -1) {
        printf("TTF initialization failed: %s\n", TTF_GetError());
        return -1;
    }
    
    SDL_Window* window = SDL_CreateWindow("YUI Renderer", 
                                        SDL_WINDOWPOS_CENTERED,
                                        SDL_WINDOWPOS_CENTERED,
                                        800, 600, SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_SHOWN);
    renderer = SDL_CreateRenderer(window, -1, 
                                 SDL_RENDERER_ACCELERATED);

    scale = getDisplayScale(window);

    SDL_RenderSetScale(renderer, scale, scale);
    
    
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
        SDL_GetWindowSize(window, &window_width, &window_height);
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
    
    // 主循环
    SDL_Event event;
    int running = 1;
    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = 0;
            handle_event(ui_root, &event);
        }
        
        SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
        SDL_RenderClear(renderer);
        
        render_layer(ui_root);  // 执行渲染管线
        
        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }
    
    // 清理资源
    IMG_Quit();
    if (default_font) {
        TTF_CloseFont(default_font);
    }
    TTF_Quit();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}