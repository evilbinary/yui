#include "backend.h"
#include "event.h"
#include "render.h"
#include "ytype.h"



#define WINDOW_WIDTH 1000
#define MAX_TOUCHES 10

// ====================== 全局渲染器 ======================
SDL_Renderer* renderer = NULL;
float scale=1.0;
SDL_Window* window=NULL;
DFont* default_font=NULL;

// 触屏事件状态
typedef struct {
    int fingerCount;          // 当前触摸点数量
    int lastX[MAX_TOUCHES];   // 上次触摸点X坐标
    int lastY[MAX_TOUCHES];   // 上次触摸点Y坐标
    Uint32 startTime;         // 触摸开始时间戳（用于长按检测）
    Uint32 lastTapTime;       // 上次点击时间戳（用于双击检测）
    int tapCount;             // 点击次数
    int longPressDetected;    // 是否已检测到长按
} TouchState;

#define MAX_TOUCHES 10
TouchState touchState = {0};

// 检查是否Retina显示屏并获取缩放因子
float getDisplayScale(SDL_Window* window) {
    int renderW, renderH;
    int windowW, windowH;
    
    SDL_GetWindowSize(window, &windowW, &windowH);
    SDL_GL_GetDrawableSize(window, &renderW, &renderH);
    
    return (renderW) / windowW;
}

void draw_rounded_rect_with_border(SDL_Renderer* renderer, int x, int y, int w, int h, int radius, int border_width, SDL_Color bg_color, SDL_Color border_color);

void draw_rounded_rect(SDL_Renderer* renderer, int x, int y, int w, int h, int radius, SDL_Color color);

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
    
    // 初始化触摸状态
    memset(&touchState, 0, sizeof(TouchState));
    
    return 0;

}

// 重置触摸状态
void resetTouchState() {
    touchState.fingerCount = 0;
    memset(touchState.lastX, 0, sizeof(touchState.lastX));
    memset(touchState.lastY, 0, sizeof(touchState.lastY));
    touchState.longPressDetected = 0;
}

// 检查点是否在图层内
int pointInLayer(SDL_Point* point, Layer* layer) {
    return SDL_PointInRect(point, &layer->rect);
}

// 递归处理图层的触屏事件
void propagateTouchEvent(Layer* layer, TouchEvent* event) {
    if (!layer || !event) return;
    
    // 检查点是否在当前图层内
    SDL_Point point = {event->x, event->y};
    if (pointInLayer(&point, layer)) {
        // 调用当前图层的触摸事件处理函数
        if (layer->handle_touch_event) {
            layer->handle_touch_event(layer,event);
        }
        
        // 递归处理子图层
        for (int i = 0; i < layer->child_count; i++) {
            propagateTouchEvent(layer->children[i], event);
        }
        
        // 处理子图层
        if (layer->sub) {
            propagateTouchEvent(layer->sub, event);
        }
    }
}

