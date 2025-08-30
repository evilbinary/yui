#include "backend.h"
#include "event.h"
#include "render.h"



#define WINDOW_WIDTH 1000

// ====================== 全局渲染器 ======================
SDL_Renderer* renderer = NULL;
float scale=1.0;
SDL_Window* window=NULL;
DFont* default_font=NULL;

// 检查是否Retina显示屏并获取缩放因子
float getDisplayScale(SDL_Window* window) {
    int renderW, renderH;
    int windowW, windowH;
    
    SDL_GetWindowSize(window, &windowW, &windowH);
    SDL_GL_GetDrawableSize(window, &renderW, &renderH);
    
    return (renderW) / windowW;
}


int backend_init(){
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
    
    window = SDL_CreateWindow("YUI Renderer", 
                                        SDL_WINDOWPOS_CENTERED,
                                        SDL_WINDOWPOS_CENTERED,
                                        800, 600, SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_SHOWN);
    renderer = SDL_CreateRenderer(window, -1, 
                                 SDL_RENDERER_ACCELERATED);

    scale = getDisplayScale(window);

    SDL_RenderSetScale(renderer, scale, scale);
    
    return 0;

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


void backend_render_text_copy(Texture * texture,
                   const Rect * srcrect,
                   const Rect * dstrect){

   SDL_RenderCopy(renderer, texture, srcrect, dstrect);                 
}

void backend_render_text_destroy(Texture * texture){
    SDL_DestroyTexture(texture);
}

void backend_run(Layer* ui_root){
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
}

DFont* backend_load_font(char* font_path,int size){
    TTF_Font* default_font = TTF_OpenFont(font_path,size*scale);
    if (!default_font) {
        printf("Warning: Could not load font 'Roboto-Regular.ttf', trying other fonts\n");
        // 尝试加载其他西文字体
        default_font = TTF_OpenFont("arial.ttf",size*scale);
        if (!default_font) {
            default_font = TTF_OpenFont("Arial.ttf",size*scale);
        }
        if (!default_font) {
            default_font = TTF_OpenFont("assets/Roboto-Regular.ttf",size*scale);
        }
        if (!default_font) {
            default_font = TTF_OpenFont("app/assets/Roboto-Regular.ttf",size*scale);
        }
    }
    TTF_SetFontHinting(default_font, TTF_HINTING_LIGHT); 
    TTF_SetFontOutline(default_font, 0); // 无轮廓

    return default_font;
}

void backend_quit(){
      // 清理资源
    IMG_Quit();
    if (default_font) {
        TTF_CloseFont(default_font);
    }
    TTF_Quit();
    
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

void backend_get_windowsize(int* width,int * height){
    SDL_GetWindowSize(window, width, height);
}

Texture* backend_load_texture(char* path){
     SDL_Surface* surface = IMG_Load(path);
     SDL_Texture* texture=NULL;
    if (surface) {
        texture = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_FreeSurface(surface);
    } else {
        printf("Failed to load image %s: %s\n", path, IMG_GetError());
    }
    return texture;
}

Texture* backend_render_texture(DFont* font,const char* text,Color color){
     SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text, color);
    if (!surface) return NULL;
    
    
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_SetTextureScaleMode(texture, SDL_ScaleModeBest);

    SDL_FreeSurface(surface);

    return texture;
}

void backend_render_clear_color(unsigned char r,unsigned char g,unsigned char b,unsigned char a){
    SDL_SetRenderDrawColor(renderer, r, g, b, a);
    SDL_RenderClear(renderer);
}

void backend_render_present(){
    SDL_RenderPresent(renderer);
}

void backend_delay(int delay){
    SDL_Delay(delay);
}

void backend_render_fill_rect(Rect* rect,Color color){
    SDL_SetRenderDrawColor(renderer, 
                                color.r, 
                                color.g, 
                                color.b, 
                                color.a);
    SDL_RenderFillRect(renderer, rect);
}

void backend_render_fill_rect_color(Rect* rect,unsigned char r,unsigned char g,unsigned char b,unsigned char a){
    SDL_SetRenderDrawColor(renderer, r, g, b, a);
    SDL_RenderFillRect(renderer, rect);
}

void backend_render_rect(Rect* rect,Color color){
     // 绘制按钮边框
    SDL_SetRenderDrawColor(renderer, 
                                color.r, 
                                color.g, 
                                color.b, 
                                color.a);
    SDL_RenderDrawRect(renderer, rect);
}

void backend_render_rect_color(Rect* rect,unsigned char r,unsigned char g,unsigned char b,unsigned char a){
     // 绘制按钮边框
    SDL_SetRenderDrawColor(renderer, r, g, b, a);
    SDL_RenderDrawRect(renderer,rect);
}


void backend_render_get_clip_rect(Rect* prev_clip){
    SDL_RenderGetClipRect(renderer, prev_clip);
}

void backend_render_set_clip_rect(Rect* clip){
    SDL_RenderSetClipRect(renderer, clip);
}


// 创建带透明度的颜色
SDL_Color create_color(Uint8 r, Uint8 g, Uint8 b, Uint8 a) {
    SDL_Color color = {r, g, b, a};
    return color;
}

// 绘制带透明度的矩形
void draw_rect(SDL_Renderer* renderer, int x, int y, int w, int h, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_Rect rect = {x, y, w, h};
    SDL_RenderFillRect(renderer, &rect);
}

// 绘制带透明度的圆形
void draw_circle(SDL_Renderer* renderer, int center_x, int center_y, int radius, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    
    for (int x = -radius; x <= radius; x++) {
        for (int y = -radius; y <= radius; y++) {
            if (x*x + y*y <= radius*radius) {
                SDL_RenderDrawPoint(renderer, center_x + x, center_y + y);
            }
        }
    }
}

// // 绘制带透明度的多边形
// void draw_polygon(SDL_Renderer* renderer, SDL_Point points[], int count, SDL_Color color) {
//     SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    
//     // 绘制多边形轮廓
//     for (int i = 0; i < count; i++) {
//         int next = (i + 1) % count;
//         SDL_RenderDrawLine(renderer, points[i].x, points[i].y, points[next].x, points[next].y);
//     }
    
//     // 填充多边形 (简单实现，实际应用中可能需要更复杂的算法)
//     int min_y = points[0].y, max_y = points[0].y;
//     for (int i = 1; i < count; i++) {
//         if (points[i].y < min_y) min_y = points[i].y;
//         if (points[i].y > max_y) max_y = points[i].y;
//     }
    
//     for (int y = min_y; y <= max_y; y++) {
//         for (int x = 0; x < WINDOW_WIDTH; x++) {
//             int inside = 0;
//             for (int i = 0, j = count-1; i < count; j = i++) {
//                 if (((points[i].y > y) != (points[j].y > y)) &&
//                     (x < (points[j].x - points[i].x) * (y - points[i].y) / (points[j].y - points[i].y) + points[i].x)) {
//                     inside = !inside;
//                 }
//             }
//             if (inside) {
//                 SDL_RenderDrawPoint(renderer, x, y);
//             }
//         }
//     }
// }