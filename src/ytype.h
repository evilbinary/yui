#ifndef YUI_TYPE_H
#define YUI_TYPE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"

#if defined(YUI_BACKEND_MOBILE)

#include <stdint.h>

typedef uint32_t Uint32;
typedef int SDL_EventType;

#ifndef SDL_PRESSED
#define SDL_PRESSED 1
#endif
#ifndef SDL_RELEASED
#define SDL_RELEASED 0
#endif
#ifndef SDL_BUTTON_LEFT
#define SDL_BUTTON_LEFT 1
#endif
#ifndef SDL_BUTTON_RIGHT
#define SDL_BUTTON_RIGHT 3
#endif
#ifndef SDL_MOUSEBUTTONDOWN
#define SDL_MOUSEBUTTONDOWN 1
#endif
#ifndef SDL_MOUSEBUTTONUP
#define SDL_MOUSEBUTTONUP 2
#endif
#ifndef SDL_MOUSEMOTION
#define SDL_MOUSEMOTION 3
#endif

/* SDL2 key/mod stubs — compile shared component code without linking SDL */
#ifndef SDLK_UNKNOWN
#define SDLK_UNKNOWN 0
#endif
#ifndef SDLK_RETURN
#define SDLK_RETURN '\r'
#endif
#ifndef SDLK_ESCAPE
#define SDLK_ESCAPE '\033'
#endif
#ifndef SDLK_BACKSPACE
#define SDLK_BACKSPACE '\b'
#endif
#ifndef SDLK_TAB
#define SDLK_TAB '\t'
#endif
#ifndef SDLK_SPACE
#define SDLK_SPACE ' '
#endif
#ifndef SDLK_DELETE
#define SDLK_DELETE 127
#endif
#ifndef SDLK_a
#define SDLK_a 'a'
#endif
#ifndef SDLK_c
#define SDLK_c 'c'
#endif
#ifndef SDLK_v
#define SDLK_v 'v'
#endif
#ifndef SDLK_x
#define SDLK_x 'x'
#endif
#ifndef SDLK_z
#define SDLK_z 'z'
#endif
#ifndef SDLK_y
#define SDLK_y 'y'
#endif
#ifndef SDLK_b
#define SDLK_b 'b'
#endif
#ifndef SDLK_i
#define SDLK_i 'i'
#endif
#ifndef SDLK_UP
#define SDLK_UP 1073741906
#endif
#ifndef SDLK_DOWN
#define SDLK_DOWN 1073741905
#endif
#ifndef SDLK_RIGHT
#define SDLK_RIGHT 1073741903
#endif
#ifndef SDLK_LEFT
#define SDLK_LEFT 1073741904
#endif
#ifndef SDLK_HOME
#define SDLK_HOME 1073741898
#endif
#ifndef SDLK_END
#define SDLK_END 1073741901
#endif
#ifndef SDLK_KP_ENTER
#define SDLK_KP_ENTER 1073741912
#endif
#ifndef SDLK_F1
#define SDLK_F1 1073741882
#endif
#ifndef SDLK_F2
#define SDLK_F2 1073741883
#endif
#ifndef SDLK_F12
#define SDLK_F12 1073741893
#endif

#ifndef KMOD_NONE
#define KMOD_NONE 0x0000
#endif
#ifndef KMOD_LSHIFT
#define KMOD_LSHIFT 0x0001
#endif
#ifndef KMOD_RSHIFT
#define KMOD_RSHIFT 0x0002
#endif
#ifndef KMOD_LCTRL
#define KMOD_LCTRL 0x0040
#endif
#ifndef KMOD_RCTRL
#define KMOD_RCTRL 0x0080
#endif
#ifndef KMOD_SHIFT
#define KMOD_SHIFT (KMOD_LSHIFT | KMOD_RSHIFT)
#endif
#ifndef KMOD_CTRL
#define KMOD_CTRL (KMOD_LCTRL | KMOD_RCTRL)
#endif
#ifndef KMOD_LALT
#define KMOD_LALT 0x0100
#endif
#ifndef KMOD_RALT
#define KMOD_RALT 0x0200
#endif
#ifndef KMOD_ALT
#define KMOD_ALT (KMOD_LALT | KMOD_RALT)
#endif

typedef struct Rect {
    int x, y;
    int w, h;
} Rect;

typedef struct Color {
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
} Color;

typedef struct YuiTexture {
    int w;
    int h;
    void* priv;
} YuiTexture;

typedef struct YuiFont {
    int size;
    void* priv;
} YuiFont;

typedef YuiTexture Texture;
typedef YuiFont DFont;

#define BUTTON_LEFT SDL_BUTTON_LEFT
#define BUTTON_PRESSED SDL_PRESSED
#define BUTTON_RIGHT SDL_BUTTON_RIGHT

#else /* !YUI_BACKEND_MOBILE */

