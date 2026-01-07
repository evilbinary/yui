#include "backend.h"
#include "event.h"
#include "render.h"
#include "ytype.h"
#include "popup_manager.h"
#include <stdbool.h>  // 添加支持bool类型
#include <math.h>     // 添加数学函数支持

#define WINDOW_WIDTH 1000
#define MAX_TOUCHES 10
#define MAX_UPDATE_CALLBACKS 16

// ====================== 全局渲染器 ======================
SDL_Renderer* renderer = NULL;
float scale=1.0;
SDL_Window* window=NULL;
DFont* default_font=NULL;

// 主循环更新回调管理
static UpdateCallback update_callbacks[MAX_UPDATE_CALLBACKS] = {NULL};
static int update_callback_count = 0;

// 字体缓存结构
typedef struct {
    char font_path[MAX_PATH];
    int size;
    char weight[32];  // "normal", "bold", "light"
    TTF_Font* font;
    Uint32 last_used;
} FontCacheEntry;

#define MAX_FONT_CACHE_ENTRIES 50
FontCacheEntry font_cache[MAX_FONT_CACHE_ENTRIES] = {0};
int font_cache_initialized = 0;

// ====================== 字体缓存管理 ======================
void init_font_cache() {
    if (font_cache_initialized) return;
    
    for (int i = 0; i < MAX_FONT_CACHE_ENTRIES; i++) {
        font_cache[i].font = NULL;
        font_cache[i].font_path[0] = '\0';
        font_cache[i].size = 0;
        font_cache[i].weight[0] = '\0';
        font_cache[i].last_used = 0;
    }
    
    font_cache_initialized = 1;
}

void cleanup_font_cache() {
    printf("Cleaning up font cache...\n");
    for (int i = 0; i < MAX_FONT_CACHE_ENTRIES; i++) {
        if (font_cache[i].font) {
            TTF_CloseFont(font_cache[i].font);
            font_cache[i].font = NULL;
        }
        font_cache[i].font_path[0] = '\0';
        font_cache[i].size = 0;
        font_cache[i].weight[0] = '\0';
    }
    printf("Font cache cleanup completed\n");
}

// 在缓存中查找字体
TTF_Font* find_font_in_cache(const char* font_path, int size, const char* weight) {
    for (int i = 0; i < MAX_FONT_CACHE_ENTRIES; i++) {
        if (font_cache[i].font &&
            strcmp(font_cache[i].font_path, font_path) == 0 &&
            font_cache[i].size == size &&
            strcmp(font_cache[i].weight, weight) == 0) {
            font_cache[i].last_used = SDL_GetTicks();
            return font_cache[i].font;
        }
    }
    return NULL;
}

// 添加字体到缓存
void add_font_to_cache(const char* font_path, int size, const char* weight, TTF_Font* font) {
    // 查找空闲位置或最久未使用的位置
    int cache_index = -1;
    Uint32 oldest_time = SDL_GetTicks();
    
    // 首先查找空闲位置
    for (int i = 0; i < MAX_FONT_CACHE_ENTRIES; i++) {
        if (!font_cache[i].font) {
            cache_index = i;
            break;
        }
    }
    
    // 如果没有空闲位置，查找最久未使用的位置
    if (cache_index == -1) {
        for (int i = 0; i < MAX_FONT_CACHE_ENTRIES; i++) {
            if (font_cache[i].last_used < oldest_time) {
                oldest_time = font_cache[i].last_used;
                cache_index = i;
            }
        }
    }
    
    if (cache_index >= 0) {
        // 如果该位置已有字体，先关闭它
        if (font_cache[cache_index].font) {
            TTF_CloseFont(font_cache[cache_index].font);
        }
        
        // 添加新字体到缓存
        strncpy(font_cache[cache_index].font_path, font_path, MAX_PATH - 1);
        font_cache[cache_index].font_path[MAX_PATH - 1] = '\0';
        font_cache[cache_index].size = size;
        strncpy(font_cache[cache_index].weight, weight, 31);
        font_cache[cache_index].weight[31] = '\0';
        font_cache[cache_index].font = font;
        font_cache[cache_index].last_used = SDL_GetTicks();
    }
}

// 毛玻璃效果缓存结构
typedef struct {
    SDL_Texture* texture;
    int x, y, w, h;
    int blur_radius;
    float saturation, brightness;
    Uint32 last_used;
    int in_use;
} BlurCacheEntry;

#define MAX_BLUR_CACHE_ENTRIES 5
BlurCacheEntry blur_cache[MAX_BLUR_CACHE_ENTRIES] = {0};
int blur_cache_initialized = 0;

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

