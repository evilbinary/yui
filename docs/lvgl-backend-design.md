#                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          YUI LVGL Backend 与组件注册系统设计

> 版本：草案 v0.2  
> 日期：2026-07-13  
> 状态：设计评审完成，已实现

## 目录

- [背景与目标](#背景与目标)
- [总体架构](#总体架构)
- [目录结构](#目录结构)
- [Backend 层设计](#backend-层设计)
- [LVGL Port 层](#lvgl-port-层)
- [组件注册系统](#组件注册系统)
- [统一创建函数与内置注册表](#统一创建函数与内置注册表)
- [与 layer.c 的集成](#与-layerc-的集成)
- [LVGL Module 包装层](#lvgl-module-包装层)
- [渲染与输入](#渲染与输入)
- [构建系统](#构建系统)
- [类型 ID 空间](#类型-id-空间)
- [迁移计划](#迁移计划)
- [风险与对策](#风险与对策)
- [待确认事项](#待确认事项)

---

## 背景与目标

YUI 当前通过 `backend.h` 抽象渲染与平台能力，已有：

- `backend_sdl.c` — 桌面 / Web（Emscripten）
- `backend_stm32.c` — 嵌入式帧缓冲草案

计划引入 **LVGL** 作为嵌入式（及 PC 模拟）backend，并统一 **原生 YUI 组件** 与 **LVGL Widget 组件** 的注册与解析机制。

### 设计目标

1. **零侵入上层语义**：`Layer` / JSON / `render.c` / `event.c` 继续工作
2. **复用 LVGL Port**：display / indev / tick 使用官方或板级 port，不在 YUI 内重复写硬件驱动
3. **统一组件注册表**：同时接管 `layer_type_name` 名称解析与 `YuiComponentCreateFn` 工厂
4. **渐进迁移**：内置组件枚举 id 不变，`layout.c` 等 `layer->type == XXX` 无需改动
5. **按需裁剪**：`lv_conf.h` + 构建 feature 控制 LVGL widget 体积

### 非目标（首版不做）

- 运行时 dlopen 插件（MCU 不适用）
- 将 Layer 树完整映射为 LVGL Widget 树（保留 YUI 即时渲染为主）
- 第一步迁移全部 if-else 到各 `*_component.c` 分散注册

---

## 总体架构

```
                    app / main
                        │
                        ▼
              yui (static, src/*)
                        │
        ┌───────────────┼───────────────┐
        ▼               ▼               ▼
 component_registry  backend_*    原生 components/*
        │               │
        │               ▼
        │          backend_common
        │
        ▼ (plat=lvgl 时由 app 链接)
   lvglmodule (lib/lvglmodule)
        │
        ▼
      lvgl (lib/lvgl + port_sdl / port_stm32)
```

### 职责边界


| 模块                                  | 职责                                     | 不做什么                       |
| ----------------------------------- | -------------------------------------- | -------------------------- |
| `lib/lvgl`                          | LVGL 源码 + port（display / indev / tick） | 不知道 Layer、JSON、`backend.h` |
| `lib/lvglmodule`                    | LVGL Widget → YUI Component 适配与注册      | 不实现 port、不实现 `fill_rect`   |
| `src/components/component_registry` | 类型名表 + 组件工厂 + `instantiate`            | 不依赖 LVGL 头文件               |
| `src/backend/backend_lvgl`          | `backend.h` P0 绘制 + 主循环 + 调 port       | 不写具体 widget 包装             |
| `src/backend/backend_common`        | 可选 API stub + 共享工具                     | 不含平台逻辑                     |


### 主循环（LVGL 构建）

```
lv_port_input_poll()           → 更新 indev
yui_bridge_indev_to_events()   → indev → YUI MouseEvent / KeyEvent
yui_run_update_callbacks()
backend_render_clear_color()
render_layer(ui_root)          → 原生组件画帧缓冲
popup_manager_render()
lvgl_component_sync_layout()   → LVGL widget 对齐 layer->rect（在 layout 或帧末）
lv_port_flush()                → 刷屏
lv_timer_handler()
```

---

## 目录结构

```
src/
├── backend/
│   ├── backend.h
│   ├── backend_common.h/c     # stub + 颜色/裁剪等共享工具
│   ├── backend_sdl.c
│   ├── backend_lvgl.c         # backend.h P0 + 调 port
│   └── backend_stm32.c        # 可保留，逐步由 lvgl+port_stm32 替代
├── components/
│   ├── component_registry.h   # 组件注册 API
│   ├── component_registry.c   # 表实现 + instantiate
│   ├── component_registry_builtin.c  # 22 个内置类型注册
│   ├── button_component.c
│   └── ...
└── layer.c                    # 改用 resolve + instantiate

lib/
├── lvgl/                      # LVGL 上游 + port（纯 LVGL）
│   ├── ya.py                  # target('lvgl')
│   ├── lv_conf.h
│   ├── src/                   # LVGL 源码
│   ├── port_sdl/
│   │   ├── lv_port.h
│   │   ├── lv_port_disp.c
│   │   └── lv_port_indev.c
│   └── port_stm32/
│       ├── lv_port.h
│       ├── lv_port_disp.c
│       └── lv_port_indev.c
└── lvglmodule/                # LVGL widget 的 YUI 包装
    ├── ya.py                  # target('lvglmodule')
    ├── lvgl_component.h
    ├── lvgl_register.c        # lvglmodule_register_all()
    ├── chart/
    │   └── lvgl_chart_component.c
    └── slider/
        └── lvgl_slider_component.c
```

---

## Backend 层设计

### backend.h 分层


| 层级        | API 示例                                                                              | 实现方                            |
| --------- | ----------------------------------------------------------------------------------- | ------------------------------ |
| **P0 必须** | `init/run/quit`、`fill_rect`、`text`、`texture copy`、`font load`、`get_ticks`、`present` | `backend_sdl` / `backend_lvgl` |
| **P1 常用** | `rounded_rect`、`clip`、`line`、`load_texture`                                         | 各 backend                      |
| **P2 可选** | `backdrop_filter`、`clipboard`、`IME`、`screenshot`、`titlebar`                         | weak stub 于 `backend_common`   |


### backend_common

```c
// backend_common.h（概念）

typedef enum {
    BACKEND_CAP_CLIP       = 1 << 0,
    BACKEND_CAP_ROUND_RECT = 1 << 1,
    BACKEND_CAP_BACKDROP   = 1 << 2,
    BACKEND_CAP_CLIPBOARD  = 1 << 3,
    BACKEND_CAP_IME        = 1 << 4,
    BACKEND_CAP_SCREENSHOT = 1 << 5,
} BackendCaps;

uint32_t backend_get_caps(void);
uint32_t backend_color_to_pixel(Color c, int cf);
void     backend_clip_push(Rect* clip);
void     backend_clip_pop(Rect* prev);
```

可选 API 在 `backend_common.c` 提供 `__attribute__((weak))` 占位；`backend_sdl.c` 全量覆盖，`backend_lvgl.c` 只实现 P0/P1 并声明能力子集。

### backend_lvgl 与 port 的边界

- **port 负责**：`lv_init`、display `flush_cb`、indev 读取、tick、`lv_port_get_draw_buf()`、`lv_port_get_root()`
- **backend_lvgl 负责**：向 draw buffer 执行 `backend_render_`*、主循环编排、indev → YUI 事件桥接
- **不在 YUI 内写**：LTDC / SPI LCD / SDL 窗口驱动细节

### ytype.h 解耦

当前 `Texture`、`DFont`、`Rect`、`Color` 在非 SDL 场景仍映射 SDL 类型，LVGL 构建需增加第三套：

```c
#if defined(YUI_BACKEND_SDL)
    #define Texture SDL_Texture
    #define DFont   TTF_Font
    ...
#elif defined(YUI_BACKEND_LVGL)
    // YuiTexture / YuiFont / 独立 Rect / Color
#elif defined(YUI_BACKEND_STM32)
    // 现有或保留类型
#endif
```

---

## LVGL Port 层

`lib/lvgl/port_sdl` 与 `lib/lvgl/port_stm32` 仅提供统一 C API：

```c
// lv_port.h

int  lv_port_init(void);
void lv_port_deinit(void);
void lv_port_input_poll(void);
void lv_port_flush(void);
void* lv_port_get_draw_buf(void);
lv_obj_t* lv_port_get_root(void);
```

PC 调试可用 LVGL 9 的 SDL 驱动或自建 `port_sdl`；STM32 使用 LTDC / DMA2D 等已有板级 `flush_cb`。

---

## 组件注册系统

`component_registry` 放在 `src/components/`，同时替代：

1. 静态 `layer_type_name[]` — JSON `"type"` 字符串解析
2. `layer.c` 巨型 if-else — 组件 `create_from_json` 工厂

设计参考现有 `register_event_handler()` 动态注册模式。

### 数据结构

现有 `layer.c` 中约 20 个组件分支，**绝大多数**创建函数签名相同：

```c
void* xxx_component_create_from_json(Layer* layer, cJSON* json);
```

因此注册表采用 **统一的创建函数类型**，直接挂各组件已有工厂函数，无需为每个 type 写一层包装。

```c
// src/components/component_registry.h

#define YUI_COMP_FOCUSABLE       (1 << 0)
#define YUI_COMP_CUSTOM_CHILDREN (1 << 1)  // List / Table / Pagination
#define YUI_COMP_SKIP_CHILDREN   (1 << 2)  // Scrollbar
#define YUI_COMP_NATIVE_RENDER   (1 << 3)  // layer->render + backend_render_*
#define YUI_COMP_LVGL_WIDGET     (1 << 4)  // lv_obj_t，不走 render 管线

#define YUI_BACKEND_SDL  (1 << 0)
#define YUI_BACKEND_LVGL (1 << 1)
#define YUI_BACKEND_ALL  (YUI_BACKEND_SDL | YUI_BACKEND_LVGL)

/* 统一创建签名：layer.c 只调 ops->create(layer, json) */
typedef void* (*YuiComponentCreateFn)(Layer* layer, cJSON* json);

typedef struct YuiComponentOps {
    YuiComponentCreateFn create;   /* 可为 NULL（View / Grid 等纯容器） */
    void  (*destroy)(Layer* layer);

    void  (*on_layout)(Layer* layer);
    int   (*handle_mouse)(Layer* layer, MouseEvent* e);
    int   (*handle_key)(Layer* layer, KeyEvent* e);
    void  (*after_create)(Layer* layer, cJSON* json);  /* 创建后副作用 */

    uint32_t flags;
    uint32_t backend_mask;
} YuiComponentOps;

typedef struct YuiComponentTypeEntry {
    int                    type_id;
    const char*            type_name;
    const YuiComponentOps* ops;
} YuiComponentTypeEntry;
```

各组件返回 `ButtonComponent*` 等具体指针时，注册表中需显式转为 `YuiComponentCreateFn`（或提供返回 `void*` 的薄适配器），以满足 C 函数指针类型一致。

### API

```c
void yui_component_registry_init(void);

// 内置：固定 type_id（0 .. LAYER_TYPE_BUILTIN_MAX-1）
int yui_component_register_builtin(int type_id,
                                   const char* type_name,
                                   const YuiComponentOps* ops);

// 动态：分配 type_id >= LAYER_TYPE_USER_BASE
int yui_component_register(const char* type_name,
                           const YuiComponentOps* ops);

// 替代 layer_type_name[]
int         yui_type_resolve(const char* type_name);
const char* yui_type_name(int type_id);
const YuiComponentOps* yui_type_get_ops(int type_id);
int         yui_type_count(void);
const YuiComponentTypeEntry* yui_type_entry_at(int index);

uint32_t yui_get_current_backend_mask(void);

// layer.c 唯一创建入口
int yui_component_instantiate(Layer* layer, cJSON* json_obj, Layer* parent,
                              int* out_custom_children);
```

---

## 统一创建函数与内置注册表

### 设计原则

1. **一个函数指针类型** `YuiComponentCreateFn`，对应 `(Layer*, cJSON*) -> void`*
2. **一张静态表** `g_builtin[]`，每行：`type_id` + JSON 名 + `create` 函数指针 + `flags`
3. **副作用不进 layer.c**：`focusable`、`has_custom_children` 等用 `flags`；Table 刷新数据等用 `after_create`
4. **签名不一致的组件**：在各自 `*_component.c` 内提供 2～5 行适配器，表内仍挂统一签名

### 内置注册表（component_registry_builtin.c）

```c
// src/components/component_registry_builtin.c

#include "component_registry.h"
#include "button_component.h"
#include "image_component.h"
#include "scrollbar_component.h"
/* ... 其余组件头文件 ... */

static void table_after_create(Layer* layer, cJSON* json);
static void scrollbar_after_create(Layer* layer, cJSON* json);

static const struct {
    int                 type_id;
    const char*         name;
    YuiComponentCreateFn create;
    uint32_t            flags;
    void                (*after_create)(Layer*, cJSON*);
} g_builtin[] = {
    { VIEW,        "View",       NULL,                                          YUI_COMP_NATIVE_RENDER, NULL },
    { BUTTON,      "Button",     (YuiComponentCreateFn)button_component_create_from_json,
                                                                                  YUI_COMP_NATIVE_RENDER | YUI_COMP_FOCUSABLE, NULL },
    { INPUT_FIELD, "Input",      (YuiComponentCreateFn)input_component_create_from_json,
                                                                                  YUI_COMP_NATIVE_RENDER | YUI_COMP_FOCUSABLE, NULL },
    { LABEL,       "Label",      (YuiComponentCreateFn)label_component_create_from_json,
                                                                                  YUI_COMP_NATIVE_RENDER, NULL },
    { IMAGE,       "Image",      image_component_create_from_json,                YUI_COMP_NATIVE_RENDER, NULL },
    { LAYER_LIST,  "List",       (YuiComponentCreateFn)list_component_create_from_json,
                                                                                  YUI_COMP_NATIVE_RENDER | YUI_COMP_FOCUSABLE | YUI_COMP_CUSTOM_CHILDREN, NULL },
    { GRID,        "Grid",       NULL,                                          YUI_COMP_NATIVE_RENDER, NULL },
    { PROGRESS,    "Progress",   (YuiComponentCreateFn)progress_component_create_from_json,
                                                                                  YUI_COMP_NATIVE_RENDER, NULL },
    { CHECKBOX,    "Checkbox",   (YuiComponentCreateFn)checkbox_component_create_from_json,
                                                                                  YUI_COMP_NATIVE_RENDER, NULL },
    { RADIOBOX,    "Radiobox",   (YuiComponentCreateFn)radiobox_component_create_from_json,
                                                                                  YUI_COMP_NATIVE_RENDER, NULL },
    { TEXT,        "Text",       (YuiComponentCreateFn)text_component_create_from_json,
                                                                                  YUI_COMP_NATIVE_RENDER | YUI_COMP_FOCUSABLE, NULL },
    { TREEVIEW,    "Treeview",   (YuiComponentCreateFn)treeview_component_create_from_json,
                                                                                  YUI_COMP_NATIVE_RENDER, NULL },
    { TAB,         "Tab",        (YuiComponentCreateFn)tab_component_create_from_json,
                                                                                  YUI_COMP_NATIVE_RENDER, NULL },
    { SLIDER,      "Slider",     (YuiComponentCreateFn)slider_component_create_from_json,
                                                                                  YUI_COMP_NATIVE_RENDER, NULL },
    { SELECT,      "Select",     (YuiComponentCreateFn)select_component_create_from_json,
                                                                                  YUI_COMP_NATIVE_RENDER | YUI_COMP_FOCUSABLE, NULL },
    { SCROLLBAR,   "Scrollbar",  scrollbar_component_create_from_json_adapter,
                                                                                  YUI_COMP_SKIP_CHILDREN, scrollbar_after_create },
    { MENU,        "Menu",       (YuiComponentCreateFn)menu_component_create_from_json,
                                                                                  YUI_COMP_NATIVE_RENDER, NULL },
    { DIALOG,      "Dialog",     (YuiComponentCreateFn)dialog_component_create_from_json,
                                                                                  YUI_COMP_NATIVE_RENDER, NULL },
    { CLOCK,       "Clock",      (YuiComponentCreateFn)clock_component_create_from_json,
                                                                                  YUI_COMP_NATIVE_RENDER, NULL },
    { SASH,        "Sash",       (YuiComponentCreateFn)sash_component_create_from_json,
                                                                                  YUI_COMP_NATIVE_RENDER, NULL },
    { TABLE,       "Table",      (YuiComponentCreateFn)table_component_create_from_json,
                                                                                  YUI_COMP_NATIVE_RENDER | YUI_COMP_FOCUSABLE | YUI_COMP_CUSTOM_CHILDREN, table_after_create },
    { PAGINATION,  "Pagination", (YuiComponentCreateFn)pagination_component_create_from_json,
                                                                                  YUI_COMP_NATIVE_RENDER | YUI_COMP_CUSTOM_CHILDREN, NULL },
};

void yui_components_register_builtin(void)
{
    for (size_t i = 0; i < sizeof(g_builtin) / sizeof(g_builtin[0]); i++) {
        const YuiComponentOps ops = {
            .create       = g_builtin[i].create,
            .after_create = g_builtin[i].after_create,
            .flags        = g_builtin[i].flags,
            .backend_mask = YUI_BACKEND_ALL,
        };
        yui_component_register_builtin(
            g_builtin[i].type_id,
            g_builtin[i].name,
            &ops);
    }
}
```

上表同时替代原 `layer.c` 中静态 `layer_type_name[]`（`name` 列即 JSON `"type"` 字符串）；全项目查询类型名改用 `yui_type_name(layer->type)`。

### 签名适配器（仅 2 处例外）


| 组件            | 原签名                               | 适配方式                                                                         |
| ------------- | --------------------------------- | ---------------------------------------------------------------------------- |
| **Image**     | 仅 `image_component_create(layer)` | 新增 `image_component_create_from_json`，内部忽略 `json`                            |
| **Scrollbar** | `(layer, parent, json)` 三参数       | 新增 `scrollbar_component_create_from_json_adapter`，从 `layer->parent` 取 parent |


```c
// image_component.c
void* image_component_create_from_json(Layer* layer, cJSON* json) {
    (void)json;
    return image_component_create(layer);
}

// scrollbar_component.c
void* scrollbar_component_create_from_json_adapter(Layer* layer, cJSON* json) {
    Layer* target = layer->parent ? layer->parent : layer;
    if (!layer->parent) {
        printf("Warning: SCROLLBAR layer %s has no parent layer\n", layer->id);
    }
    return scrollbar_component_create_from_json(layer, target, json);
}
```

### after_create 钩子（替代 layer.c 内联副作用）

```c
// component_registry_builtin.c（文件内 static）

static void scrollbar_after_create(Layer* layer, cJSON* json) {
    (void)json;
    layer->child_count = 0;
    if (layer->children) {
        free(layer->children);
        layer->children = NULL;
    }
}

static void table_after_create(Layer* layer, cJSON* json) {
    (void)json;
    if (layer->data && layer->data->json && layer->on_data_update) {
        layer->on_data_update(layer, layer->data->json);
    }
}
```

原先在 `layer.c` if-else 里 scattered 的 `layer->focusable = 1`、`has_custom_children = 1` 等，统一由 `flags` 在 `yui_component_instantiate` 内处理，**不再按 type 写分支**。

### LVGL 动态组件注册（对照）

`lib/lvglmodule` 仍用 `yui_component_register("LvChart", &ops)` 分配动态 type_id；`ops.create` 同样是 `YuiComponentCreateFn`：

```c
yui_component_register("LvChart", &(YuiComponentOps){
    .create       = lvgl_chart_create,
    .on_layout    = lvgl_chart_layout,
    .flags        = YUI_COMP_LVGL_WIDGET,
    .backend_mask = YUI_BACKEND_LVGL,
});
```

### 其他注册方式（备选，非默认）


| 方式                               | 说明                                  | 选用时机              |
| -------------------------------- | ----------------------------------- | ----------------- |
| **静态表 `g_builtin[]`（本文推荐）**      | 类型/id/工厂/flags 一张表                  | 迁移与日常维护           |
| **各组件内 `const YuiComponentOps`** | ops 定义在 `button_component.c`，表内只引指针 | 表过大后拆分            |
| **各组件 `xxx_register()`**         | 启动时逐个调用                             | 需要按 feature 裁剪链接时 |
| **X-Macro 单表**                   | 一个 `.def` 展开 enum + 名 + 注册          | 强约束防漂移            |
| **链接段 constructor**              | 自动注册                                | 仅桌面；MCU 不推荐       |


内置组件默认采用 **静态表**；LVGL 组件采用 `**yui_component_register()` 动态表**。

---

## 与 layer.c 的集成

### 原有两步 → 统一为 registry 两步

**之前（割裂）：**

```
① type 字符串 → layer_type_name[] 查找 → layer->type
② layer->type  → if-else → xxx_component_create_from_json()
```

**之后（统一）：**

```
① yui_type_resolve(type_str)  → layer->type
② yui_component_instantiate(layer, json, parent)
```

### 解析 type

```c
if (cJSON_HasObjectItem(json_obj, "type")) {
    const char* type_str = cJSON_GetObjectItem(json_obj, "type")->valuestring;
    layer->type = yui_type_resolve(type_str);
}
```

### 创建 component（替代 if-else）

```c
int has_custom_children = 0;
int rc = yui_component_instantiate(layer, json_obj, parent, &has_custom_children);
if (rc == 0 && layer->type != VIEW) {
    LOGW("no ops for type '%s' (id=%d)", yui_type_name(layer->type), layer->type);
}
```

`has_custom_children` 由 `instantiate` 根据 `ops->flags` 写出，供后续 children 解析使用。

### yui_component_instantiate 内部逻辑

```c
int yui_component_instantiate(Layer* layer, cJSON* json, Layer* parent,
                              int* out_custom_children)
{
    const YuiComponentOps* ops = yui_type_get_ops(layer->type);
    if (!ops)
        return 0;

    if (!(ops->backend_mask & yui_get_current_backend_mask())) {
        LOGW("type '%s' not supported on current backend", yui_type_name(layer->type));
        return -1;
    }

    if (!ops->create)
        return 0;  /* View / Grid 等无 component */

    layer->component = ops->create(layer, json);

    if (ops->handle_mouse) layer->handle_mouse_event = ops->handle_mouse;
    if (ops->handle_key)   layer->handle_key_event   = ops->handle_key;
    if (ops->on_layout)    layer->layout             = ops->on_layout;
    if (ops->after_create) ops->after_create(layer, json);

    if (ops->flags & YUI_COMP_FOCUSABLE)
        layer->focusable = 1;

    if (out_custom_children && (ops->flags & YUI_COMP_CUSTOM_CHILDREN))
        *out_custom_children = 1;

    return layer->component ? 1 : 0;
}
```

**要点：** `layer.c` 内不再出现 `if (layer->type == BUTTON)` 等分支；Scrollbar 的 parent 逻辑在 `scrollbar_component_create_from_json_adapter` 内完成，清空 children 在 `scrollbar_after_create` 完成。

### 子节点解析标志

```c
const YuiComponentOps* ops = yui_type_get_ops(layer->type);
int skip_children   = ops && (ops->flags & YUI_COMP_SKIP_CHILDREN);
int custom_children = ops && (ops->flags & YUI_COMP_CUSTOM_CHILDREN);

if (children && !skip_children && !custom_children) {
    /* 原有 children 递归解析 */
}
```

### 特例处理策略


| 原 layer.c 行为                  | 新归属                                                 |
| ----------------------------- | --------------------------------------------------- |
| Image 无 `_from_json`          | `image_component_create_from_json` 适配器              |
| Scrollbar 需要 parent           | `scrollbar_component_create_from_json_adapter`      |
| Scrollbar 清空 children         | `scrollbar_after_create` + `YUI_COMP_SKIP_CHILDREN` |
| Table 调 `on_data_update`      | `table_after_create`                                |
| Text/Select/List 等设 focusable | `YUI_COMP_FOCUSABLE`                                |
| List/Table/Pagination 自定义子节点  | `YUI_COMP_CUSTOM_CHILDREN`                          |
| View / Grid 无 component       | `create = NULL`                                     |


### 启动顺序

```c
yui_component_registry_init();
yui_components_register_builtin();

#ifdef YUI_BACKEND_LVGL
lv_port_init();
lvglmodule_register_all();
#endif

/* 之后 parse_json / layer_create_from_json 可用全部类型 */
```

---

## LVGL Module 包装层

位于 `lib/lvglmodule/`，每个文件包装一个（或一类）LVGL widget。

### 公共结构

```c
// lib/lvglmodule/lvgl_component.h

typedef struct LvglComponent {
    Layer*    layer;
    lv_obj_t* obj;
    void*     widget_data;
} LvglComponent;

void lvgl_component_sync_rect(LvglComponent* c);
void lvgl_component_destroy(LvglComponent* c);
```

### 注册示例

```c
// lib/lvglmodule/lvgl_register.c

void lvglmodule_register_all(void)
{
#if LV_USE_CHART
    yui_component_register("LvChart", &(YuiComponentOps){
        .create       = lvgl_chart_create,
        .destroy      = lvgl_chart_destroy,
        .on_layout    = lvgl_chart_layout,
        .flags        = YUI_COMP_LVGL_WIDGET,
        .backend_mask = YUI_BACKEND_LVGL,
    });
#endif
}
```

在 `backend_lvgl.c` 的 `backend_init()` 中调用 `lvglmodule_register_all()`。

### JSON 约定

**推荐：显式前缀（不与原生组件冲突）**

```json
{ "type": "LvChart", "id": "cpu", "style": { "width": 200, "height": 120 } }
```

**可选：同 type + engine 字段**

```json
{ "type": "Chart", "engine": "lvgl" }
```

### Widget 生命周期

```
parse_layer_from_json
  → yui_type_resolve("LvChart")        → type_id = 256
  → yui_component_instantiate
      → lv_chart_create(root)
      → layer->component = LvglComponent*
      → layer->render = NULL
      → layer->layout = lvgl_chart_layout

backend_run 每帧
  → layout sync rect
  → render_layer 跳过 LVGL widget
  → lv_timer_handler()

destroy_layer
  → ops->destroy → lv_obj_del + free
```

### 链接依赖（避免循环）

- `lvglmodule` **只 include** `src/` 头文件（`layer.h`、`component_registry.h`）
- `lvglmodule` **不** `add_deps('yui')` 链接静态库
- 最终由 **app / main target** 同时链接 `yui` + `lvglmodule` + `lvgl`

---

## 渲染与输入

### 双渲染共存（同一页面）

```
View (YUI native, backend_render 背景)
 ├── Label (native)
 ├── Button (native)
 └── LvChart (lvglmodule, lv_obj 自绘)
```

`render_layer` 分流：

```c
const YuiComponentOps* ops = yui_type_get_ops(layer->type);

if (ops && (ops->flags & YUI_COMP_LVGL_WIDGET)) {
    if (layer->layout) layer->layout(layer);
} else if (layer->render) {
    layer->render(layer);
} else if (layer->type == VIEW) {
    /* 现有 VIEW 背景逻辑 */
}
```

### 输入优先级（可调）

1. `lv_port_input_poll()`
2. 若 LVGL 有 pressed / focused 对象 → 先交给 LVGL
3. 否则走 YUI `handle_mouse_event` 树

MVP 可先分页面验证（全 native 或全 Lv 前缀），再调混合优先级。

### 全项目 type 名迁移


| 文件                   | 改法                            |
| -------------------- | ----------------------------- |
| `layer.c`            | 删除 `layer_type_name[]`        |
| `theme_manager.c`    | `yui_type_name(layer->type)`  |
| `layer_properties.c` | 同上                            |
| `render.c`           | 同上                            |
| `ytype.h`            | 删除 `extern layer_type_name[]` |


---

## 构建系统

### lib/lvgl/ya.py

```python
target('lvgl')
set_kind('static')
add_files('src/**/*.c')
if plat in ['sdl', 'windows', 'linux', 'macosx']:
    add_files('port_sdl/*.c')
    add_cflags('-DYUI_LVGL_PORT_SDL')
elif plat == 'stm32':
    add_files('port_stm32/*.c')
    add_cflags('-DYUI_LVGL_PORT_STM32')
```

### lib/lvglmodule/ya.py

```python
target('lvglmodule')
set_kind('static')
add_deps('lvgl')
add_includedirs('../src', '../src/components')
add_files('**/*.c')
# 按 feature 裁剪 chart/、slider/ 等
```

### src/ya.py

```python
add_files('components/component_registry.c')
add_files('components/component_registry_builtin.c')
add_files('backend/backend_common.c')

if plat == 'lvgl':
    add_files('backend/backend_lvgl.c')
    add_cflags('-DYUI_BACKEND_LVGL')
    # app target 链接 lvgl + lvglmodule
elif plat == 'stm32':
    add_files('backend/backend_stm32.c')  # 过渡期可保留
else:
    add_files('backend/backend_sdl.c')
    add_cflags('-DYUI_BACKEND_SDL')
```

---

## 类型 ID 空间

```c
typedef enum {
    VIEW = 0,
    BUTTON,
    INPUT_FIELD,
    /* ... 现有顺序不变 ... */
    PAGINATION,

    LAYER_TYPE_BUILTIN_MAX,       // 内置上界，如 22
    LAYER_TYPE_USER_BASE = 256,   // 动态类型起点
} LayerType;
```


| 区间                              | 来源                               | 示例              |
| ------------------------------- | -------------------------------- | --------------- |
| `0 .. LAYER_TYPE_BUILTIN_MAX-1` | 编译期枚举，`register_builtin` 固定绑定    | `BUTTON = 1`    |
| `>= LAYER_TYPE_USER_BASE`       | `yui_component_register()` 运行时分配 | `LvChart = 256` |


`layout.c` 中 `layer->type == LAYER_LIST` 等比较 **无需修改**。

### Registry 内部实现要点

- 内置区：按下标 `g_types[type_id]` 直接存储
- 动态区：追加数组 + 按 `type_name` 线性或哈希查找
- `yui_type_resolve(name)`：遍历 name 表，未知返回 `VIEW(0)` 并打日志
- 建议 `YUI_MAX_TYPES = 128`（内置 + 动态总量上限）

---

## 迁移计划


| 阶段    | 内容                                                                 | layer.c if-else |
| ----- | ------------------------------------------------------------------ | --------------- |
| **0** | `component_registry` + `ytype.h` 解耦 SDL 类型                         | 不动              |
| **1** | `g_builtin[]` 静态表 + `yui_type_resolve/name`，删除 `layer_type_name[]` | **保留** if-else  |
| **2** | `yui_component_instantiate` 替代 if-else；补 Image/Scrollbar 适配器       | **删除** if-else  |
| **3** | `backend_common` + `lib/lvgl` + `port_sdl` + `backend_lvgl` MVP    | —               |
| **4** | `lib/lvglmodule` + `LvLabel` 验证动态注册                                | —               |
| **5** | 全项目 `layer_type_name` → `yui_type_name`                            | 完成              |
| **6** | `port_stm32` + 更多 widget                                           | 实机              |


### 阶段 0 文件清单（骨架）

```
src/components/component_registry.h
src/components/component_registry.c
src/components/component_registry_builtin.c
src/backend/backend_common.h
src/backend/backend_common.c
lib/lvgl/ya.py + port_sdl/lv_port.c
lib/lvglmodule/lvgl_register.c + lvgl_component.h
src/backend/backend_lvgl.c
```

---

## 风险与对策


| 风险                           | 对策                                                           |
| ---------------------------- | ------------------------------------------------------------ |
| `backend_common` 膨胀为上帝对象     | 仅 stub + 纯函数工具                                               |
| `lvglmodule` ↔ `yui` 循环依赖    | module 只 include 头文件，app 统一链接                                |
| JSON type 与 LVGL widget 数量爆炸 | `Lv` 前缀命名空间 + `lv_conf` 裁剪                                   |
| 主题 / inspect 不认识动态 type      | 统一 `yui_type_name()`                                         |
| 固件体积                         | `lv_conf.h` + ya feature + 只注册用到的 ops                        |
| Scrollbar parent 特例          | `scrollbar_component_create_from_json_adapter`，不在 layer.c 分支 |
| 混合渲染 Z-order / 输入冲突          | MVP 分页面验证，再定优先级                                              |


---

## 待确认事项

1. **动态 type_id 从 256 起** — 是否接受？
2. **内置注册** — 已定为 `g_builtin[]` 静态表；后续表过大时可拆 ops 到各 `*_component.c`
3. ~~**Scrollbar 特例**~~ — 已定为 adapter + `after_create`
4. **LVGL JSON 命名** — `LvChart` 前缀 vs `"engine": "lvgl"`？
5. `**backend_stm32.c**` — 是否与 `backend_lvgl` 长期并存（plat 切换）？
6. **PC 首验目标** — `port_sdl` 跑 `watch-os` vs 直接上 STM32？

---

## 相关文档

- [架构设计文档](architecture.md)
- [JSON 格式规范](json-format-spec.md)
- [事件系统](event.md)
- [Button 组件](components/button-component.md)

---

## 修订记录


| 版本   | 日期         | 说明                                                                                |
| ---- | ---------- | --------------------------------------------------------------------------------- |
| v0.1 | 2026-07-13 | 初稿：LVGL backend、component_registry、lvglmodule 统一设计                                |
| v0.2 | 2026-07-13 | 统一 `YuiComponentCreateFn`；`g_builtin[]` 静态注册表；Image/Scrollbar 适配器与 `after_create` |


