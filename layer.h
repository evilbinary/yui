#ifndef YUI_LAYER_H
#define YUI_LAYER_H

#ifdef D_SDL
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#else
#include <SDL.h>
#include <SDL_ttf.h>
#include <SDL_image.h>
#endif

#include "event.h"
#include "cJSON.h"

#define MAX_PATH 1024
#define MAX_TEXT 256


// ====================== 图层数据结构 ======================
typedef enum {
    LAYOUT_ABSOLUTE,
    LAYOUT_HORIZONTAL,
    LAYOUT_VERTICAL,
    LAYOUT_CENTER,
    LAYOUT_LEFT,
    LAYOUT_RIGHT,
    LAYOUT_ALIGN_CENTER,
    LAYOUT_ALIGN_LEFT,
    LAYOUT_ALIGN_RIGHT
} LayoutType;

// 添加图片渲染模式枚举
typedef enum {
    IMAGE_MODE_STRETCH,     // 拉伸填充整个区域
    IMAGE_MODE_ASPECT_FIT,  // 自适应完整显示图片（可能有空白）
    IMAGE_MODE_ASPECT_FILL  // 填充整个区域（可能裁剪图片）
} ImageMode;

typedef struct LayoutManager {
    LayoutType type;
    int spacing;
    int padding[4]; // top, right, bottom, left
    LayoutType align;
} LayoutManager;

// 添加数据绑定结构
typedef struct Binding {
    char path[MAX_PATH];
} Binding;

typedef struct Data {
    int size;
    cJSON* json;
} Data;

typedef struct Font {
    char path[MAX_PATH];
    int size;
    TTF_Font* default_font;  // 添加默认字体
} Font;

typedef struct Assets {
    char path[MAX_PATH];
    int size;
} Assets;

typedef struct Event {
    char click_name[MAX_PATH];
    void (*click)();  // 事件回调函数指针
    void (*press)();
} Event;

typedef enum {
    VIEW,
    BUTTON,
    INPUT,
    LABEL,
    IMAGE,
    LIST
} LayerType;

#define LAYER_TYPE_SIZE 6
extern char *layer_type_name[];

typedef struct Layer Layer;
typedef struct Layer {
    char id[50];
    LayerType type;
    SDL_Rect rect;
    SDL_Color color;
    SDL_Color bgColor;
    char source[MAX_PATH];
    SDL_Texture* texture;
    Layer** children;
    int child_count;
    
    // 新增布局管理器
    LayoutManager* layout_manager;
    int fixed_width;
    int fixed_height;
    float flex_ratio;
    
    // 新增label和text字段
    char label[MAX_TEXT];
    char text[MAX_TEXT];
    
    // 添加图片模式字段
    ImageMode image_mode;
    
    // 添加数据绑定和模板字段
    Binding* binding;
    Layer* item_template;
    //数据
    Data* data;

    //资源文件路径
    Font* font;
    Assets* assets;

    Layer* sub;
    Layer* parent;

    //事件
    Event* event;
} Layer;



cJSON* parse_json(char* json_path);
Layer* parse_layer(cJSON* json_obj,Layer* parent);


#endif