void cleanup_corner_texture_cache();

int backend_init(){
    // 初始化SDL
    SDL_Init(SDL_INIT_VIDEO);
    

    
    // 设置渲染质量为最佳（抗锯齿）
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "best");

    // Windows：默认“点击激活窗口”会吞掉首次点击，导致控件第一次点不中（拿不到焦点）
    // 开启 click-through 让首次点击也能送达应用侧
    SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");
    
    window = SDL_CreateWindow("YUI",
                                        SDL_WINDOWPOS_CENTERED,
                                        SDL_WINDOWPOS_CENTERED,
                                        800, 600, SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_SHOWN);

    // 尝试创建渲染器，优先使用硬件加速和垂直同步
    Uint32 renderer_flags = SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC;
    renderer = SDL_CreateRenderer(window, -1, renderer_flags);

    if (!renderer) {
        // 如果同时支持加速和垂直同步失败，尝试只使用硬件加速
        printf("Warning: Failed to create renderer with ACCELERATED | PRESENTVSYNC: %s\n", SDL_GetError());
        printf("Retrying with ACCELERATED only...\n");

        renderer_flags = SDL_RENDERER_ACCELERATED;
        renderer = SDL_CreateRenderer(window, -1, renderer_flags);

        if (!renderer) {
            // 如果硬件加速也失败，使用软件渲染器
            printf("Warning: Failed to create hardware accelerated renderer: %s\n", SDL_GetError());
            printf("Falling back to software renderer...\n");

            renderer_flags = SDL_RENDERER_SOFTWARE;
            renderer = SDL_CreateRenderer(window, -1, renderer_flags);
        }
    }

    if (!renderer) {
        printf("Error: Failed to create any renderer: %s\n", SDL_GetError());
        return -1;
    }

    // 检查渲染器信息
    SDL_RendererInfo renderer_info;
    if (SDL_GetRendererInfo(renderer, &renderer_info) == 0) {
        printf("Renderer created successfully!\n");
        printf("  Renderer name: %s\n", renderer_info.name);
        printf("  Renderer flags: ");

        if (renderer_info.flags & SDL_RENDERER_ACCELERATED) {
            printf("ACCELERATED ");
        }
        if (renderer_info.flags & SDL_RENDERER_PRESENTVSYNC) {
            printf("PRESENTVSYNC ");
        }
        if (renderer_info.flags & SDL_RENDERER_SOFTWARE) {
            printf("SOFTWARE ");
        }
        if (renderer_info.flags & SDL_RENDERER_TARGETTEXTURE) {
            printf("TARGETTEXTURE ");
        }
        printf("\n");

        // 检查是否支持垂直同步
        if (renderer_info.flags & SDL_RENDERER_PRESENTVSYNC) {
            printf("  Vertical Sync: Enabled\n");
        } else {
            printf("  Vertical Sync: Disabled (not supported)\n");
        }
    }
    
    // 启用透明度混合
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    scale = getDisplayScale(window);

    SDL_RenderSetScale(renderer, scale, scale);
    
    // 初始化SDL_image库，支持多种图片格式
    int imgFlags = IMG_INIT_PNG | IMG_INIT_JPG | IMG_INIT_TIF | IMG_INIT_WEBP;
    if (!(IMG_Init(imgFlags) & imgFlags)) {
        printf("SDL_image initialization failed: %s\n", IMG_GetError());
        return -1;
    }

    // 检查SDL_image版本是否支持SVG（SDL_image 2.6.0及以上版本支持SVG）
    const char* imgVersion = IMG_Linked_Version() ? SDL_GetRevision() : "Unknown";
    printf("SDL_image version: %s\n", imgVersion);
    
    // 初始化TTF
    if (TTF_Init() == -1) {
        printf("TTF initialization failed: %s\n", TTF_GetError());
        return -1;
    }
    
    // 初始化字体缓存
    init_font_cache();
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
        int mouse_x, mouse_y;

        // 根据事件类型选择正确的结构体成员
        if (event->type == SDL_MOUSEBUTTONDOWN || event->type == SDL_MOUSEBUTTONUP) {
            mouse_x = event->button.x;
            mouse_y = event->button.y;
        } else { // SDL_MOUSEMOTION
            mouse_x = event->motion.x;
            mouse_y = event->motion.y;
        }

        SDL_Point mouse_pos = { mouse_x, mouse_y };
        int event_state;
        if (event->type == SDL_MOUSEBUTTONDOWN) {
            event_state = SDL_PRESSED;
        } else if (event->type == SDL_MOUSEBUTTONUP) {
            event_state = SDL_RELEASED;
        } else if (event->type == SDL_MOUSEMOTION) {
            event_state = SDL_MOUSEMOTION;
        } else {
            event_state = 0;
        }

        MouseEvent mouse_event = {
            .x = mouse_x,
            .y = mouse_y,
            .button = event->button.button,
            .state = event_state
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
        int mouse_x = event->motion.x;
        int mouse_y = event->motion.y;
        SDL_GetMouseState(&mouse_x, &mouse_y);
        SDL_Point mouse_pos = { mouse_x, mouse_y };
        if (SDL_PointInRect(&mouse_pos, &root->rect)) {
            // printf("鼠标滚轮事件在图层内: %d %d, %d\n",root->type, event->wheel.x, event->wheel.y);
            handle_scroll_event(root,mouse_x,mouse_y, event->wheel.x,-event->wheel.y);
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

// 毛玻璃效果缓存管理函数声明
void init_blur_cache();
void cleanup_blur_cache();
int find_matching_cache_entry(Rect* rect, int blur_radius, float saturation, float brightness);
int find_available_cache_entry();

void backend_quit(){
      // 清理毛玻璃缓存
      cleanup_blur_cache();
      
      // 清理圆角纹理缓存
      cleanup_corner_texture_cache();
      
      // 清理字体缓存
      cleanup_font_cache();
      
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

// 打印所有图层信息的辅助函数
void print_layer_info(Layer* layer, int depth) {
    for (int i = 0; i < depth; i++) printf("  ");
    printf("Layer '%s': type=%d, scrollable=%d, scrollbar_v=%p, scrollbar_h=%p\n", 
           layer->id, layer->type, layer->scrollable, (void*)layer->scrollbar_v, (void*)layer->scrollbar_h);
    
    for (int i = 0; i < layer->child_count; i++) {
        if (layer->children[i]) {
            print_layer_info(layer->children[i], depth + 1);
        }
    }
}

void backend_run(Layer* ui_root){
     // 打印所有图层信息
    printf("=== Layer Information ===\n");
    print_layer_info(ui_root, 0);
    printf("========================\n");
    
    // 主循环
    SDL_Event event;
    int running = 1;
    while (running) {

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = 0;
            handle_event(ui_root, &event);
        }

        // 调用所有注册的更新回调
        for (int i = 0; i < update_callback_count; i++) {
            if (update_callbacks[i]) {
                update_callbacks[i]();
            }
        }

        SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
        SDL_RenderClear(renderer);

        render_layer(ui_root);  // 执行渲染管线

        // 渲染弹出层
        popup_manager_render();

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }
}

DFont* backend_load_font(char* font_path,int size){
    return backend_load_font_with_weight(font_path, size, "normal");
}

DFont* backend_load_font_with_weight(char* font_path,int size,const char* weight){
    // 初始化字体缓存
    if (!font_cache_initialized) {
        init_font_cache();
    }
    
    // 先在缓存中查找字体
    TTF_Font* cached_font = find_font_in_cache(font_path, size, weight);
    if (cached_font) {
        return cached_font;
    }
    
    char full_path[MAX_PATH];
    TTF_Font* default_font = NULL;
    
    // 根据字体粗细选择字体文件
    if (strcmp(weight, "bold") == 0) {
        // 尝试加载粗体字体
        snprintf(full_path, sizeof(full_path), "%s", font_path);
        // 如果路径中没有包含Bold，尝试添加Bold后缀
        if (strstr(font_path, "Bold") == NULL && strstr(font_path, "bold") == NULL) {
            char* ext = strrchr(font_path, '.');
            if (ext) {
                int base_len = ext - font_path;
                snprintf(full_path, sizeof(full_path), "%.*s-Bold%s", base_len, font_path, ext);
            }
        }
        default_font = TTF_OpenFont(full_path, size*scale);
        
        // 如果粗体字体不存在，尝试其他粗体字体
        if (!default_font) {
            default_font = TTF_OpenFont("assets/Roboto-Bold.ttf", size*scale);
        }
        if (!default_font) {
            default_font = TTF_OpenFont("app/assets/Roboto-Bold.ttf", size*scale);
        }
        if (!default_font) {
            // 如果找不到粗体字体文件，使用normal字体并设置样式
            default_font = TTF_OpenFont(font_path, size*scale);
            if (default_font) {
                TTF_SetFontStyle(default_font, TTF_STYLE_BOLD);
            }
        }
    } else if (strcmp(weight, "light") == 0) {
        // 尝试加载细体字体
        snprintf(full_path, sizeof(full_path), "%s", font_path);
        if (strstr(font_path, "Light") == NULL && strstr(font_path, "light") == NULL) {
            char* ext = strrchr(font_path, '.');
            if (ext) {
                int base_len = ext - font_path;
                snprintf(full_path, sizeof(full_path), "%.*s-Light%s", base_len, font_path, ext);
            }
        }
        default_font = TTF_OpenFont(full_path, size*scale);
        
        if (!default_font) {
            default_font = TTF_OpenFont("assets/Roboto-Light.ttf", size*scale);
        }
        if (!default_font) {
            default_font = TTF_OpenFont("app/assets/Roboto-Light.ttf", size*scale);
        }
        if (!default_font) {
            default_font = TTF_OpenFont(font_path, size*scale);
            if (default_font) {
                TTF_SetFontStyle(default_font, TTF_STYLE_NORMAL);
            }
        }
    } else {
        // normal或其他情况，使用普通字体
        default_font = TTF_OpenFont(font_path,size*scale);
    }
    
    // 如果指定的字体加载失败，尝试备用字体
    if (!default_font) {
        printf("Warning: Could not load font '%s', trying fallback fonts\n", font_path);
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
    
    if (default_font) {
        TTF_SetFontHinting(default_font, TTF_HINTING_LIGHT); 
        TTF_SetFontOutline(default_font, 0); // 无轮廓
        
        // 将成功加载的字体添加到缓存
        add_font_to_cache(font_path, size, weight, default_font);
    }

    return default_font;
}

void backend_get_windowsize(int* width,int * height){
    SDL_GetWindowSize(window, width, height);
}

void backend_set_windowsize(int width,int  height){
    SDL_SetWindowSize(window, width, height);
}

void backend_set_window_size(char* title){
    SDL_SetWindowTitle(window,title);
}



// 检查文件是否为SVG格式
bool is_svg_file(const char* path) {
    const char* ext = strrchr(path, '.');
    if (ext && strlen(ext) > 4) {
        // 检查文件扩展名是否为.svg或.SVG
        return (strcmp(ext, ".svg") == 0 || strcmp(ext, ".SVG") == 0);
    }
    return false;
}

Texture* backend_load_texture(char* path){
    SDL_Surface* surface = NULL;
    SDL_Texture* texture=NULL;
    
    // 检查是否为SVG文件
    if (is_svg_file(path)) {
        // SDL_image 2.6.0及以上版本原生支持SVG
        surface = IMG_Load(path);
        
        if (!surface) {
            printf("Failed to load SVG image %s: %s\n", path, IMG_GetError());
            printf("Note: SVG support requires SDL_image 2.6.0 or newer.\n");
            
            // 创建一个占位符表面，用于显示SVG加载失败的提示
            surface = SDL_CreateRGBSurface(0, 100, 100, 32, 0, 0, 0, 0);
            if (surface) {
                // 填充为灰色背景
                SDL_FillRect(surface, NULL, SDL_MapRGB(surface->format, 200, 200, 200));
            }
        }
    } else {
        // 非SVG文件，使用常规加载方式
        surface = IMG_Load(path);
    }
    
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
    if (!font) {
        printf("error: backend_render_texture called with NULL font (text: '%s')\n", text ? text : "(null)");
        return NULL;
    }
    if(strlen(text)==0){
        return NULL;
    }
    
    // 检查字体指针是否被破坏
    if ((uintptr_t)font == 0xbebebebebebebebeULL) {
        printf("error: corrupted font pointer in backend_render_texture (text: '%s')\n", text ? text : "(null)");
        return NULL;
    }
    
     SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text, color);
    if (!surface) {
        printf("error: TTF_RenderUTF8_Blended failed for text '%s': %s\n", text, TTF_GetError());
        return NULL;
    }
    
    
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
    // 转换Color到SDL_Color
    SDL_Color sdl_bg_color = {bg_color.r, bg_color.g, bg_color.b, bg_color.a};
    SDL_Color sdl_border_color = {border_color.r, border_color.g, border_color.b, border_color.a};
    draw_rounded_rect_with_border(renderer, rect->x, rect->y, rect->w, rect->h, radius, border_width, sdl_bg_color, sdl_border_color);
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
// 改进的圆角纹理缓存结构
typedef struct {
    SDL_Texture* texture;
    int radius;
    Uint32 color_key; // RGBA的组合作为缓存键
} CornerCacheEntry;

static CornerCacheEntry corner_texture_cache[64] = {0};

// 生成颜色缓存键
static Uint32 generate_color_key(SDL_Color color) {
    return ((Uint32)color.r << 24) | ((Uint32)color.g << 16) | ((Uint32)color.b << 8) | color.a;
}

// 获取或创建圆角纹理
static SDL_Texture* get_corner_texture(SDL_Renderer* renderer, int radius, SDL_Color color) {
    if (radius <= 0 || radius >= 64) return NULL;
    
    Uint32 color_key = generate_color_key(color);
    
    // 遍历所有缓存条目，查找匹配的半径和颜色
    for (int i = 0; i < 64; i++) {
        if (corner_texture_cache[i].texture && 
            corner_texture_cache[i].radius == radius && 
            corner_texture_cache[i].color_key == color_key) {
            return corner_texture_cache[i].texture;
        }
    }
    
    // 创建新的圆角纹理 - 使用RGBA格式
    int size = radius + 2;
    SDL_Surface* surface = SDL_CreateRGBSurface(0, size, size, 32, 0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF);
    if (!surface) return NULL;
    
    // 绘制抗锯齿圆角到surface
    Uint32* pixels = (Uint32*)surface->pixels;
    int pitch = surface->pitch / 4;
    
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            // 计算到圆心的距离（这是左上角，圆心在radius, radius）
            float dx = x - radius;
            float dy = y - radius;
            float distance = sqrt(dx * dx + dy * dy);
            
            float alpha = 0.0f;
            if (distance <= radius) {
                alpha = 1.0f; // 完全在圆内
            } else if (distance <= radius + 1.0f) {
                // 边缘抗锯齿，1像素的渐变区域
                float t = distance - radius;
                alpha = 1.0f - t; // 线性衰减
            }
            
            if (alpha > 0.01f) {
                Uint8 a = (Uint8)(alpha * color.a);
                // SDL像素格式: 0xRRGGBBAA (RGBA格式，与Surface匹配)
                pixels[y * pitch + x] = (color.r << 24) | (color.g << 16) | (color.b << 8) | a;
            } else {
                pixels[y * pitch + x] = 0; // 完全透明
            }
        }
    }
    
    // 创建纹理
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    
    if (!texture) return NULL;
    
    // 查找空闲位置或最久未使用的位置
    int cache_index = -1;
    int duplicate_found = 0;
    
    // 首先检查是否有重复的条目
    for (int i = 0; i < 64; i++) {
        if (corner_texture_cache[i].texture && 
            corner_texture_cache[i].radius == radius && 
            corner_texture_cache[i].color_key == color_key) {
            // 发现重复，使用这个条目
            SDL_DestroyTexture(texture); // 销毁新创建的纹理
            return corner_texture_cache[i].texture;
        }
    }
    
    // 查找空闲位置
    for (int i = 0; i < 64; i++) {
        if (!corner_texture_cache[i].texture) {
            cache_index = i;
            break;
        }
    }
    
    // 如果没有空闲位置，使用轮转方式覆盖
    if (cache_index == -1) {
        static int round_robin = 0;
        cache_index = round_robin;
        round_robin = (round_robin + 1) % 64;
        if (corner_texture_cache[cache_index].texture) {
            SDL_DestroyTexture(corner_texture_cache[cache_index].texture);
        }
    }
    
    corner_texture_cache[cache_index].texture = texture;
    corner_texture_cache[cache_index].radius = radius;
    corner_texture_cache[cache_index].color_key = color_key;
    
    return texture;
}

// 清理圆角纹理缓存
void cleanup_corner_texture_cache() {
    for (int i = 0; i < 64; i++) {
        if (corner_texture_cache[i].texture) {
            SDL_DestroyTexture(corner_texture_cache[i].texture);
            corner_texture_cache[i].texture = NULL;
        }
        corner_texture_cache[i].radius = 0;
        corner_texture_cache[i].color_key = 0;
    }
}

void draw_rounded_rect(SDL_Renderer* renderer, int x, int y, int w, int h, int radius, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    
    // 限制圆角半径不超过宽高的一半
    int r = radius;
    if (r > w / 2) r = w / 2;
    if (r > h / 2) r = h / 2;
    
    // 如果半径很小，直接绘制矩形
    if (r <= 2) {
        SDL_Rect rect = {x, y, w, h};
        SDL_RenderFillRect(renderer, &rect);
        return;
    }
    
    // 绘制中间矩形部分（不包括圆角区域）
    if (w > 2 * r && h > 0) {
        SDL_Rect middle_rect = {x + r, y, w - 2 * r, h};
        SDL_RenderFillRect(renderer, &middle_rect);
    }
    
    // 绘制左右两个矩形部分（避开圆角区域）
    if (h > 2 * r) {
        SDL_Rect left_rect = {x, y + r, r, h - 2 * r};
        SDL_Rect right_rect = {x + w - r, y + r, r, h - 2 * r};
        SDL_RenderFillRect(renderer, &left_rect);
        SDL_RenderFillRect(renderer, &right_rect);
    }
    
    // 使用缓存的抗锯齿纹理绘制圆角
    SDL_Texture* corner_texture = get_corner_texture(renderer, r, color);
    if (corner_texture) {
        // 设置纹理混合模式
        SDL_SetTextureBlendMode(corner_texture, SDL_BLENDMODE_BLEND);
        
        // 圆角纹理大小
        int corner_size = r + 2;
        SDL_Rect src_rect = {0, 0, corner_size, corner_size};
        
        // 左上角 - 只绘制圆角扇形区域
        SDL_Rect dst_tl = {x, y, corner_size, corner_size};
        SDL_RenderCopy(renderer, corner_texture, &src_rect, &dst_tl);
        
        // 右上角 - 水平翻转
        SDL_Rect dst_tr = {x + w - corner_size, y, corner_size, corner_size};
        SDL_RenderCopyEx(renderer, corner_texture, &src_rect, &dst_tr, 0, NULL, SDL_FLIP_HORIZONTAL);
        
        // 左下角 - 垂直翻转
        SDL_Rect dst_bl = {x, y + h - corner_size, corner_size, corner_size};
        SDL_RenderCopyEx(renderer, corner_texture, &src_rect, &dst_bl, 0, NULL, SDL_FLIP_VERTICAL);
        
        // 右下角 - 水平和垂直翻转
        SDL_Rect dst_br = {x + w - corner_size, y + h - corner_size, corner_size, corner_size};
        SDL_RenderCopyEx(renderer, corner_texture, &src_rect, &dst_br, 0, NULL, SDL_FLIP_HORIZONTAL | SDL_FLIP_VERTICAL);
    }
}

// 绘制带边框的圆角矩形
void draw_rounded_rect_with_border(SDL_Renderer* renderer, int x, int y, int w, int h, int radius, int border_width, SDL_Color bg_color, SDL_Color border_color) {
    // 如果边框宽度为0，只绘制填充
    if (border_width <= 0) {
        draw_rounded_rect(renderer, x, y, w, h, radius, bg_color);
        return;
    }
    
    // 限制圆角半径不超过宽高的一半
    int r = radius;
    if (r > w / 2) r = w / 2;
    if (r > h / 2) r = h / 2;
    
    // 先绘制边框（外层）
    draw_rounded_rect(renderer, x, y, w, h, r, border_color);
    
    // 然后绘制内部填充，这样背景色和圆角都是正确的
    int inner_x = x + border_width;
    int inner_y = y + border_width;
    int inner_w = w - 2 * border_width;
    int inner_h = h - 2 * border_width;
    int inner_r = r - border_width;
    if (inner_r < 0) inner_r = 0;
    
    if (inner_w > 0 && inner_h > 0) {
        draw_rounded_rect(renderer, inner_x, inner_y, inner_w, inner_h, inner_r, bg_color);
    }
}

// Add this function anywhere in backend_sdl.c after the global renderer declaration

// 绘制抗锯齿线段的辅助函数
static void plot_aa_pixel(int x, int y, float alpha, Color color) {
    if (alpha <= 0.0f) return;
    Uint8 a = (Uint8)(alpha * color.a);
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, a);
    SDL_RenderDrawPoint(renderer, x, y);
}