void handle_event(Layer* root, SDL_Event* event) {
    // 处理键盘事件
    if (event->type == SDL_KEYDOWN) {
        // 创建键盘按键事件
        KeyEvent key_event;
        key_event.type = KEY_EVENT_DOWN;
        key_event.data.key.key_code = event->key.keysym.sym;
        key_event.data.key.mod = event->key.keysym.mod;
        key_event.data.key.repeat = event->key.repeat;
        
        handle_key_event(root, &key_event);
    } else if (event->type == SDL_TEXTINPUT) {
        // 创建文本输入事件
        KeyEvent key_event;
        key_event.type = KEY_EVENT_TEXT_INPUT;
        strncpy(key_event.data.text.text, event->text.text, 31);
        key_event.data.text.text[31] = '\0';
        
        handle_key_event(root, &key_event);
    }
    
    // 处理鼠标事件
    if (event->type == SDL_MOUSEBUTTONDOWN || event->type == SDL_MOUSEBUTTONUP || event->type == SDL_MOUSEMOTION) {
        int mouse_x = event->motion.x;
        int mouse_y = event->motion.y;
        SDL_Point mouse_pos = { mouse_x, mouse_y };
        MouseEvent mouse_event = {
            .x = mouse_x,
            .y = mouse_y,
            .button = event->button.button,
            .state = (event->type == SDL_MOUSEBUTTONDOWN) ? 1 : 0
        };
        
        // 调用事件系统处理滚动条拖动
        handle_scrollbar_drag_event(root, mouse_x, mouse_y, event->type);
        
        if (SDL_PointInRect(&mouse_pos, &root->rect)) {
            handle_mouse_event(root, &mouse_event);
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
    // 触摸开始事件
    else if (event->type == SDL_FINGERDOWN) {
        // 更新触摸状态
        touchState.fingerCount++;
        if (touchState.fingerCount == 1) {
            touchState.startTime = SDL_GetTicks();
            touchState.longPressDetected = 0;
        }
        
        // 转换触摸坐标为窗口坐标
        int x, y;
        SDL_GetWindowSize(window, &x, &y);
        x = (int)(event->tfinger.x * x);
        y = (int)(event->tfinger.y * y);
        
        // 保存触摸位置
        int touchId = event->tfinger.fingerId % MAX_TOUCHES;
        touchState.lastX[touchId] = x;
        touchState.lastY[touchId] = y;
        
        // 创建触摸事件
        TouchEvent touchEvent;
        memset(&touchEvent, 0, sizeof(TouchEvent));
        touchEvent.type = TOUCH_TYPE_START;
        touchEvent.x = x;
        touchEvent.y = y;
        touchEvent.deltaX = 0;
        touchEvent.deltaY = 0;
        touchEvent.scale = 1.0f;
        touchEvent.rotation = 0.0f;
        touchEvent.fingerCount = touchState.fingerCount;
        touchEvent.timestamp = SDL_GetTicks();
        
        // 传播触摸事件
        propagateTouchEvent(root, &touchEvent);
        
        // 双击检测
        Uint32 currentTime = SDL_GetTicks();
        if (currentTime - touchState.lastTapTime < 300) {
            touchState.tapCount++;
            if (touchState.tapCount == 2) {
                // 触发双击事件
                touchEvent.type = TOUCH_TYPE_DOUBLE_TAP;
                propagateTouchEvent(root, &touchEvent);
                touchState.tapCount = 0;
            }
        } else {
            touchState.tapCount = 1;
        }
        touchState.lastTapTime = currentTime;
    }
    // 触摸移动事件
    else if (event->type == SDL_FINGERMOTION) {
        // 转换触摸坐标为窗口坐标
        int x, y;
        SDL_GetWindowSize(window, &x, &y);
        x = (int)(event->tfinger.x * x);
        y = (int)(event->tfinger.y * y);
        
        // 计算位移
        int touchId = event->tfinger.fingerId % MAX_TOUCHES;
        int deltaX = x - touchState.lastX[touchId];
        int deltaY = y - touchState.lastY[touchId];
        
        // 保存当前位置
        touchState.lastX[touchId] = x;
        touchState.lastY[touchId] = y;
        
        // 创建触摸事件
        TouchEvent touchEvent;
        memset(&touchEvent, 0, sizeof(TouchEvent));
        touchEvent.type = TOUCH_TYPE_MOVE;
        touchEvent.x = x;
        touchEvent.y = y;
        touchEvent.deltaX = deltaX;
        touchEvent.deltaY = deltaY;
        touchEvent.scale = 1.0f;
        touchEvent.rotation = 0.0f;
        touchEvent.fingerCount = touchState.fingerCount;
        touchEvent.timestamp = SDL_GetTicks();
        
        // 传播触摸事件
        propagateTouchEvent(root, &touchEvent);
        
        // 长按检测（500ms）
        if (touchState.fingerCount == 1 && !touchState.longPressDetected) {
            if (SDL_GetTicks() - touchState.startTime > 500) {
                touchEvent.type = TOUCH_TYPE_LONG_PRESS;
                propagateTouchEvent(root, &touchEvent);
                touchState.longPressDetected = 1;
            }
        }
    }
    // 触摸结束事件
    else if (event->type == SDL_FINGERUP) {
        // 更新触摸状态
        if (touchState.fingerCount > 0) {
            touchState.fingerCount--;
        }
        
        // 转换触摸坐标为窗口坐标
        int x, y;
        SDL_GetWindowSize(window, &x, &y);
        x = (int)(event->tfinger.x * x);
        y = (int)(event->tfinger.y * y);
        
        // 创建触摸事件
        TouchEvent touchEvent;
        memset(&touchEvent, 0, sizeof(TouchEvent));
        touchEvent.type = TOUCH_TYPE_END;
        touchEvent.x = x;
        touchEvent.y = y;
        touchEvent.deltaX = 0;
        touchEvent.deltaY = 0;
        touchEvent.scale = 1.0f;
        touchEvent.rotation = 0.0f;
        touchEvent.fingerCount = touchState.fingerCount;
        touchEvent.timestamp = SDL_GetTicks();
        
        // 传播触摸事件
        propagateTouchEvent(root, &touchEvent);
        
        // 如果所有手指都离开屏幕，重置状态
        if (touchState.fingerCount == 0) {
            resetTouchState();
        }
    }
    // 多点触控手势事件（捏合、旋转）
    else if (event->type == SDL_MULTIGESTURE) {
        // 获取窗口尺寸
        int winW, winH;
        SDL_GetWindowSize(window, &winW, &winH);
        
        // 创建触摸事件
        TouchEvent touchEvent;
        memset(&touchEvent, 0, sizeof(TouchEvent));
        
        // 设置中心点坐标
        touchEvent.x = (int)(event->mgesture.x * winW);
        touchEvent.y = (int)(event->mgesture.y * winH);
        touchEvent.fingerCount = event->mgesture.numFingers;
        touchEvent.timestamp = SDL_GetTicks();
        
        // 处理缩放（捏合）
        if (event->mgesture.dDist != 0.0f) {
            touchEvent.type = TOUCH_TYPE_PINCH;
            touchEvent.scale = 1.0f + event->mgesture.dDist;
            propagateTouchEvent(root, &touchEvent);
        }
        
        // 处理旋转
        if (event->mgesture.dTheta != 0.0f) {
            touchEvent.type = TOUCH_TYPE_ROTATE;
            touchEvent.rotation = event->mgesture.dTheta;
            propagateTouchEvent(root, &touchEvent);
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

int backend_query_texture(Texture * texture,
                     Uint32 * format, int *access,
                     int *w, int *h){

   return SDL_QueryTexture(texture,format,access,w,h);                      
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

// 绘制带圆角的填充矩形
void backend_render_rounded_rect(Rect* rect, Color color, int radius) {
    draw_rounded_rect(renderer, rect->x, rect->y, rect->w, rect->h, radius, color);
}

// 绘制带圆角的填充矩形（直接指定颜色值）
void backend_render_rounded_rect_color(Rect* rect, unsigned char r, unsigned char g, unsigned char b, unsigned char a, int radius) {
    SDL_Color color = {r, g, b, a};
    draw_rounded_rect(renderer, rect->x, rect->y, rect->w, rect->h, radius, color);
}

// 绘制带圆角和边框的填充矩形
void backend_render_rounded_rect_with_border(Rect* rect, Color bg_color, int radius, int border_width, Color border_color) {
    draw_rounded_rect_with_border(renderer, rect->x, rect->y, rect->w, rect->h, radius, border_width, bg_color, border_color);
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
//     
//     // 绘制多边形轮廓
//     for (int i = 0; i < count; i++) {
//         int next = (i + 1) % count;
//         SDL_RenderDrawLine(renderer, points[i].x, points[i].y, points[next].x, points[next].y);
//     }
//     
//     // 填充多边形 (简单实现，实际应用中可能需要更复杂的算法)
//     int min_y = points[0].y, max_y = points[0].y;
//     for (int i = 1; i < count; i++) {
//         if (points[i].y < min_y) min_y = points[i].y;
//         if (points[i].y > max_y) max_y = points[i].y;
//     }
//     
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

// 绘制带透明度的圆角矩形
void draw_rounded_rect(SDL_Renderer* renderer, int x, int y, int w, int h, int radius, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    
    // 限制圆角半径不超过宽高的一半
    int r = radius;
    if (r > w / 2) r = w / 2;
    if (r > h / 2) r = h / 2;
    
    // 绘制四个角的圆弧
    for (int i = 0; i <= r; i++) {
        // 计算偏移量
        int j = (int)sqrt(r * r - i * i);
        
        // 左上角圆弧
        SDL_RenderDrawPoint(renderer, x + r - i, y + r - j);
        SDL_RenderDrawPoint(renderer, x + r - j, y + r - i);
        // 右上角圆弧
        SDL_RenderDrawPoint(renderer, x + w - r + i, y + r - j);
        SDL_RenderDrawPoint(renderer, x + w - r + j, y + r - i);
        // 左下角圆弧
        SDL_RenderDrawPoint(renderer, x + r - i, y + h - r + j);
        SDL_RenderDrawPoint(renderer, x + r - j, y + h - r + i);
        // 右下角圆弧
        SDL_RenderDrawPoint(renderer, x + w - r + i, y + h - r + j);
        SDL_RenderDrawPoint(renderer, x + w - r + j, y + h - r + i);
    }
    
    // 绘制四条边
    SDL_RenderDrawLine(renderer, x + r, y, x + w - r, y); // 上边
    SDL_RenderDrawLine(renderer, x + r, y + h, x + w - r, y + h); // 下边
    SDL_RenderDrawLine(renderer, x, y + r, x, y + h - r); // 左边
    SDL_RenderDrawLine(renderer, x + w, y + r, x + w, y + h - r); // 右边
    
    // 填充圆角矩形内部
    for (int fill_y = y + 1; fill_y < y + h - 1; fill_y++) {
        // 计算当前行的左右边界
        int left = x + 1;
        int right = x + w - 1;
        
        // 调整圆角部分的左右边界
        if (fill_y < y + r) {
            int offset = (int)(sqrt(r * r - (fill_y - (y + r)) * (fill_y - (y + r))));
            left = x + r - offset;
            right = x + w - r + offset;
        } else if (fill_y > y + h - r) {
            int offset = (int)(sqrt(r * r - ((y + h - r) - fill_y) * ((y + h - r) - fill_y)));
            left = x + r - offset;
            right = x + w - r + offset;
        }
        
        SDL_RenderDrawLine(renderer, left, fill_y, right, fill_y);
    }
}

// 绘制带边框的圆角矩形
void draw_rounded_rect_with_border(SDL_Renderer* renderer, int x, int y, int w, int h, int radius, int border_width, SDL_Color bg_color, SDL_Color border_color) {
    // 先绘制内部填充的圆角矩形
    draw_rounded_rect(renderer, x, y, w, h, radius, bg_color);
    
    // 设置边框颜色
    SDL_SetRenderDrawColor(renderer, border_color.r, border_color.g, border_color.b, border_color.a);
    
    // 限制圆角半径不超过宽高的一半
    int r = radius;
    if (r > w / 2) r = w / 2;
    if (r > h / 2) r = h / 2;
    
    // 绘制圆角边框（多次绘制实现宽度效果）
    for (int bw = 0; bw < border_width; bw++) {
        int current_r = r - bw;
        int current_x = x + bw;
        int current_y = y + bw;
        int current_w = w - 2 * bw;
        int current_h = h - 2 * bw;
        
        // 绘制四个角的圆弧
        for (int i = 0; i <= current_r; i++) {
            int j = (int)sqrt(current_r * current_r - i * i);
            
            // 左上角圆弧
            SDL_RenderDrawPoint(renderer, current_x + current_r - i, current_y + current_r - j);
            SDL_RenderDrawPoint(renderer, current_x + current_r - j, current_y + current_r - i);
            // 右上角圆弧
            SDL_RenderDrawPoint(renderer, current_x + current_w - current_r + i, current_y + current_r - j);
            SDL_RenderDrawPoint(renderer, current_x + current_w - current_r + j, current_y + current_r - i);
            // 左下角圆弧
            SDL_RenderDrawPoint(renderer, current_x + current_r - i, current_y + current_h - current_r + j);
            SDL_RenderDrawPoint(renderer, current_x + current_r - j, current_y + current_h - current_r + i);
            // 右下角圆弧
            SDL_RenderDrawPoint(renderer, current_x + current_w - current_r + i, current_y + current_h - current_r + j);
            SDL_RenderDrawPoint(renderer, current_x + current_w - current_r + j, current_y + current_h - current_r + i);
        }
        
        // 绘制四条边
        SDL_RenderDrawLine(renderer, current_x + current_r, current_y, current_x + current_w - current_r, current_y); // 上边
        SDL_RenderDrawLine(renderer, current_x + current_r, current_y + current_h, current_x + current_w - current_r, current_y + current_h); // 下边
        SDL_RenderDrawLine(renderer, current_x, current_y + current_r, current_x, current_y + current_h - current_r); // 左边
        SDL_RenderDrawLine(renderer, current_x + current_w, current_y + current_r, current_x + current_w, current_y + current_h - current_r); // 右边
    }
}

// Add this function anywhere in backend_sdl.c after the global renderer declaration

// 绘制线段
void backend_render_line(int x1, int y1, int x2, int y2, Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
}