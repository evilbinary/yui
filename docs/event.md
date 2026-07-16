# 事件系统

## 架构原则

1. **解耦**：C 组件只负责业务逻辑与触发时机，不直接调用 JavaScript。
2. **延迟绑定**：JSON 加载时只保存 handler 名称（`@funcName`）；真正触发时再 `find_event_by_name` 查找已注册的 C 桥接函数。
3. **统一桥接**：`lib/jsmodule/js_common.c` 在解析 JSON 的 `events` 时，把 `layerId.onXxx` → `@handler` 注册进全局事件表；组件触发时走同一条链路。

参考实现：`Input` / `Select` 的 `onChange`，`List` 的 `onSelect`，`Sash` 的 `onChange`，`Connector` 的 `onConnectChange`，`Draggable` 的 `onDragChange`。

---

## 组件自定义事件标准流程（必读）

新增组件事件时**必须**按此流程实现，**不要**另建 `graph_event`、`layer_emit_event`、独立 JS dispatch 等旁路。

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

## 端到端数据流

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

## 标准事件类型（内置）

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
| 带 JSON payload 的自定义事件 | `src/components/list_component.c`（`list_dispatch_select`） |
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