// 绘制抗锯齿线段
void backend_render_line(int x1, int y1, int x2, int y2, Color color) {
    // 对于水平或垂直线，使用SDL原生绘制（更快）
    if (x1 == x2 || y1 == y2) {
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
        SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
        return;
    }
    
    // 对于斜线，使用简化的抗锯齿算法
    // 使用Bresenham算法的改进版本
    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy;
    
    int x = x1, y = y1;
    
    while (1) {
        // 绘制主像素
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
        SDL_RenderDrawPoint(renderer, x, y);
        
        // 计算误差来决定是否绘制周围的像素进行抗锯齿
        int err2 = 2 * err;
        float alpha_factor = 0.3f; // 抗锯齿强度
        
        if (err2 > -dy) {
            // 绘制旁边像素进行抗锯齿
            if (y + sy >= 0) {
                plot_aa_pixel(x, y + sy, alpha_factor, color);
            }
            err -= dy;
            x += sx;
        }
        
        if (err2 < dx) {
            // 绘制旁边像素进行抗锯齿
            if (x + sx >= 0) {
                plot_aa_pixel(x + sx, y, alpha_factor, color);
            }
            err += dx;
            y += sy;
        }
        
        if (x == x2 && y == y2) break;
    }
}

// 毛玻璃效果缓存管理函数
void init_blur_cache() {
    if (blur_cache_initialized) return;
    
    for (int i = 0; i < MAX_BLUR_CACHE_ENTRIES; i++) {
        blur_cache[i].texture = NULL;
        blur_cache[i].in_use = 0;
        blur_cache[i].last_used = 0;
    }
    
    blur_cache_initialized = 1;
}

