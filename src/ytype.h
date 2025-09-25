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

// 功能定义区域
#define SDL2 1
// #define DEBUG_VIEW 1 
#define YUI_ANIMATION 1
#define YUI_INPUT_COMPONENT 1
#define YUI_LABEL_COMPONENT 1
#define YUI_IMAGE_COMPONENT 1
#define YUI_BUTTON_COMPONENT 1
#define YUI_PROGRESS_COMPONENT 1


#ifdef YUI_ANIMATION
#include "animate.h"
typedef struct Animation Animation;
#else 
typedef struct void* Animation;
#endif

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
    GRID,  // 添加GRID类型
    PROGRESS
} LayerType;

extern char *layer_type_name[];
#define LAYER_TYPE_SIZE (sizeof(layer_type_name) / sizeof(layer_type_name[0]))  // 更新图层类型数量


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

// 触屏事件类型枚举
typedef enum {
    TOUCH_TYPE_START,    // 触摸开始
    TOUCH_TYPE_MOVE,     // 触摸移动
    TOUCH_TYPE_END,      // 触摸结束
    TOUCH_TYPE_SWIPE,    // 滑动
    TOUCH_TYPE_DOUBLE_TAP, // 双击
    TOUCH_TYPE_LONG_PRESS, // 长按
    TOUCH_TYPE_PINCH,    // 捏合
    TOUCH_TYPE_ROTATE    // 旋转
} TouchType;

// 触屏事件参数结构
typedef struct TouchEvent {
    TouchType type;       // 事件类型
    int x;                // x坐标
    int y;                // y坐标
    int deltaX;           // x方向变化量
    int deltaY;           // y方向变化量
    float scale;          // 缩放比例（用于捏合）
    float rotation;       // 旋转角度（用于旋转）
    int fingerCount;      // 手指数量
    Uint32 timestamp;     // 时间戳
} TouchEvent;

// 自定义键盘事件结构体
typedef struct KeyEvent{
    enum {
        KEY_EVENT_DOWN,
        KEY_EVENT_UP,
        KEY_EVENT_TEXT_INPUT
    } type;
    
    union {
        struct {
            int key_code;  // 键码
            int mod;       // 修饰键状态
            int repeat;    // 是否重复按键
        } key;
        
        struct {
            char text[32];  // 输入的文本
        } text;
    } data;
} KeyEvent;

// 事件结构
typedef struct MouseEvent {
    int x;
    int y;
    int button;
    int state;
    Uint32 timestamp;
} MouseEvent;

typedef struct Event {
    char click_name[MAX_PATH];
    void (*click)();  // 事件回调函数指针
    void (*press)();
    // 添加滚动事件回调函数指针
    void (*scroll)(); 
    char scroll_name[MAX_PATH];
    
    // 合并的触屏事件
    char touch_name[MAX_PATH];
    void (*touch)(TouchEvent* event);  // 统一的触屏事件处理函数
} Event;

// Animation结构体在animate.h中定义

typedef struct Layer Layer;
typedef struct Layer {
    char id[50];
    int index;
    LayerType type;
    Rect rect;
    Color color;
    Color bgColor;
    int radius; // 圆角半径
    char source[MAX_PATH];
    Texture* texture;
    Layer** children;
    int child_count;

    int rotation;

    //动画
    Animation* animation;

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
    
    // 组件指针
    void* component;
    
    // 自定义渲染函数指针
    void (*render)(Layer* layer);

    // 新增事件处理函数指针
    void (*handle_key_event)(Layer* layer, KeyEvent* event);
    void (*handle_mouse_event)(Layer* layer, MouseEvent* event);
    void (*handle_touch_event)(Layer* layer, TouchEvent* event);

} Layer;






#define MAX_EVENT 512

// 定义事件处理函数类型
typedef void (*EventHandler)(void* data);


typedef struct {
    char name[50];          // 事件名称
    EventHandler handler;   // 事件处理函数
} EventEntry;

#ifdef YUI_INPUT_COMPONENT
#include "components/input_component.h"
#endif

#ifdef YUI_LABEL_COMPONENT
#include "components/label_component.h"
#endif

#ifdef YUI_IMAGE_COMPONENT
#include "components/image_component.h"
#endif

#ifdef YUI_BUTTON_COMPONENT
#include "components/button_component.h"
#endif

#ifdef YUI_PROGRESS_COMPONENT
#include "components/progress_component.h"
#endif

#endif