#ifdef D_SDL
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#else
#include <SDL.h>
#include <SDL_ttf.h>
#include <SDL_image.h>
#endif

#endif /* YUI_BACKEND_MOBILE */

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
#define YUI_LIST_COMPONENT 1
#define YUI_TAB_COMPONENT 1
#define YUI_SLIDER_COMPONENT 1
#define YUI_SELECT_COMPONENT 1
#define YUI_SCROLLBAR_COMPONENT 1
#define YUI_MENU_COMPONENT 1
#define YUI_DIALOG_COMPONENT 1
#define YUI_CLOCK_COMPONENT 1
#define YUI_SASH_COMPONENT 1
#define YUI_TABLE_COMPONENT 1
#define YUI_PAGINATION_COMPONENT 1
#define YUI_LOADING_COMPONENT 1

// 指针输入设备（须在 animate.h 之前：animate→layer→event 依赖 PointerPhase）
typedef enum {
    POINTER_DEVICE_MOUSE = 0,
    POINTER_DEVICE_TOUCH = 1,
} PointerDevice;

typedef enum {
    POINTER_DOWN = 0,
    POINTER_MOVE,
    POINTER_UP,
    POINTER_WHEEL,
    POINTER_SWIPE,
    POINTER_DOUBLE_TAP,
    POINTER_LONG_PRESS,
    POINTER_PINCH,
    POINTER_ROTATE,
    POINTER_CANCEL,
} PointerPhase;

typedef struct PointerEvent {
    PointerDevice device;
    PointerPhase phase;
    int x;
    int y;
    int delta_x;   /* MOVE 拖动 / WHEEL 滚轮 / SWIPE 向量，由 phase 区分 */
    int delta_y;
    int button;  /* device == POINTER_DEVICE_MOUSE 时有效 */
    int pointer_id;   /* 触点 ID（SDL fingerId / Android pointer），mouse 为 0 */
    int finger_count; /* 当前屏幕上的触点数 */
    union {
        struct { float scale; float rotation; } gesture; /* PINCH / ROTATE */
    } ext;
} PointerEvent;

typedef struct Layer Layer;
typedef struct KeyEvent KeyEvent;
typedef struct WindowEvent WindowEvent;

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


#if defined(YUI_BACKEND_MOBILE)
/* Rect / Color / Texture / DFont defined above */
#elif SDL2

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

#if !defined(YUI_BACKEND_MOBILE)

#define DFont TTF_Font

#endif




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
    LAYOUT_GRID,
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
} VisibleType;



// 添加图层类型枚举值 GRID
typedef enum {
    VIEW,
    BUTTON,
    INPUT_FIELD,
    LABEL,
    IMAGE,
    LAYER_LIST,
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
    CLOCK,  // 添加CLOCK类型
    SASH,
    TABLE,
    PAGINATION,
    LOADING,
    CONNECTOR,
    DRAGGABLE,

    LAYER_TYPE_BUILTIN_MAX,
    LAYER_TYPE_USER_BASE = 256,
} LayerType;


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
    Color track_color;
    int is_dragging;  // 滚动条是否被拖动
    int drag_offset;  // 拖动时鼠标相对于滚动条顶部的偏移
    ScrollbarDirection direction;  // 滚动条方向
} Scrollbar;

// 自定义键盘事件结构体
typedef struct KeyEvent{
    enum {
        KEY_EVENT_DOWN,
        KEY_EVENT_UP,
        KEY_EVENT_TEXT_INPUT,
        KEY_EVENT_TEXT_EDITING  // IME 文本编辑事件（候选词）
    } type;
    
    union {
        struct {
            int key_code;  // 键码
            int mod;       // 修饰键状态
            int repeat;    // 是否重复按键
        } key;
        
        struct {
            char text[32];  // 输入的文本
            int start;      // 编辑起始位置（用于IME）
            int length;     // 编辑长度（用于IME）
        } text;
    } data;
} KeyEvent;

typedef struct ResizeEvent {
    int old_width;
    int old_height;
    int new_width;
    int new_height;
    float scale_x;
    float scale_y;
} ResizeEvent;

/* OS 窗口生命周期（与 Layer ResizeEvent 分离） */
typedef enum {
    WINDOW_RESIZED = 0,
    WINDOW_FOCUS_GAINED,
    WINDOW_FOCUS_LOST,
    WINDOW_MINIMIZED,
    WINDOW_RESTORED,
    WINDOW_EXPOSED,
    WINDOW_MOVED
} WindowEventType;

typedef struct WindowEvent {
    WindowEventType type;
    int width;
    int height;
    int x;
    int y;
} WindowEvent;

#define MAX_EVENT 512

// 定义事件处理函数类型
typedef void* (*EventHandler)(void* data);