// 清理毛玻璃缓存
void cleanup_blur_cache() {
    printf("Cleaning up blur cache...\n");
    for (int i = 0; i < MAX_BLUR_CACHE_ENTRIES; i++) {
        if (blur_cache[i].texture) {
            printf("Destroying texture at cache index %d\n", i);
            SDL_DestroyTexture(blur_cache[i].texture);
            blur_cache[i].texture = NULL;
        }
        blur_cache[i].in_use = 0;
    }
    printf("Blur cache cleanup completed\n");
}

// 查找匹配的缓存条目
int find_matching_cache_entry(Rect* rect, int blur_radius, float saturation, float brightness) {
    printf("Searching for matching cache entry...\n");
    for (int i = 0; i < MAX_BLUR_CACHE_ENTRIES; i++) {
        if (blur_cache[i].in_use &&
            blur_cache[i].x == rect->x &&
            blur_cache[i].y == rect->y &&
            blur_cache[i].w == rect->w &&
            blur_cache[i].h == rect->h &&
            blur_cache[i].blur_radius == blur_radius &&
            blur_cache[i].saturation == saturation &&
            blur_cache[i].brightness == brightness) {
            printf("Found matching cache entry at index %d\n", i);
            return i;
        }
    }
    printf("No matching cache entry found\n");
    return -1;
}

