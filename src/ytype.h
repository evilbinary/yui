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
#define YUI_CHECKBOX_COMPONENT 1
#define YUI_RADIOBOX_COMPONENT 1
#define YUI_TEXT_COMPONENT 1
#define YUI_TREEVIEW_COMPONENT 1
#define YUI_TAB_COMPONENT 1
#define YUI_SLIDER_COMPONENT 1
#define YUI_SELECT_COMPONENT 1
#define YUI_SCROLLBAR_COMPONENT 1
#define YUI_MENU_COMPONENT 1
#define YUI_DIALOG_COMPONENT 1
#define YUI_CLOCK_COMPONENT 1


#ifdef YUI_ANIMATION
#include "animate.h"
typedef struct Animation Animation;
#else 
typedef struct void* Animation;
#endif


typedef struct Point {
    int x;
    int y;
} Point;


#if SDL2

#define Texture SDL_Texture
#define Color SDL_Color
#define Rect SDL_Rect
#define  BUTTON_LEFT SDL_BUTTON_LEFT
#define  BUTTON_PRESSED SDL_PRESSED
#define  BUTTON_RIGHT SDL_BUTTON_RIGHT

#else

#define  BUTTON_LEFT 1
#define  BUTTON_PRESSED 2
#define  BUTTON_RIGHT 3
typedef struct Rect {
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

#define point_in_rect(pt, rect) \
    ((pt.x) >= (rect).x && (pt.x) <= (rect).x + (rect).w) && \
    ((pt.y) >= (rect).y && (pt.y) <= (rect).y + (rect).h)
        
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
    LAYOUT_ALIGN_RIGHT,
    LAYOUT_ALIGN_SPACE_BETWEEN,  // space-between: 两端对齐，子组件之间间距相等
    LAYOUT_ALIGN_SPACE_AROUND    // space-around: 每个子组件两侧间距相等
} LayoutType;

// 添加图片渲染模式枚举
typedef enum {
    IMAGE_MODE_STRETCH,     // 拉伸填充整个区域
    IMAGE_MODE_ASPECT_FIT,  // 自适应完整显示图片（可能有空白）
    IMAGE_MODE_ASPECT_FILL  // 填充整个区域（可能裁剪图片）
} ImageMode;

typedef enum {
    VISIBLE,
    IN_VISIBLE,
} VisableType;



// 添加图层类型枚举值 GRID
typedef enum {
    VIEW,
    BUTTON,
    INPUT_FIELD,
    LABEL,
    IMAGE,
    LIST,
    GRID,  // 添加GRID类型
    PROGRESS,
    CHECKBOX,
    RADIOBOX,
    TEXT,  // 添加TEXT类型
    TREEVIEW,
    TAB,
    SLIDER,
    SELECT,
    SCROLLBAR,  // 添加SCROLLBAR类型
    MENU,  // 添加MENU类型
    DIALOG,  // 添加DIALOG类型
    CLOCK  // 添加CLOCK类型
} LayerType;

 
extern char* layer_type_name[];
extern int layer_type_size ;


typedef struct Layer Layer;

typedef struct LayoutManager {
    LayoutType type;
    int spacing;
    int padding[4]; // top, right, bottom, left
    LayoutType align;
    LayoutType justify;  // 主轴对齐方式（水平布局中的水平对齐，垂直布局中的垂直对齐）
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
    char weight[20];      // 字体粗细：normal, bold, light, etc.
    char path[MAX_PATH];
    int size;
    DFont* default_font;  // 添加默认字体
} Font;

typedef struct Assets {
    char path[MAX_PATH];
    int size;
} Assets;
// 滚动条方向枚举
typedef enum {
    SCROLLBAR_DIRECTION_VERTICAL,   // 垂直滚动条
    SCROLLBAR_DIRECTION_HORIZONTAL  // 水平滚动条
} ScrollbarDirection;

