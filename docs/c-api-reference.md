# YUI C API Reference

## 目录
- [图层系统 API](#图层系统-api)
- [渲染系统 API](#渲染系统-api)
- [事件系统 API](#事件系统-api)
- [布局系统 API](#布局系统-api)
- [动画系统 API](#动画系统-api)
- [主题系统 API](#主题系统-api)
- [工具函数 API](#工具函数-api)

## 图层系统 API

图层是 YUI 的核心概念，所有的 UI 元素都是以图层的形式存在。

### 基础操作

```c
#include "layer.h"
```

#### 创建和销毁

```c
/**
 * @brief 从 JSON 对象创建图层
 * @param json_obj JSON 对象
 * @param parent 父图层
 * @return 新创建的图层指针，失败返回 NULL
 */
Layer* layer_create_from_json(cJSON* json_obj, Layer* parent);

/**
 * @brief 从 JSON 字符串创建图层
 * @param json_str JSON 字符串
 * @param parent 父图层
 * @return 新创建的图层指针，失败返回 NULL
 */
Layer* parse_layer_from_string(const char* json_str, Layer* parent);

/**
 * @brief 创建空白图层
 * @param root_layer 根图层
 * @param x X 坐标
 * @param y Y 坐标
 * @param width 宽度
 * @param height 高度
 * @return 新创建的图层指针
 */
Layer* layer_create(Layer* root_layer, int x, int y, int width, int height);

/**
 * @brief 销毁图层及其所有子图层
 * @param layer 要销毁的图层
 */
void destroy_layer(Layer* layer);
```

#### 查找和遍历

```c
/**
 * @brief 根据 ID 查找图层
 * @param root 根图层
 * @param id 图层 ID
 * @return 找到的图层指针，未找到返回 NULL
 */
Layer* find_layer_by_id(Layer* root, const char* id);
```

#### 属性操作

```c
/**
 * @brief 设置图层标签文本
 * @param layer 目标图层
 * @param value 标签文本
 */
void layer_set_label(Layer* layer, const char* value);

/**
 * @brief 设置图层文本内容
 * @param layer 目标图层
 * @param value 文本内容
 */
void layer_set_text(Layer* layer, const char* value);

/**
 * @brief 获取图层标签文本
 * @param layer 目标图层
 * @return 标签文本指针
 */
const char* layer_get_label(const Layer* layer);

/**
 * @brief 获取图层文本内容
 * @param layer 目标图层
 * @return 文本内容指针
 */
const char* layer_get_text(const Layer* layer);
```

### 图层属性管理

```c
#include "layer_properties.h"
```

#### 位置和大小

```c
/**
 * @brief 设置图层位置
 * @param layer 目标图层
 * @param x X 坐标
 * @param y Y 坐标
 */
void layer_set_position(Layer* layer, int x, int y);

/**
 * @brief 设置图层大小
 * @param layer 目标图层
 * @param width 宽度
 * @param height 高度
 */
void layer_set_size(Layer* layer, int width, int height);

/**
 * @brief 获取图层位置
 * @param layer 目标图层
 * @param x 输出 X 坐标
 * @param y 输出 Y 坐标
 */
void layer_get_position(const Layer* layer, int* x, int* y);

/**
 * @brief 获取图层大小
 * @param layer 目标图层
 * @param width 输出宽度
 * @param height 输出高度
 */
void layer_get_size(const Layer* layer, int* width, int* height);
```

#### 样式属性

```c
/**
 * @brief 设置背景颜色
 * @param layer 目标图层
 * @param color 颜色值 (RGBA 格式)
 */
void layer_set_bg_color(Layer* layer, uint32_t color);

/**
 * @brief 设置前景颜色
 * @param layer 目标图层
 * @param color 颜色值 (RGBA 格式)
 */
void layer_set_fg_color(Layer* layer, uint32_t color);

/**
 * @brief 设置透明度
 * @param layer 目标图层
 * @param alpha 透明度值 (0-255)
 */
void layer_set_alpha(Layer* layer, uint8_t alpha);

/**
 * @brief 设置可见性
 * @param layer 目标图层
 * @param visible 是否可见
 */
void layer_set_visible(Layer* layer, bool visible);

/**
 * @brief 设置启用状态
 * @param layer 目标图层
 * @param enabled 是否启用
 */
void layer_set_enabled(Layer* layer, bool enabled);
```

### 图层更新

```c
#include "layer_update.h"
```

```c
/**
 * @brief 标记图层为脏状态，需要重绘
 * @param layer 目标图层
 */
void layer_mark_dirty(Layer* layer);

/**
 * @brief 强制更新图层
 * @param layer 目标图层
 */
void layer_force_update(Layer* layer);

/**
 * @brief 更新图层树
 * @param root 根图层
 */
void layer_update_tree(Layer* root);
```

## 渲染系统 API

```c
#include "render.h"
```

### 渲染器管理

```c
/**
 * @brief 初始化渲染系统
 * @return 成功返回 true，失败返回 false
 */
bool render_init(void);

/**
 * @brief 关闭渲染系统
 */
void render_shutdown(void);

/**
 * @brief 开始渲染帧
 */
void render_begin_frame(void);

/**
 * @brief 结束渲染帧
 */
void render_end_frame(void);
```

### 图层渲染

```c
/**
 * @brief 渲染单个图层
 * @param layer 要渲染的图层
 */
void render_layer(Layer* layer);

/**
 * @brief 渲染图层树
 * @param root 根图层
 */
void render_layer_tree(Layer* root);

/**
 * @brief 渲染带有裁剪的图层
 * @param layer 要渲染的图层
 * @param clip_rect 裁剪矩形
 */
void render_layer_clipped(Layer* layer, const Rect* clip_rect);
```

### 绘制原语

```c
/**
 * @brief 绘制矩形
 * @param rect 矩形区域
 * @param color 颜色值
 */
void render_draw_rect(const Rect* rect, uint32_t color);

/**
 * @brief 绘制带圆角的矩形
 * @param rect 矩形区域
 * @param color 颜色值
 * @param radius 圆角半径
 */
void render_draw_rounded_rect(const Rect* rect, uint32_t color, int radius);

/**
 * @brief 绘制文本
 * @param text 文本内容
 * @param x X 坐标
 * @param y Y 坐标
 * @param color 文本颜色
 * @param font_size 字体大小
 */
void render_draw_text(const char* text, int x, int y, uint32_t color, int font_size);

/**
 * @brief 绘制纹理
 * @param texture 纹理对象
 * @param src_rect 源矩形 (NULL 表示整个纹理)
 * @param dst_rect 目标矩形
 */
void render_draw_texture(Texture* texture, const Rect* src_rect, const Rect* dst_rect);
```

## 事件系统 API

```c
#include "event.h"
```

### 事件类型定义

```c
typedef enum {
    EVENT_NONE = 0,
    EVENT_MOUSE_DOWN,
    EVENT_MOUSE_UP,
    EVENT_MOUSE_MOVE,
    EVENT_MOUSE_ENTER,
    EVENT_MOUSE_LEAVE,
    EVENT_KEY_DOWN,
    EVENT_KEY_UP,
    EVENT_TEXT_INPUT,
    EVENT_WINDOW_RESIZE,
    EVENT_WINDOW_CLOSE,
    EVENT_FOCUS_GAINED,
    EVENT_FOCUS_LOST,
    EVENT_CUSTOM
} EventType;
```

### 事件结构

```c
typedef struct {
    EventType type;          // 事件类型
    Layer* target;           // 目标图层
    int x, y;               // 鼠标坐标
    int button;             // 鼠标按键
    int key_code;           // 键码
    char* text;             // 输入文本
    void* user_data;        // 用户数据
    uint32_t timestamp;     // 时间戳
} Event;
```

### 事件处理

```c
/**
 * @brief 事件处理器函数类型
 * @param layer 目标图层
 * @param event 事件对象
 * @return 是否处理了事件
 */
typedef bool (*EventHandler)(Layer* layer, Event* event);

/**
 * @brief 注册事件处理器
 * @param layer 目标图层
 * @param type 事件类型
 * @param handler 事件处理器
 */
void layer_register_event_handler(Layer* layer, EventType type, EventHandler handler);

/**
 * @brief 分发事件
 * @param event 事件对象
 * @return 是否被处理
 */
bool event_dispatch(Event* event);

/**
 * @brief 处理事件冒泡
 * @param layer 起始图层
 * @param event 事件对象
 * @return 是否被处理
 */
bool event_bubble(Layer* layer, Event* event);
```

### 事件辅助函数

```c
/**
 * @brief 创建鼠标事件
 * @param type 事件类型
 * @param x X 坐标
 * @param y Y 坐标
 * @param button 鼠标按键
 * @return 事件对象
 */
Event* event_create_mouse(EventType type, int x, int y, int button);

/**
 * @brief 创建键盘事件
 * @param type 事件类型
 * @param key_code 键码
 * @return 事件对象
 */
Event* event_create_key(EventType type, int key_code);

/**
 * @brief 销毁事件对象
 * @param event 事件对象
 */
void event_destroy(Event* event);
```

## 布局系统 API

```c
#include "layout.h"
```

### 布局类型

```c
typedef enum {
    LAYOUT_NONE = 0,
    LAYOUT_HORIZONTAL,
    LAYOUT_VERTICAL,
    LAYOUT_GRID,
    LAYOUT_ABSOLUTE
} LayoutType;
```

### 布局结构

```c
typedef struct {
    LayoutType type;        // 布局类型
    int spacing;            // 子元素间距
    int padding[4];         // 内边距 [top, right, bottom, left]
    int align;              // 对齐方式
    int justify;            // 主轴对齐
    float flex_ratio;       // 弹性比例
} Layout;
```

### 布局操作

```c
/**
 * @brief 设置图层布局
 * @param layer 目标图层
 * @param layout 布局对象
 */
void layer_set_layout(Layer* layer, const Layout* layout);

/**
 * @brief 计算布局
 * @param layer 目标图层
 */
void layout_calculate(Layer* layer);

/**
 * @brief 更新布局树
 * @param root 根图层
 */
void layout_update_tree(Layer* root);
```

### 布局辅助函数

```c
/**
 * @brief 创建水平布局
 * @param spacing 元素间距
 * @return 布局对象
 */
Layout* layout_create_horizontal(int spacing);

/**
 * @brief 创建垂直布局
 * @param spacing 元素间距
 * @return 布局对象
 */
Layout* layout_create_vertical(int spacing);

/**
 * @brief 创建网格布局
 * @param columns 列数
 * @param spacing 元素间距
 * @return 布局对象
 */
Layout* layout_create_grid(int columns, int spacing);
```

## 动画系统 API

```c
#include "animate.h"
```

### 动画属性

```c
typedef enum {
    ANIM_PROP_NONE = 0,
    ANIM_PROP_X,
    ANIM_PROP_Y,
    ANIM_PROP_WIDTH,
    ANIM_PROP_HEIGHT,
    ANIM_PROP_ALPHA,
    ANIM_PROP_ROTATION,
    ANIM_PROP_SCALE_X,
    ANIM_PROP_SCALE_Y,
    ANIM_PROP_COLOR
} AnimationProperty;
```

### 缓动函数

```c
typedef float (*EasingFunction)(float t);

// 常用缓动函数
extern EasingFunction ease_linear;
extern EasingFunction ease_in_quad;
extern EasingFunction ease_out_quad;
extern EasingFunction ease_in_out_quad;
extern EasingFunction ease_out_elastic;
extern EasingFunction ease_in_bounce;
```

### 动画结构

```c
typedef struct {
    AnimationProperty property;  // 动画属性
    float from_value;           // 起始值
    float to_value;             // 目标值
    float duration;             // 持续时间(秒)
    EasingFunction easing;      // 缓动函数
    bool auto_reverse;          // 是否自动反向
    int repeat_count;           // 重复次数(-1为无限)
} Animation;
```

### 动画操作

```c
/**
 * @brief 创建动画
 * @param property 动画属性
 * @param from_value 起始值
 * @param to_value 目标值
 * @param duration 持续时间
 * @param easing 缓动函数
 * @return 动画对象
 */
Animation* animation_create(AnimationProperty property, float from_value, 
                          float to_value, float duration, EasingFunction easing);

/**
 * @brief 开始动画
 * @param layer 目标图层
 * @param animation 动画对象
 */
void animation_start(Layer* layer, Animation* animation);

/**
 * @brief 停止动画
 * @param layer 目标图层
 */
void animation_stop(Layer* layer);

/**
 * @brief 暂停动画
 * @param layer 目标图层
 */
void animation_pause(Layer* layer);

/**
 * @brief 恢复动画
 * @param layer 目标图层
 */
void animation_resume(Layer* layer);

/**
 * @brief 更新动画
 * @param layer 目标图层
 * @param delta_time 帧间隔时间
 */
void animation_update(Layer* layer, float delta_time);
```

### 动画组

```c
/**
 * @brief 动画组结构
 */
typedef struct {
    Animation** animations;     // 动画数组
    int count;                  // 动画数量
    bool parallel;              // 是否并行执行
} AnimationGroup;

/**
 * @brief 创建动画组
 * @param parallel 是否并行执行
 * @return 动画组对象
 */
AnimationGroup* animation_group_create(bool parallel);

/**
 * @brief 添加动画到组
 * @param group 动画组
 * @param animation 动画对象
 */
void animation_group_add(AnimationGroup* group, Animation* animation);

/**
 * @brief 开始动画组
 * @param layer 目标图层
 * @param group 动画组
 */
void animation_group_start(Layer* layer, AnimationGroup* group);
```

## 主题系统 API

```c
#include "theme.h"
#include "theme_manager.h"
```

### 主题结构

```c
typedef struct {
    char* name;                 // 主题名称
    HashTable* styles;          // 样式映射
    Color primary_color;        // 主色调
    Color secondary_color;      // 次色调
    Color background_color;     // 背景色
    Color text_color;           // 文字颜色
} Theme;
```

### 主题管理

```c
/**
 * @brief 加载主题
 * @param path 主题文件路径
 * @return 主题对象
 */
Theme* theme_load(const char* path);

/**
 * @brief 应用主题到图层
 * @param theme 主题对象
 * @param layer 目标图层
 */
void theme_apply_to_layer(Theme* theme, Layer* layer);

/**
 * @brief 应用主题到图层树
 * @param theme 主题对象
 * @param root 根图层
 */
void theme_apply_to_tree(Theme* theme, Layer* root);

/**
 * @brief 获取样式值
 * @param theme 主题对象
 * @param selector 选择器
 * @param property 属性名
 * @return 样式值
 */
const char* theme_get_style(Theme* theme, const char* selector, const char* property);
```

### 主题管理器

```c
/**
 * @brief 注册主题
 * @param name 主题名称
 * @param theme 主题对象
 */
void theme_manager_register(const char* name, Theme* theme);

/**
 * @brief 设置当前主题
 * @param name 主题名称
 * @return 是否成功
 */
bool theme_manager_set_current(const char* name);

/**
 * @brief 获取当前主题
 * @return 当前主题对象
 */
Theme* theme_manager_get_current(void);

/**
 * @brief 应用当前主题到图层树
 * @param root 根图层
 * @return 是否成功
 */
bool theme_manager_apply_to_tree(Layer* root);
```

## 工具函数 API

```c
#include "util.h"
```

### 字符串工具

```c
/**
 * @brief 字符串复制
 * @param str 源字符串
 * @return 复制的字符串
 */
char* util_strdup(const char* str);

/**
 * @brief 字符串比较
 * @param str1 字符串1
 * @param str2 字符串2
 * @return 比较结果
 */
int util_strcmp(const char* str1, const char* str2);

/**
 * @brief 字符串格式化
 * @param format 格式字符串
 * @param ... 参数
 * @return 格式化后的字符串
 */
char* util_sprintf(const char* format, ...);
```

### 内存工具

```c
/**
 * @brief 安全内存分配
 * @param size 分配大小
 * @return 分配的内存指针
 */
void* util_malloc(size_t size);

/**
 * @brief 安全内存重分配
 * @param ptr 原内存指针
 * @param size 新大小
 * @return 重新分配的内存指针
 */
void* util_realloc(void* ptr, size_t size);

/**
 * @brief 安全内存释放
 * @param ptr 要释放的指针
 */
void util_free(void* ptr);
```

### 数学工具

```c
/**
 * @brief 限制数值范围
 * @param value 原值
 * @param min 最小值
 * @param max 最大值
 * @return 限制后的值
 */
int util_clamp(int value, int min, int max);

/**
 * @brief 线性插值
 * @param start 起始值
 * @param end 结束值
 * @param t 插值因子(0-1)
 * @return 插值结果
 */
float util_lerp(float start, float end, float t);

/**
 * @brief 颜色混合
 * @param color1 颜色1
 * @param color2 颜色2
 * @param ratio 混合比例
 * @return 混合后的颜色
 */
uint32_t util_blend_color(uint32_t color1, uint32_t color2, float ratio);
```

### 日志工具

```c
typedef enum {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR
} LogLevel;

/**
 * @brief 设置日志级别
 * @param level 日志级别
 */
void util_log_set_level(LogLevel level);

/**
 * @brief 输出调试日志
 * @param format 格式字符串
 * @param ... 参数
 */
void util_log_debug(const char* format, ...);

/**
 * @brief 输出信息日志
 * @param format 格式字符串
 * @param ... 参数
 */
void util_log_info(const char* format, ...);

/**
 * @brief 输出警告日志
 * @param format 格式字符串
 * @param ... 参数
 */
void util_log_warn(const char* format, ...);

/**
 * @brief 输出错误日志
 * @param format 格式字符串
 * @param ... 参数
 */
void util_log_error(const char* format, ...);
```

## 数据类型参考

### 基础类型

```c
// 颜色类型 (RGBA)
typedef uint32_t Color;

// 矩形结构
typedef struct {
    int x, y;           // 位置
    int width, height;  // 尺寸
} Rect;

// 点结构
typedef struct {
    int x, y;
} Point;

// 尺寸结构
typedef struct {
    int width, height;
} Size;
```

### 容器类型

```c
// 动态数组
typedef struct {
    void** data;        // 数据指针数组
    size_t size;        // 当前大小
    size_t capacity;    // 容量
} Vector;

// 哈希表
typedef struct {
    // 内部实现
} HashTable;

// 字符串映射
typedef struct {
    char* key;
    void* value;
} KeyValuePair;
```

## 错误处理

### 错误码

```c
typedef enum {
    YUI_SUCCESS = 0,
    YUI_ERROR_INVALID_PARAM,
    YUI_ERROR_OUT_OF_MEMORY,
    YUI_ERROR_FILE_NOT_FOUND,
    YUI_ERROR_PARSE_FAILED,
    YUI_ERROR_RENDER_FAILED,
    YUI_ERROR_UNKNOWN
} YUIErrorCode;
```

### 错误检查宏

```c
#define YUI_CHECK(condition, error_code) \
    do { \
        if (!(condition)) { \
            util_log_error("Check failed: %s at %s:%d", #condition, __FILE__, __LINE__); \
            return error_code; \
        } \
    } while(0)

#define YUI_ASSERT(condition) \
    do { \
        if (!(condition)) { \
            util_log_error("Assertion failed: %s at %s:%d", #condition, __FILE__, __LINE__); \
            abort(); \
        } \
    } while(0)
```

## 最佳实践

### 内存管理

```c
// 正确的资源管理方式
Layer* create_button_layer(void) {
    Layer* button = layer_create(NULL, 0, 0, 100, 30);
    if (!button) {
        return NULL;
    }
    
    // 设置属性
    layer_set_text(button, "Click Me");
    
    return button;
    // 调用者负责销毁
}

// 使用示例
void example_usage(void) {
    Layer* button = create_button_layer();
    if (button) {
        // 使用 button...
        destroy_layer(button);  // 记得销毁
    }
}
```

### 错误处理

```c
// 推荐的错误处理模式
YUIErrorCode initialize_system(void) {
    if (!render_init()) {
        util_log_error("Failed to initialize renderer");
        return YUI_ERROR_RENDER_FAILED;
    }
    
    if (!event_system_init()) {
        util_log_error("Failed to initialize event system");
        render_shutdown();
        return YUI_ERROR_UNKNOWN;
    }
    
    return YUI_SUCCESS;
}
```

### 事件处理

```c
// 事件处理器示例
bool button_click_handler(Layer* layer, Event* event) {
    if (event->type == EVENT_MOUSE_DOWN) {
        // 处理点击事件
        util_log_info("Button clicked: %s", layer_get_text(layer));
        return true;  // 标记事件已处理
    }
    return false;  // 事件未处理，继续传播
}

// 注册事件处理器
void setup_button(Layer* button) {
    layer_register_event_handler(button, EVENT_MOUSE_DOWN, button_click_handler);
}
```

---

*文档持续更新中，API 可能在未来版本中有所变化*