// 查找可用的缓存条目
int find_available_cache_entry() {
    // 首先查找未使用的条目
    for (int i = 0; i < MAX_BLUR_CACHE_ENTRIES; i++) {
        if (!blur_cache[i].in_use) {
            return i;
        }
    }
    
    // 如果没有未使用的，查找最久未使用的条目
    Uint32 oldest_time = SDL_GetTicks();
    int oldest_index = 0;
    
    for (int i = 0; i < MAX_BLUR_CACHE_ENTRIES; i++) {
        if (blur_cache[i].last_used < oldest_time) {
            oldest_time = blur_cache[i].last_used;
            oldest_index = i;
        }
    }
    
    // 清理最旧的条目
    if (blur_cache[oldest_index].texture) {
        SDL_DestroyTexture(blur_cache[oldest_index].texture);
        blur_cache[oldest_index].texture = NULL;
    }
    
    return oldest_index;
}

// 实现优化的毛玻璃效果（带缓存）
void backend_render_backdrop_filter(Rect* rect, int blur_radius, float saturation, float brightness) {
    if (!renderer || !rect || blur_radius <= 0) {
        return;
    }
    
    // 初始化缓存（如果尚未初始化）
    if (!blur_cache_initialized) {
        init_blur_cache();
    }
    
    // 查找匹配的缓存条目
    int cache_index = find_matching_cache_entry(rect, blur_radius, saturation, brightness);
    
    // 如果找到匹配的缓存，直接使用
    if (cache_index >= 0) {
        SDL_RenderCopy(renderer, blur_cache[cache_index].texture, NULL, rect);
        blur_cache[cache_index].last_used = SDL_GetTicks();
        return;
    }
    
    // 获取当前渲染目标
    SDL_Texture* current_target = SDL_GetRenderTarget(renderer);
    
    // 限制模糊半径以提高性能
    if (blur_radius > 20) blur_radius = 20;
    
    // 创建一个临时纹理来捕获当前屏幕内容
    SDL_Texture* temp_texture = SDL_CreateTexture(
        renderer, 
        SDL_PIXELFORMAT_RGBA8888, 
        SDL_TEXTUREACCESS_TARGET, 
        rect->w, 
        rect->h
    );
    
    if (!temp_texture) {
        return;
    }
    
    // 设置临时纹理为渲染目标
    if (SDL_SetRenderTarget(renderer, temp_texture) != 0) {
        SDL_DestroyTexture(temp_texture);
        return;
    }
    
    // 将当前屏幕内容复制到临时纹理
    SDL_Rect src_rect = *rect;
    SDL_Rect dst_rect = {0, 0, rect->w, rect->h};
    
    // 将屏幕内容渲染到临时纹理
    SDL_RenderCopy(renderer, current_target, &src_rect, &dst_rect);
    
    // 恢复原始渲染目标
    SDL_SetRenderTarget(renderer, current_target);
    
    // 创建模糊纹理（只创建一次）
    SDL_Texture* blur_texture = SDL_CreateTexture(
        renderer, 
        SDL_PIXELFORMAT_RGBA8888, 
        SDL_TEXTUREACCESS_TARGET, 
        rect->w, 
        rect->h
    );
    
    if (!blur_texture) {
        SDL_DestroyTexture(temp_texture);
        return;
    }
    
    // 设置模糊纹理为渲染目标
    SDL_SetRenderTarget(renderer, blur_texture);
    SDL_SetTextureBlendMode(blur_texture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureBlendMode(temp_texture, SDL_BLENDMODE_BLEND);
    
    // 优化的模糊算法 - 使用更少的采样点
    SDL_SetTextureAlphaMod(temp_texture, 200); // 增加不透明度以减少模糊次数
    
    // 使用优化的采样模式 - 减少采样点数量
    int sample_step = (blur_radius > 10) ? 3 : 2; // 根据模糊半径调整采样步长
    
    for (int x = -blur_radius; x <= blur_radius; x += sample_step) {
        for (int y = -blur_radius; y <= blur_radius; y += sample_step) {
            // 使用圆形采样区域
            if (x * x + y * y <= blur_radius * blur_radius) {
                SDL_Rect offset_rect = {x, y, rect->w, rect->h};
                SDL_RenderCopy(renderer, temp_texture, NULL, &offset_rect);
            }
        }
    }
    
    // 恢复原始渲染目标
    SDL_SetRenderTarget(renderer, current_target);
    
    // 应用饱和度和亮度调整（如果需要）
    if (saturation != 1.0f || brightness != 1.0f) {
        // 应用颜色调整
        SDL_SetTextureColorMod(blur_texture, 
            (Uint8)(255 * brightness), 
            (Uint8)(255 * brightness), 
            (Uint8)(255 * brightness)
        );
    }
    
    // 将最终结果渲染到屏幕
    SDL_RenderCopy(renderer, blur_texture, NULL, rect);
    
    // 查找可用的缓存条目并保存结果
    cache_index = find_available_cache_entry();
    if (cache_index >= 0) {
        // 如果已有纹理，先销毁
        if (blur_cache[cache_index].texture) {
            SDL_DestroyTexture(blur_cache[cache_index].texture);
        }
        
        // 创建一个新的纹理作为缓存副本
        blur_cache[cache_index].texture = SDL_CreateTexture(
            renderer, 
            SDL_PIXELFORMAT_RGBA8888, 
            SDL_TEXTUREACCESS_TARGET, 
            rect->w, 
            rect->h
        );
        
        if (blur_cache[cache_index].texture) {
            // 将模糊纹理复制到缓存纹理
            SDL_SetRenderTarget(renderer, blur_cache[cache_index].texture);
            SDL_RenderCopy(renderer, blur_texture, NULL, NULL);
            SDL_SetRenderTarget(renderer, current_target);
            
            // 更新缓存条目信息
            blur_cache[cache_index].x = rect->x;
            blur_cache[cache_index].y = rect->y;
            blur_cache[cache_index].w = rect->w;
            blur_cache[cache_index].h = rect->h;
            blur_cache[cache_index].blur_radius = blur_radius;
            blur_cache[cache_index].saturation = saturation;
            blur_cache[cache_index].brightness = brightness;
            blur_cache[cache_index].last_used = SDL_GetTicks();
            blur_cache[cache_index].in_use = 1;
        }
    }
    
    // 清理纹理资源
    SDL_DestroyTexture(temp_texture);
    SDL_DestroyTexture(blur_texture);
}

// ====================== 主循环回调管理 ======================

// 注册主循环更新回调
void backend_register_update_callback(UpdateCallback callback) {
    if (!callback) return;

    if (update_callback_count >= MAX_UPDATE_CALLBACKS) {
        printf("Warning: Update callback limit reached\n");
        return;
    }

    update_callbacks[update_callback_count++] = callback;
}

// 获取剪贴板文本
char* backend_get_clipboard_text() {
    char* clipboard_text = SDL_GetClipboardText();
    if (!clipboard_text) {
        printf("Failed to get clipboard text: %s\n", SDL_GetError());
        return NULL;
    }
    
    // 分配内存并复制剪贴板内容
    size_t len = strlen(clipboard_text);
    char* result = (char*)malloc(len + 1);
    if (!result) {
        printf("Failed to allocate memory for clipboard text\n");
        SDL_free(clipboard_text);
        return NULL;
    }
    
    strcpy(result, clipboard_text);
    SDL_free(clipboard_text);
    
    return result;
}