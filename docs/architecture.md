# YUI Architecture Design Document

## 目录
- [系统概述](#系统概述)
- [架构层次](#架构层次)
- [核心模块](#核心模块)
- [数据流设计](#数据流设计)
- [组件系统](#组件系统)
- [渲染管线](#渲染管线)
- [事件处理](#事件处理)
- [内存管理](#内存管理)
- [扩展机制](#扩展机制)

## 系统概述

YUI 是一个轻量级的 GUI 框架，采用分层架构设计，通过 JSON 配置文件描述用户界面。系统遵循关注点分离原则，将 UI 定义、业务逻辑和底层渲染完全解耦。

### 设计哲学

1. **声明式 UI**: 通过 JSON 描述界面，而非编程方式创建
2. **组件化**: 可复用的 UI 组件，支持嵌套和组合
3. **数据驱动**: 界面状态通过数据变更驱动更新
4. **跨平台**: 抽象的后端层屏蔽平台差异
5. **高性能**: 脏矩形渲染、纹理缓存等优化技术

### 技术栈

```
┌─────────────────────────────────────┐
│           Application Layer         │  ← JSON Config + JS Logic
├─────────────────────────────────────┤
│         JavaScript Engines          │  ← QuickJS/mquickjs/Mario
├─────────────────────────────────────┤
│           Component System          │  ← UI Components
├─────────────────────────────────────┤
│            Event System             │  ← Input Processing
├─────────────────────────────────────┤
│           Layout Manager            │  ← Auto Layout
├─────────────────────────────────────┤
│          Render Pipeline            │  ← Graphics Rendering
├─────────────────────────────────────┤
│           Backend Layer             │  ← SDL2/Graphics API
└─────────────────────────────────────┘
```

## 架构层次

### 1. 应用层 (Application Layer)

**职责**：
- 解析 JSON 配置文件
- 管理应用生命周期
- 协调各子系统工作

**主要组件**：
- `main.c` - 应用入口点
- JSON 解析器
- 应用状态管理器

### 2. 脚本层 (Scripting Layer)

**职责**：
- 提供 JavaScript 绑定
- 实现业务逻辑
- 处理用户交互

**支持引擎**：
- QuickJS - 轻量级 JS 引擎
- mquickjs - 增强版 QuickJS
- Mario - 自定义 JS 引擎

### 3. 组件层 (Component Layer)

**职责**：
- 实现各种 UI 组件
- 管理组件状态
- 处理组件间通信

**核心组件**：
- View (基础容器)
- Button (按钮)
- Input (输入框)
- Label (文本标签)
- Image (图像)
- List/Grid (列表/网格)

### 4. 系统层 (System Layer)

#### 事件系统 (Event System)
- 输入设备抽象
- 事件分发机制
- 事件冒泡处理

#### 布局系统 (Layout System)
- 自动布局计算
- 弹性布局支持
- 响应式设计

#### 渲染系统 (Render System)
- 图形渲染管线
- 脏矩形优化
- 纹理管理

### 5. 后端层 (Backend Layer)

**职责**：
- 平台相关实现
- 图形 API 抽象
- 系统资源管理

**当前实现**：
- SDL2 后端 (跨平台)
- 可扩展其他后端

## 核心模块

### 图层系统 (Layer System)

#### 数据结构

```c
typedef struct Layer {
    // 基本属性
    char* id;                    // 唯一标识符
    LayerType type;              // 图层类型
    Rect position;               // 位置和大小
    Color background_color;      // 背景色
    
    // 样式属性
    Style* style;                // 样式对象
    Layout* layout;              // 布局信息
    
    // 树形结构
    struct Layer* parent;        // 父节点
    Vector* children;            // 子节点列表
    
    // 组件关联
    Component* component;        // 关联的组件
    
    // 状态标志
    bool visible;                // 是否可见
    bool enabled;                // 是否启用
    bool dirty;                  // 是否需要重绘
    
    // 动画支持
    Animation* animation;        // 动画对象
    
    // 事件处理
    EventHandler* event_handlers; // 事件处理器
} Layer;
```

#### 核心功能

1. **树形管理**：
   ```c
   Layer* layer_create(LayerType type);
   void layer_add_child(Layer* parent, Layer* child);
   void layer_remove_child(Layer* parent, Layer* child);
   Layer* layer_find_by_id(Layer* root, const char* id);
   ```

2. **属性管理**：
   ```c
   void layer_set_position(Layer* layer, int x, int y);
   void layer_set_size(Layer* layer, int width, int height);
   void layer_set_style(Layer* layer, Style* style);
   ```

3. **生命周期**：
   ```c
   void layer_init(Layer* layer);
   void layer_update(Layer* layer, float delta_time);
   void layer_render(Layer* layer);
   void layer_destroy(Layer* layer);
   ```

### 渲染系统 (Render System)

#### 渲染管线

```
JSON Config
    ↓
Layer Tree Creation
    ↓
Style Resolution
    ↓
Layout Calculation
    ↓
Dirty Rectangle Detection
    ↓
Batch Rendering
    ↓
Backend API Calls
    ↓
Display Output
```

#### 关键优化

1. **脏矩形渲染**：
   ```c
   typedef struct DirtyRegion {
       Rect bounds;
       bool active;
   } DirtyRegion;
   
   void render_mark_dirty(Layer* layer);
   void render_update_dirty_regions(Renderer* renderer);
   ```

2. **纹理缓存**：
   ```c
   typedef struct TextureCache {
       HashTable* textures;  // 按文件名索引
       size_t max_memory;    // 最大内存限制
       size_t current_memory; // 当前使用内存
   } TextureCache;
   ```

3. **批处理**：
   ```c
   void batch_begin(void);
   void batch_draw_texture(Texture* tex, Rect* src, Rect* dst);
   void batch_end(void);
   ```

### 事件系统 (Event System)

#### 事件流

```
Input Device
    ↓
Backend Event Queue
    ↓
Event Normalization
    ↓
Event Dispatch
    ↓
Target Layer Detection
    ↓
Event Bubbling/Capturing
    ↓
Handler Execution
    ↓
State Update
```

#### 事件类型

```c
typedef enum EventType {
    EVENT_MOUSE_DOWN,
    EVENT_MOUSE_UP,
    EVENT_MOUSE_MOVE,
    EVENT_MOUSE_WHEEL,
    EVENT_KEY_DOWN,
    EVENT_KEY_UP,
    EVENT_TEXT_INPUT,
    EVENT_FOCUS_GAINED,
    EVENT_FOCUS_LOST,
    EVENT_RESIZE,
    EVENT_CLOSE
} EventType;
```

#### 事件处理机制

```c
typedef struct Event {
    EventType type;
    Layer* target;        // 目标图层
    Point position;       // 鼠标位置
    int key_code;         // 键码
    char* text;           // 输入文本
    void* user_data;      // 用户数据
} Event;

// 事件处理器类型
typedef bool (*EventHandler)(Layer* layer, Event* event);

// 事件分发
bool event_dispatch(Event* event);
bool event_bubble(Layer* layer, Event* event);
```

### 布局系统 (Layout System)

#### 布局算法

```c
typedef enum LayoutType {
    LAYOUT_ABSOLUTE,     // 绝对定位
    LAYOUT_HORIZONTAL,   // 水平布局
    LAYOUT_VERTICAL,     // 垂直布局
    LAYOUT_GRID,         // 网格布局
    LAYOUT_FLEX          // 弹性布局
} LayoutType;

typedef struct Layout {
    LayoutType type;
    int spacing;         // 间距
    int padding[4];      // 内边距
    Align align;         // 对齐方式
    Justify justify;     // 主轴对齐
    float flex_ratio;    // 弹性比例
} Layout;
```

#### 布局计算流程

```
Parent Size Known?
    ↓ No
Calculate Content Size
    ↓
Apply Layout Constraints
    ↓
Position Children
    ↓
Recursive Layout
    ↓
Update Dirty Flags
```

## 数据流设计

### 1. 初始化阶段

```
1. 应用启动
   ↓
2. 解析主配置 (app.json)
   ↓
3. 加载资源 (字体、图片)
   ↓
4. 创建根图层
   ↓
5. 解析UI配置 (JSON → Layer Tree)
   ↓
6. 初始化组件
   ↓
7. 应用主题样式
   ↓
8. 执行布局计算
   ↓
9. 首次渲染
```

### 2. 运行时数据流

```
用户输入
    ↓
事件系统
    ↓
目标图层检测
    ↓
事件处理器执行
    ↓
状态变更
    ↓
脏矩形标记
    ↓
布局重新计算 (如有必要)
    ↓
增量渲染
    ↓
显示更新
```

### 3. 动画数据流

```
动画启动
    ↓
每帧更新
    ↓
属性插值计算
    ↓
图层属性更新
    ↓
脏矩形标记
    ↓
渲染管线
    ↓
显示输出
```

## 组件系统

### 组件基类

```c
typedef struct Component {
    ComponentType type;
    Layer* layer;
    void* data;              // 组件私有数据
    ComponentVTable* vtable; // 虚函数表
} Component;

typedef struct ComponentVTable {
    void (*init)(Component* self);
    void (*update)(Component* self, float delta_time);
    void (*render)(Component* self);
    void (*destroy)(Component* self);
    bool (*handle_event)(Component* self, Event* event);
} ComponentVTable;
```

### 组件生命周期

```
创建 → 初始化 → 更新循环 → 销毁
   ↓       ↓         ↓         ↓
alloc   init      update    destroy
```

### 组件通信

#### 1. 父子通信

```c
// 子组件通知父组件
void component_notify_parent(Component* child, const char* message, void* data);

// 父组件控制子组件
void component_control_child(Component* parent, Component* child, const char* command, void* data);
```

#### 2. 事件总线

```c
// 全局事件发布
void event_bus_publish(const char* event_name, void* data);

// 事件订阅
void event_bus_subscribe(const char* event_name, EventBusHandler handler);

// 事件取消订阅
void event_bus_unsubscribe(const char* event_name, EventBusHandler handler);
```

## 渲染管线

### 管线阶段

```
1. 场景准备
   - 清除缓冲区
   - 设置视口
   
2. 图层遍历 (深度优先)
   - 检查可见性
   - 检查脏矩形
   
3. 样式应用
   - 解析样式规则
   - 应用主题
   
4. 变换计算
   - 位置变换
   - 旋转变换
   - 缩放变换
   
5. 几何绘制
   - 背景绘制
   - 边框绘制
   - 内容绘制
   
6. 后处理
   - 混合模式
   - 特效应用
   
7. 缓冲区交换
```

### 渲染优化策略

1. **空间分区**：
   ```c
   typedef struct SpatialHash {
       Grid* cells;
       int cell_size;
       int width, height;
   } SpatialHash;
   ```

2. **LOD 系统**：
   ```c
   typedef enum RenderQuality {
       QUALITY_LOW,     // 低质量，高性能
       QUALITY_MEDIUM,  // 中等质量
       QUALITY_HIGH     // 高质量，低性能
   } RenderQuality;
   ```

3. **渲染批处理**：
   ```c
   typedef struct RenderBatch {
       Texture* texture;
       VertexBuffer* vertices;
       IndexBuffer* indices;
       Shader* shader;
   } RenderBatch;
   ```

## 事件处理

### 事件捕获和冒泡

```
捕获阶段: Window → Parent → Target
目标阶段: Target 处理事件
冒泡阶段: Target → Parent → Window
```

### 事件优先级

```c
typedef enum EventPriority {
    PRIORITY_SYSTEM = 0,    // 系统级事件
    PRIORITY_HIGH = 1,      // 高优先级
    PRIORITY_NORMAL = 2,    // 普通优先级
    PRIORITY_LOW = 3        // 低优先级
} EventPriority;
```

### 事件过滤

```c
typedef struct EventFilter {
    EventTypeMask mask;     // 事件类型掩码
    Layer* target;          // 目标图层
    EventPredicate predicate; // 过滤谓词
    EventHandler handler;   // 处理函数
} EventFilter;
```

## 内存管理

### 内存分配策略

#### 1. 对象池

```c
typedef struct ObjectPool {
    void** free_list;
    size_t object_size;
    size_t capacity;
    size_t count;
} ObjectPool;

void* pool_alloc(ObjectPool* pool);
void pool_free(ObjectPool* pool, void* object);
```

#### 2. 引用计数

```c
typedef struct RefCounted {
    int ref_count;
    void (*destroy)(struct RefCounted* self);
} RefCounted;

void ref_inc(RefCounted* obj);
void ref_dec(RefCounted* obj);
```

#### 3. 内存跟踪

```c
#ifdef DEBUG_MEMORY
void mem_track_alloc(void* ptr, size_t size, const char* file, int line);
void mem_track_free(void* ptr, const char* file, int line);
void mem_report_leaks(void);
#endif
```

### 资源管理

#### 纹理管理

```c
typedef struct TextureManager {
    HashTable* textures;        // 按ID索引
    TextureCache* cache;        // LRU缓存
    size_t total_memory;        // 总内存使用
    size_t max_memory;          // 内存上限
} TextureManager;
```

#### 字体管理

```c
typedef struct FontManager {
    HashTable* fonts;           // 按名称索引
    Font* default_font;         // 默认字体
    int default_size;           // 默认字号
} FontManager;
```

## 扩展机制

### 插件系统

```c
typedef struct Plugin {
    char* name;
    int version;
    PluginInterface* interface;
    void* user_data;
} Plugin;

typedef struct PluginInterface {
    bool (*init)(Plugin* plugin);
    void (*shutdown)(Plugin* plugin);
    void (*update)(Plugin* plugin, float delta_time);
    void (*render)(Plugin* plugin);
} PluginInterface;
```

### 自定义组件

```c
// 注册新组件类型
void component_register_type(ComponentType type, ComponentFactory factory);

// 组件工厂函数
typedef Component* (*ComponentFactory)(Layer* layer);

// 示例：注册自定义按钮组件
component_register_type(CUSTOM_BUTTON, custom_button_create);
```

### 主题扩展

```c
// 注册自定义样式属性
void theme_register_property(const char* name, PropertyParser parser);

// 属性解析器
typedef bool (*PropertyParser)(const char* value, void* out_value);
```

### 后端扩展

```c
typedef struct BackendInterface {
    bool (*init)(void);
    void (*shutdown)(void);
    void (*create_window)(int width, int height);
    void (*render_begin)(void);
    void (*render_end)(void);
    // ... 更多接口
} BackendInterface;

// 注册新后端
void backend_register(const char* name, BackendInterface* interface);
```

## 性能指标

### 关键性能指标

| 指标 | 目标值 | 测量方法 |
|------|--------|----------|
| 帧率 | ≥ 60 FPS | Frame time measurement |
| 内存使用 | < 100MB | Memory profiler |
| 启动时间 | < 2秒 | Startup timer |
| 渲染延迟 | < 16ms | Render pipeline timing |
| 事件响应 | < 10ms | Event processing time |

### 性能监控

```c
typedef struct PerformanceMetrics {
    double frame_time_avg;      // 平均帧时间
    double frame_time_min;      // 最小帧时间
    double frame_time_max;      // 最大帧时间
    size_t memory_usage;        // 当前内存使用
    int draw_calls_per_frame;   // 每帧绘制调用数
    int texture_switches;       // 纹理切换次数
} PerformanceMetrics;
```

---

*本文档将持续更新以反映架构演进*