// 添加滚动条结构
typedef struct Scrollbar {
    int visible;
    int thickness;
    Color color;
    int is_dragging;  // 滚动条是否被拖动
    int drag_offset;  // 拖动时鼠标相对于滚动条顶部的偏移
    ScrollbarDirection direction;  // 滚动条方向
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

#define MAX_EVENT 512

// 定义事件处理函数类型
typedef void (*EventHandler)(void* data);


typedef struct {
    char name[50];          // 事件名称
    EventHandler handler;   // 事件处理函数
} EventEntry;


typedef struct Event {
    char click_name[MAX_PATH];
    void (*click)(Layer*);  // 事件回调函数指针
    void (*press)(Layer*);
    // 添加滚动事件回调函数指针
    void (*scroll)(Layer*)    ; 
    char scroll_name[MAX_PATH];
    
    // 合并的触屏事件
    char touch_name[MAX_PATH];
    void (*touch)(TouchEvent* event);  // 统一的触屏事件处理函数
} Event;

// Animation结构体在animate.h中定义

// 图层状态枚举 - 使用位标志以便状态可以共存
typedef enum {
    LAYER_STATE_NORMAL   = 0,
    LAYER_STATE_FOCUSED  = 1 << 0,  // 1
    LAYER_STATE_DISABLED = 1 << 1,  // 2
    LAYER_STATE_HOVER    = 1 << 2,  // 4
    LAYER_STATE_PRESSED  = 1 << 3,  // 8
    LAYER_STATE_ACTIVE   = 1 << 4,  // 16 - 用于表示选中、激活状态
} LayerState;




// 辅助宏：检查状态
// HAS_STATE - 检查layer->state中是否包含st指定的状态位
// 逻辑：如果(st)与(layer->state)的按位与结果不为0，则表示st状态位被设置
#define HAS_STATE(layer, st) ((layer->state & (st)) != 0)
#define SET_STATE(layer, st) (layer->state |= (st))
#define CLEAR_STATE(layer, st) (layer->state &= ~(st))
#define CLEAR_ALL_STATES(layer) (layer->state = LAYER_STATE_NORMAL)

typedef  int (*register_event_fun_t)(Layer* layer, const char* event_name, const char* event_func_name, EventHandler event_handler);
typedef  cJSON* (*get_property_fun_t)(Layer* layer, const char* property_name);

typedef struct Layer {
    char id[50];
    int index;
    LayerType type;
    Rect rect;
    Color color;
    Color bg_color;
    int radius; // 圆角半径
    char source[MAX_PATH];
    Texture* texture;
    Layer** children;
    int child_count;

    int rotation;
    
    // 图层状态 - 现在支持位运算，可以同时表示多个状态
    unsigned int state;
    // 是否可获得焦点
    int focusable;
    int visible;

    //动画
    Animation* animation;

    // 新增布局管理器
    LayoutManager* layout_manager;
    int fixed_width;
    int fixed_height;
    float flex_ratio;
    int content_height; // 内容高度
    int content_width; // 内容宽度
    
    // 新增label和text字段
    char* label;
    char* text;
    size_t text_size;  // text字段分配的内存大小
    
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
    int scrollable;          // 滚动类型: 0=不可滚动, 1=垂直滚动, 2=水平滚动, 3=双向滚动
    int scroll_offset;       // 垂直滚动偏移
    int scroll_offset_x;     // 水平滚动偏移
    Scrollbar* scrollbar;    // 旧的滚动条指针(为了兼容性保留)
    Scrollbar* scrollbar_v;  // 垂直滚动条
    Scrollbar* scrollbar_h;  // 水平滚动条
    
    // 组件指针
    void* component;
    
    // 自定义渲染函数指针
    void (*render)(Layer* layer);

    // 布局更新函数指针
    void (*layout)(Layer* layer);

    // 新增事件处理函数指针
    void (*handle_key_event)(Layer* layer, KeyEvent* event);
    void (*handle_mouse_event)(Layer* layer, MouseEvent* event);
    void (*handle_scroll_event)(Layer* layer, int scroll_delta);
    void (*handle_touch_event)(Layer* layer, TouchEvent* event);

    //事件注册
    register_event_fun_t register_event;
    int (*unregister_event)(Layer* layer, const char* event_name);
    
    //组件属性获取函数
    get_property_fun_t get_property;

    // 毛玻璃效果相关属性
    int backdrop_filter;     // 是否启用毛玻璃效果
    int blur_radius;         // 模糊半径
    float saturation;         // 饱和度 (1.0为正常，>1.0为更饱和，<1.0为不饱和)
    float brightness;        // 亮度 (1.0为正常，>1.0为更亮，<1.0为更暗)
    
    // 增量更新支持：脏标记
    unsigned int dirty_flags; // 标记哪些属性被修改

    // inspect
    int inspect_enabled;     // 是否启用Inspect调试模式
    int inspect_mode;         // Inspect模式
    int inspect_show_bounds; // 是否显示边界
    int inspect_show_info;   // 是否显示信息

} Layer;
// 全局变量：当前拥有焦点的图层
extern Layer* focused_layer;



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

#ifdef YUI_CHECKBOX_COMPONENT
#include "components/checkbox_component.h"
#endif

#ifdef YUI_RADIOBOX_COMPONENT
#include "components/radiobox_component.h"
#endif

#ifdef YUI_TEXT_COMPONENT
#include "components/text_component.h"
#endif


#ifdef YUI_TREEVIEW_COMPONENT
#include "components/treeview_component.h"
#endif

#ifdef YUI_TAB_COMPONENT
#include "components/tab_component.h"
#endif

#ifdef YUI_SLIDER_COMPONENT
#include "components/slider_component.h"
#endif

#ifdef YUI_SELECT_COMPONENT
#include "components/select_component.h"
#endif


#ifdef YUI_SCROLLBAR_COMPONENT
#include "components/scrollbar_component.h"
#endif

#ifdef YUI_MENU_COMPONENT
#include "components/menu_component.h"
#endif

#ifdef YUI_DIALOG_COMPONENT
#include "components/dialog_component.h"
#endif

#ifdef YUI_CLOCK_COMPONENT
#include "components/clock_component.h"
#endif

#endif