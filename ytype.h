#ifndef YUI_TYPE_H
#define YUI_TYPE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"

#ifdef D_SDL
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#else
#include <SDL.h>
#include <SDL_ttf.h>
#include <SDL_image.h>



#endif

#define SDL2 1

#if SDL2

#define Texture SDL_Texture
#define Color SDL_Color
#define Rect SDL_Rect

#else
    typedef struct Rect{
        int x, y;
        int w, h;
    } Rect;

    typedef struct Color{
        unsigned char r;
        unsigned char g;
        unsigned char b;
        unsigned char a;
    } Color;

#endif

#define DFont TTF_Font






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

// 添加图层类型枚举值 GRID
typedef enum {
    VIEW,
    BUTTON,
    INPUT,
    LABEL,
    IMAGE,
    LIST,
    GRID  // 添加GRID类型
} LayerType;

#define LAYER_TYPE_SIZE 7  // 更新图层类型数量
extern char *layer_type_name[];

typedef struct LayoutManager {
    LayoutType type;
    int spacing;
    int padding[4]; // top, right, bottom, left
    LayoutType align;
    // 添加Grid布局特有属性
    int columns;  // Grid布局列数
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
    DFont* default_font;  // 添加默认字体
} Font;

typedef struct Assets {
    char path[MAX_PATH];
    int size;
} Assets;
// 添加滚动条结构
typedef struct Scrollbar {
    int visible;
    int thickness;
    Color color;
} Scrollbar;
typedef struct Event {
    char click_name[MAX_PATH];
    void (*click)();  // 事件回调函数指针
    void (*press)();
    // 添加滚动事件回调函数指针
    void (*scroll)(); 
    char scroll_name[MAX_PATH];
} Event;

typedef struct Layer Layer;
typedef struct Layer {
    char id[50];
    LayerType type;
    Rect rect;
    Color color;
    Color bgColor;
    char source[MAX_PATH];
    Texture* texture;
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
    
    // 添加滚动支持字段
    int scrollable;
    int scroll_offset;
    Scrollbar* scrollbar;
} Layer;






#define MAX_EVENT 512

// 定义事件处理函数类型
typedef void (*EventHandler)(void* data);


typedef struct {
    char name[50];          // 事件名称
    EventHandler handler;   // 事件处理函数
} EventEntry;



#endif