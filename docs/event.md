# 事件系统

本文档分两部分：

1. **[原生输入事件架构](#原生输入事件架构)** — Mouse / Touch / Key 的分层、分发与滚动设计（C 侧）
2. **[组件 → JavaScript 自定义事件](#组件自定义事件标准流程必读)** — JSON `@handler`、桥接与 payload 约定

相关源码：`src/event.c`、`src/event.h`、`src/ytype.h`、`src/backend/backend_sdl.c`。

---

## 原生输入事件架构

### 设计目标

| 原则 | 说明 |
|------|------|
| **event.c 只做分发** | 层树遍历、popup 优先级、焦点；**不识别**滚轮、拖动、swipe 等业务语义 |
| **双设备独立** | Mouse 与 Touch 是两种输入设备，各自完整支持 |
| **语义在组件** | 滚动、点击、手势由 View / List / Button 等在 hook 内处理 |
| **写 offset 单核** | 最终滚动偏移统一经 `layout_scroll_vertical` / `layout_scroll_horizontal`（`src/layout.c`） |
| **Backend 只适配** | SDL / 平台输入 → `PointerEvent`，不做 scroll 业务 |

### 分层

```
┌─────────────────────────────────────────────────────────┐
│  Backend (backend_sdl.c / backend_lvgl.c / mobile)     │
│  SDL Mouse / Finger / Wheel / Key → 原始事件包           │
└──────────────────────────┬──────────────────────────────┘
                           │
         ┌─────────────────┴─────────────────┐
         ▼                                   ▼
  handle_pointer_event()              handle_key_event()
         │                                   │
         ▼                                   ▼
  event.c：层树分发 + popup + 焦点          （同上，纯分发）
         │                                   │
         ▼                                   ▼
  layer->handle_pointer_event        组件默认 / 自定义
         │                                   │
         └─────────────┬─────────────────────┘
                       ▼
              layout_scroll_* / 点击 / 拖拽 …
```

### 对外 API

`src/event.h` 统一为指针事件入口：

```c
int handle_pointer_event(Layer* layer, PointerEvent* event);
void handle_key_event(Layer* layer, KeyEvent* event);
const PointerEvent* get_current_pointer_event(void);
```

Backend **仅**调用 `handle_pointer_event` / `handle_key_event`，例如：

```c
PointerEvent pe = { .device = POINTER_DEVICE_MOUSE, .phase = POINTER_DOWN, ... };
handle_pointer_event(root, &pe);
```

**不再**对外暴露独立的 scroll / scrollbar 顶层 API（如历史上的 `handle_scroll_event`）。滚轮用 `phase == POINTER_WHEEL` + `wheel_dx/dy`；内容区拖动用 `POINTER_MOVE` + `delta_x/y`。

### 事件结构：PointerEvent（`src/ytype.h`）

Mouse 与 Touch 已合并为单一结构：

```c
typedef enum { POINTER_DEVICE_MOUSE, POINTER_DEVICE_TOUCH } PointerDevice;

typedef enum {
    POINTER_DOWN, POINTER_MOVE, POINTER_UP, POINTER_WHEEL,
    POINTER_SWIPE, POINTER_DOUBLE_TAP, POINTER_LONG_PRESS,
    POINTER_PINCH, POINTER_ROTATE, POINTER_CANCEL,
} PointerPhase;

typedef struct PointerEvent {
    PointerDevice device;
    PointerPhase phase;
    int x, y;
    int delta_x, delta_y;
    int wheel_dx, wheel_dy;
    Uint8 button;
    int clicks;
    int finger_count;
    float scale, rotation;
    Uint32 timestamp;
} PointerEvent;
```

#### phase 与 SDL 状态对应

| SDL 鼠标 | `PointerEvent` |
|----------|----------------|
| `SDL_PRESSED` | `phase == POINTER_DOWN` |
| `SDL_MOUSEMOTION` | `phase == POINTER_MOVE` |
| `SDL_RELEASED` | `phase == POINTER_UP` |
| 滚轮 | `phase == POINTER_WHEEL`，`wheel_dx/dy` |
| 手势（swipe/pinch…） | `phase == POINTER_SWIPE` 等，`device == POINTER_DEVICE_TOUCH` |

组件 hook 读 `event->phase`、`event->delta_x/y`；Layer 字段为 `handle_pointer_event`。

### event.c 职责（目标态）

**保留：**

- 子层 → `sub` → 本层 hook 的递归分发（top-down hit）
- root 层 popup 优先（`popup_manager_handle_mouse_event` 等）
- 焦点切换（`focused_layer`）
- 调用 `layer->handle_mouse_event` / `handle_touch_event` / `handle_key_event`
- JS 回调触发入口（如 `layer->event->touch`）

**不包含：**

- `default_scrollable_*`（滚轮 / 按住拖内容的默认逻辑）
- `process_layer_scrollbar` / `handler_virtical_scroll_event` 等 scroll 实现
- 对 `wheel_dy`、`TOUCH_TYPE_MOVE`、`scrollable` 的特殊分支

目标伪代码：

```c
int handle_mouse_event(Layer* layer, MouseEvent* e) {
    if (layer->parent == NULL && popup_...) return 1;
    for (child top→bottom)
        if (handle_mouse_event(child, e)) return 1;
    focus_logic(layer, e);
    if (layer->sub && handle_mouse_event(layer->sub, e)) return 1;
    if (layer->handle_mouse_event && layer->handle_mouse_event(layer, e)) return 1;
    return default_layer_handle_mouse_event(layer, e);  // 可选：通用 hover/click
}

int handle_touch_event(Layer* layer, TouchEvent* e) {
    for (child...) if (handle_touch_event(child, e)) return 1;
    if (layer->sub && handle_touch_event(layer->sub, e)) return 1;
    if (layer->handle_touch_event && layer->handle_touch_event(layer, e)) return 1;
    if (layer->event->touch) { invoke; return 1; }
    return 0;
}
```

### 滚动语义：并入 Mouse / Touch，废弃 `handle_scroll_event`

历史上存在 **`Layer->handle_scroll_event(layer, scroll_delta)`** 单独回调（List / Select / Dialog 用于滚轮）。设计上应**并入 `handle_mouse_event` 的滚轮分支**，不再保留第三种 Layer hook。

| 用户动作 | 设备 | 入口 | 组件内识别 |
|----------|------|------|------------|
| 滚轮 | Mouse | `handle_mouse_event` | `event->wheel_dy` / `wheel_dx` |
| 鼠标按住拖内容 | Mouse | `handle_mouse_event` | `state == MOTION` 且 `delta_x/y` |
| 手指拖内容 | Touch | `handle_touch_event` | `type == MOVE` 且 `deltaX/Y` |
| 拖滚动条拇指 | Mouse | `handle_mouse_event` | 由 Scrollbar / View 在 hook 内处理 |
| ~~滚轮~~ | ~~Layer->handle_scroll_event~~ | **废弃** | 逻辑迁入 `handle_mouse_event` |

组件内推荐模式：

```c
int view_handle_mouse(Layer* layer, MouseEvent* e) {
    if (e->wheel_dy || e->wheel_dx)
        return view_scroll_wheel(layer, e);
    if (e->state == SDL_MOUSEMOTION && (e->delta_x || e->delta_y))
        return scroll_drag_vertical(layer, e->delta_x, e->delta_y);
    return 0;
}

int view_handle_touch(Layer* layer, TouchEvent* e) {
    if (e->type == TOUCH_TYPE_MOVE)
        return scroll_drag_vertical(layer, e->deltaX, e->deltaY);
    ...
}
```

共享滚动核（可放在 `src/layout.c` 或 `src/scroll.c`）：

- `layout_scroll_vertical(layer, delta_y)` — 像素级拖动（已有）
- 滚轮步进：`scroll_delta * 20` 等手感参数在**组件**内换算后再调 `layout_scroll_*`

List 迁移示例：

```c
// 原 list_component_handle_scroll_event(wheel_dy) → 并入 list_handle_mouse
if (e->wheel_dy) {
    layout_scroll_vertical(layer, -e->wheel_dy * 20);
    return 1;
}
// list_component_handle_touch_event 保留 MOVE 拖动逻辑
```

### Backend 职责

| 平台 | Mouse | Touch |
|------|-------|-------|
| 桌面 SDL | 真实鼠标 → `MouseEvent` | 真实手指 → `TouchEvent` |
| Web (Emscripten) | 鼠标 → `MouseEvent`（含 `delta_*` 拖动） | 手指 → `TouchEvent`；合成 mouse 仅用于 Button 等点击，**不带 content drag delta** |
| LVGL SDL | 同上 | 手指可合成 mouse（`from_touch` 已废弃，用 delta=0 区分） |

Hint 策略（`backend_sdl.c`）：

```c
SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");
SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
```

鼠标与触摸**分轨**，避免同一次拖动既走 mouse 又走 touch 导致双滚。

Backend 填 `MouseEvent` 示例：

```c
// 滚轮帧
mouse_event.wheel_dx = event->wheel.x;
mouse_event.wheel_dy = -event->wheel.y;
handle_mouse_event(root, &mouse_event);

// 按住拖动帧
mouse_event.delta_x = mouse_x - pointer_last_x;
mouse_event.delta_y = mouse_y - pointer_last_y;
handle_mouse_event(root, &mouse_event);
```

### Layer hook 收敛（目标）

| Hook | 状态 | 用途 |
|------|------|------|
| `handle_mouse_event` | 保留 | 点击、hover、滚轮、拖内容、滚动条 |
| `handle_touch_event` | 保留 | 点击、拖内容、手势 |
| `handle_key_event` | 保留 | 键盘 / IME |
| `handle_scroll_event` | **废弃** | 滚轮逻辑并入 `handle_mouse_event` |

### 迁移步骤（KISS）

1. 抽共享 `scroll_drag_*` / `scroll_wheel_*`（或继续用 `layout_scroll_vertical`）
2. View / Grid：`scrollable` 图层注册 `handle_mouse_event` / `handle_touch_event`
3. List / Select / Dialog：`handle_scroll_event` → 并进 `handle_mouse_event(wheel)`
4. Popup：滚轮改走 `popup_manager_handle_mouse_event` + 各层 mouse hook
5. 从 `event.c` 删除默认 scroll / scrollbar 逻辑，仅保留分发
6. 删除 `Layer->handle_scroll_event` 字段（`ytype.h`）

### 实现状态（截至当前分支）

| 项 | 状态 |
|----|------|
| 滚轮 / 滚动条 / 默认 scrollable 在 `event.c` | 过渡实现，待迁入 View/List 等组件 |
| `handle_scroll_event` 顶层 API | 已改为 `event.c` 内部 static |
| `Layer->handle_scroll_event` | 仍存在，List/Select/Dialog 使用，待迁移后删除 |
| Backend 只调 mouse/touch | 已达成 |
| `MouseEvent.delta_*` / `wheel_*` | 已定义，backend 已填充 |

---

## 组件自定义事件标准流程（必读）

新增组件事件时**必须**按此流程实现，**不要**另建 `graph_event`、`layer_emit_event`、独立 JS dispatch 等旁路。

### 架构原则

1. **解耦**：C 组件只负责业务逻辑与触发时机，不直接调用 JavaScript。
2. **延迟绑定**：JSON 加载时只保存 handler 名称（`@funcName`）；真正触发时再 `find_event_by_name` 查找已注册的 C 桥接函数。
3. **统一桥接**：`lib/jsmodule/js_common.c` 在解析 JSON 的 `events` 时，把 `layerId.onXxx` → `@handler` 注册进全局事件表；组件触发时走同一条链路。

参考实现：`Input` / `Select` 的 `onChange`，`List` 的 `onSelect`，`Sash` 的 `onChange`，`Connector` 的 `onConnectChange`，`Draggable` 的 `onDragChange`。

### 1. 组件结构体：存 handler 名

```c
typedef struct MyComponent {
    Layer* layer;
    char on_foo_name[MAX_PATH];   // 去掉 @ 后的 JS 函数名，如 "onMyFoo"
    // 可选：EventHandler on_foo;  // 仅部分组件缓存 handler（如 Sash）
} MyComponent;
```

### 2. `create`：挂 `register_event`

```c
layer->register_event = my_component_register_event;
```

### 3. `create_from_json`：解析 `events`

```c
cJSON* events = cJSON_GetObjectItem(json_obj, "events");
cJSON* item = events ? cJSON_GetObjectItem(events, "onMyFoo") : NULL;
if (item && cJSON_IsString(item) && item->valuestring[0]) {
    const char* name = item->valuestring;
    if (name[0] == '@') name++;
    strncpy(comp->on_foo_name, name, sizeof(comp->on_foo_name) - 1);
}
```

### 4. `register_event`：支持 `YUI.setEvent` 动态绑定

```c
int my_component_register_event(Layer* layer, const char* event_name,
                                const char* event_func_name, EventHandler event_handler)
{
    MyComponent* comp = (MyComponent*)layer->component;
    (void)event_handler;
    if (strcmp(event_name, "onMyFoo") != 0) return -1;
    if (!event_func_name) return -1;
    if (event_func_name[0] == '@') event_func_name++;
    strncpy(comp->on_foo_name, event_func_name, sizeof(comp->on_foo_name) - 1);
    return 0;
}
```

### 5. 触发：标准四步（与 `list_component` / `Connector` 一致）

```c
#include "../event.h"

static void my_component_emit_foo(Layer* layer, MyComponent* comp, const char* payload_json)
{
    EventHandler handler;

    if (!comp->on_foo_name[0]) return;

    handler = find_event_by_name(comp->on_foo_name);
    if (!handler) return;

    /* 让 js_module_common_event 能找到要调用的 JS 函数名 */
    if (!layer->event) {
        layer->event = (Event*)calloc(1, sizeof(Event));
    }
    if (layer->event) {
        strncpy(layer->event->click_name, comp->on_foo_name,
                sizeof(layer->event->click_name) - 1);
    }

    /* payload 写入 layer->text，JS 侧用 YUI.getText(layerId) 读取 */
    layer_set_text(layer, payload_json);

    EVENT_INVOKE(handler, layer);
}
```

**禁止：**

- 在 `src/` 里 `#include` jsmodule 或调用 `js_module_*`
- 复用 `layer_lifecycle_js_dispatch` 派发业务事件
- 新增全局 `getEventDetail()` 一类 API（payload 走 `layer->text`）

### 6. JSON 注册（页面配置）

```json
{
  "id": "myLayer",
  "type": "MyType",
  "events": {
    "onMyFoo": "@onMyFooHandler"
  }
}
```

加载后 `js_common.c` 会注册映射：`myLayer.onMyFoo` → `onMyFooHandler`，并 `register_event_handler("onMyFooHandler", js_module_common_event)`。

### 7. JavaScript handler

```javascript
function onMyFooHandler() {
  var detail = JSON.parse(YUI.getText("myLayer"));
  YUI.log(detail);
}
```

handler 是**无参全局函数**；当前触发图层 id 需由 handler 自行约定（每个图层独立 handler），或从 `YUI.getText(已知layerId)` 读取刚写入的 payload。

**测试页示例**：`app/tests/test-connector.js`（`showConnectEvent` / `showDragEvent`）。

---

## 端到端数据流（JS 桥接）

```
JSON:  "events": { "onChange": "@onTextChange" }
         ↓ parse (create_from_json)
组件:  comp->change_name = "onTextChange"
         ↓ js_module_load_from_json
js_common:  register "layerId.onChange" + register_event_handler("onTextChange", js_module_common_event)
         ↓ 用户操作
组件:  layer_set_text(layer, json) + EVENT_INVOKE(handler, layer)
         ↓
js_module_common_event(layer)
         ↓ js_module_trigger_event / js_module_call_event
JavaScript:  onTextChange()  →  JSON.parse(YUI.getText("layerId"))
```

---

## 标准事件类型（内置 JS）

`js_common.c` 的 `get_event_handler_by_type` 已内置：

| event_type | C 桥接 |
|------------|--------|
| `onClick` / `click` | `js_module_click_event` → `js_module_call_layer_event(id, "onClick")` |
| `onPress` / `press` | `js_module_press_event` |
| `onScroll` / `scroll` | `js_module_scroll_event` |
| `onTouch` / `touch` | `js_module_touch_event` |
| `onChange` / `change` | `js_module_change_event` |
| `onResize` / `resize` | 特殊处理（见 `js_module_set_layer_event`） |
| `onLoad` / `onShow` / `onHide` / `onUnload` | `layer_lifecycle`（**仅页面生命周期，不用于组件业务事件**） |

**非标准事件**（如 `onConnectChange`、`onDragChange`、`onSelect`）：走上面的「组件四步」+ `find_event_by_name`，**不要**走 lifecycle dispatch。

---

## 参考文件

| 场景 | 文件 |
|------|------|
| 输入分发（目标：纯路由） | `src/event.c`、`src/event.h` |
| 事件结构 | `src/ytype.h`（`MouseEvent`、`TouchEvent`、`KeyEvent`） |
| SDL Backend 适配 | `src/backend/backend_sdl.c` |
| LVGL Backend 适配 | `src/backend/backend_lvgl.c` |
| 滚动偏移写入 | `src/layout.c`（`layout_scroll_vertical`） |
| 带 JSON payload 的自定义事件 | `src/components/list_component.c`（`list_dispatch_select`） |
| 滚轮（待迁入 mouse hook） | `src/components/list_component.c`（`list_component_handle_scroll_event`） |
| `onChange` + `register_event` | `src/components/sash_component.c` |
| 图连线事件 | `src/components/connector_component.c`（`connector_emit_connect_change`） |
| 节点拖动事件 | `src/components/draggable_component.c`（`draggable_emit_drag_change`） |
| JSON 事件注册 | `lib/jsmodule/js_common.c`（`scan_and_register_events`） |
| 全局 handler 表 | `src/event.c`（`register_event_handler` / `find_event_by_name`） |

---

## Text / Select 的 onChange（历史说明）

```
JSON: "onChange": "@onTextChange"
  → create_from_json 存 change_name
  → 文本变化时 text_component_trigger_on_change()
  → find_event_by_name + component->on_change(layer)
```

与自定义事件相同理念；部分老组件在结构体里缓存 `EventHandler on_change`，新组件可只存 `*_name` 并在触发时 `find_event_by_name`。