typedef struct {
    char name[50];          // 事件名称
    EventHandler handler;   // 事件处理函数
} EventEntry;


typedef struct Event {
    char click_name[MAX_PATH];
    EventHandler click;
    EventHandler press;
    // 添加滚动事件回调函数指针
    char scroll_name[MAX_PATH];
    EventHandler scroll;

    // 合并的触屏事件
    char touch_name[MAX_PATH];
    EventHandler touch;

    char resize_name[MAX_PATH];
    void (*resize)(Layer* layer, const ResizeEvent* event);
} Event;

#define EVENT_INVOKE(handler, layer) \
    do { if ((handler)) (handler)((void*)(layer)); } while (0)

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

/* CSS box-shadow: offset-x offset-y blur-radius spread-radius color */
typedef struct LayerShadow {
    unsigned char enabled;
    int offset_x;
    int offset_y;
    int blur;
    int spread;
    Color color;
} LayerShadow;

#define LAYER_GRADIENT_MAX_STOPS 8
typedef struct LayerGradient {
    unsigned char enabled;
    unsigned char vertical; /* 1=to bottom, 0=to right */
    int count;
    Color colors[LAYER_GRADIENT_MAX_STOPS];
} LayerGradient;

typedef  int (*register_event_fun_t)(Layer* layer, const char* event_name, const char* event_func_name, EventHandler event_handler);
typedef  cJSON* (*get_property_fun_t)(Layer* layer, const char* property_name);
typedef  int (*set_property_fun_t)(Layer* layer, const char* key, cJSON* value, int is_creating);
typedef void (*set_style_fun_t)(Layer* layer, cJSON* style);

typedef struct Layer {
    char id[50];
    char variant[128];
    int index;
    LayerType type;
    Rect rect;
    Color color;
    Color bg_color;
    int radius; // 圆角半径
    int padding[4]; // style.padding: top, right, bottom, left
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

    // Layer 生命周期：声明位 + 运行时状态，见 layer_lifecycle.h
    unsigned char lifecycle_flags;
    char lifecycle_on_load[128];
    char lifecycle_on_show[128];
    char lifecycle_on_hide[128];
    char lifecycle_on_unload[128];

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

    // 数据更新回调（由组件各自注册）；返回 1 表示已接管 data 所有权
    int (*on_data_update)(Layer* layer, cJSON* data);

    // 销毁回调（由组件各自注册，释放 component 等资源）
    void (*on_destroy)(Layer* layer);

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

    // 显示/隐藏（组件可自定义，如 Dialog 弹出层）
    int (*set_visible)(Layer* layer, int visible);

    // 布局更新函数指针
    void (*layout)(Layer* layer);

    // 新增事件处理函数指针
    int (*handle_key_event)(Layer* layer, KeyEvent* event);
    int (*handle_pointer_event)(Layer* layer, PointerEvent* event);
    void (*handle_scroll_event)(Layer* layer, int scroll_delta);
    void (*handle_resize_event)(Layer* layer, const ResizeEvent* event);

    //事件注册
    register_event_fun_t register_event;
    int (*unregister_event)(Layer* layer, const char* event_name);
    
    //组件属性获取/设置函数
    get_property_fun_t get_property;
    set_property_fun_t set_property;
    set_style_fun_t set_style;

    // 毛玻璃效果相关属性
    int backdrop_filter;     // 是否启用毛玻璃效果
    int blur_radius;         // 模糊半径
    float saturation;         // 饱和度 (1.0为正常，>1.0为更饱和，<1.0为不饱和)
    float brightness;        // 亮度 (1.0为正常，>1.0为更亮，<1.0为更暗)

    /* box-shadow: offset-x offset-y blur spread color */
    LayerShadow shadow;
    /* 背景线性渐变（启用时优先于纯色 bgColor） */
    LayerGradient bg_gradient;
    
    // 增量更新支持：脏标记
    unsigned int dirty_flags; // 标记哪些属性被修改

    // inspect
    int inspect_enabled;     // 是否启用Inspect调试模式
    int inspect_mode;         // Inspect模式
    int inspect_show_bounds; // 是否显示边界
    int inspect_show_info;   // 是否显示信息

    // 初始布局快照，用于 layout_resize 等比缩放
    Rect layout_base_rect;
    int layout_base_fixed_w;
    int layout_base_fixed_h;
    unsigned char layout_base_valid;

    int connectable;

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

#ifdef YUI_LIST_COMPONENT
#include "components/list_component.h"
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

#ifdef YUI_SASH_COMPONENT
#include "components/sash_component.h"
#endif

#ifdef YUI_TABLE_COMPONENT
#include "components/table_component.h"
#endif

#ifdef YUI_PAGINATION_COMPONENT
#include "components/pagination_component.h"
#endif

#ifdef YUI_LOADING_COMPONENT
#include "components/loading_component.h"
#endif

#endif