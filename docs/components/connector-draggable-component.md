# Connector / Draggable 组件

用于构建**节点图、流程图、数据管道**等可拖拽画布：`Draggable` 提供可移动节点与连接端口，`Connector` 绘制节点之间的贝塞尔连线，并支持鼠标交互创建、修改与删除连线。

完整示例见 `app/tests/test-connector.json`、`app/tests/test-connector.js`。

## 架构概览

```
graphCanvas (View, layout: absolute)
├── Connector...          ← 连线层，插在子节点最前（绘制在节点下方）
├── Draggable (nodeA)     ← 可拖节点
│   ├── ...子组件
│   └── *_dots (自动)     ← 端口圆点与连线交互层
├── Draggable (nodeB)
└── _connector_pick (自动) ← 透明顶层，用于点击线条修改端点
```

| 组件 | 职责 |
|------|------|
| **Draggable** | 节点拖拽、标题栏拖动手柄、端口圆点显示与鼠标按下 |
| **Connector** | 贝塞尔曲线渲染、端点解析、创建/删除/修改连线、拖动预览 |
| **connectable** | 任意子图层上的通用属性，标记该层可作为连线端点 |

`layer.c` 仅提供通用的 `connectable` 与 `layer_resolve_path`，不依赖具体组件实现。

## 基本用法

### 画布 + 静态连线 + 节点

```json
{
  "id": "graphCanvas",
  "type": "View",
  "size": [800, 500],
  "layout": { "type": "absolute" },
  "children": [
    {
      "id": "edgeAB",
      "type": "Connector",
      "position": [0, 0],
      "size": [0, 0],
      "from": { "id": "nodeA", "anchor": "right" },
      "to": { "id": "nodeB", "anchor": "left" },
      "style": {
        "color": "#89b4fa",
        "strokeWidth": 2
      }
    },
    {
      "id": "nodeA",
      "type": "Draggable",
      "position": [40, 50],
      "size": [200, 80],
      "dragHandleHeight": 36,
      "style": { "bgColor": "#313244", "borderRadius": 8 },
      "children": [
        {
          "type": "Label",
          "text": "节点 A",
          "position": [12, 44],
          "style": { "color": "#cdd6f4" }
        }
      ]
    },
    {
      "id": "nodeB",
      "type": "Draggable",
      "position": [320, 120],
      "size": [120, 56],
      "style": { "bgColor": "#313244", "borderRadius": 8 }
    }
  ]
}
```

要点：

- 画布容器使用 **`layout.type: "absolute"`**，节点用 `position` 定位。
- `Connector` 的 `position` / `size` 可为 `[0, 0]`，几何由端点动态计算。
- 同一端口允许多条连线（每次拖放新建一条，不会替换已有连线）。
- 多条线共用同一锚点时，端点会自动**扇出偏移**，避免完全重叠。
- 点击端口圆点优先于点击线条，便于在已有连线上继续拖出新连线。

### 子组件作为端口（connectable）

`Draggable` 自身默认可连接（左右端口）。其子组件需显式设置 `connectable: true` 才会出现端口圆点：

```json
{
  "id": "nameInput",
  "type": "Input",
  "connectable": true,
  "size": [0, 32]
}
```

跨节点连接子端口时，`from` / `to` 的 `id` 支持**点分路径**（从 UI 根解析）：

```json
{
  "id": "edgeInputToPort",
  "type": "Connector",
  "from": { "id": "nodeA.nameInput", "anchor": "right" },
  "to": { "id": "outputPort", "anchor": "left" }
}
```

也可简写为字符串（锚点默认 `center`）：

```json
"from": "nodeA",
"to": "nodeB"
```

## 鼠标交互

| 操作 | 效果 |
|------|------|
| 按住节点**标题栏**拖动 | 移动 `Draggable` 节点，连线自动跟随 |
| **左键**按住端口圆点拖到另一端口 | 创建新连线（同一端口可连多个不同目标） |
| **左键**按住连线拖动 | 移动该线较近的一端；拖动时原线隐藏，仅显示预览线 |
| **右键**点击端口圆点 | 删除该端口上的**所有**连线 |
| **右键**点击连线附近 | 删除该条连线 |
| 修改连线时松手到空白处 | 取消修改，恢复原连线 |

端口圆点由 `Draggable` 自动维护（`show_dots` 开启时）。每条 `Connector` 对应独立图层，修改时只影响被点击的那一条。

## Connector 属性

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `from` | string \| object | — | 起点。对象形式：`{ "id": "...", "anchor": "right" }` |
| `to` | string \| object | — | 终点，格式同 `from` |
| `curve` | string | `"auto"` | 曲线模式，见下表 |
| `controlPoints` | array | — | 手动控制点 `[[x1,y1],[x2,y2]]`，设置后等价于 `manual` |
| `style.color` | color | `#888888` | 线条颜色（也可用 `strokeColor`） |
| `style.strokeWidth` | number | `2` | 线宽 |

### anchor 取值

`center` · `top` · `bottom` · `left` · `right`

未指定时默认为 `center`。节点级连线常用 `left` / `right`；垂直连线可用 `top` / `bottom`。

### curve 取值

| 值 | 说明 |
|----|------|
| `auto` | 水平方向自动贝塞尔控制点（默认） |
| `auto-vertical` | 垂直方向自动贝塞尔控制点 |
| `manual` | 使用 `controlPoints` 中的两个控制点 |

示例（垂直连线）：

```json
{
  "from": { "id": "nodeA", "anchor": "bottom" },
  "to": { "id": "nodeC", "anchor": "bottom" },
  "curve": "auto-vertical",
  "style": { "color": "#f9e2af", "strokeWidth": 2 }
}
```

## Draggable 属性

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `dragHandleHeight` | number | `36` | 顶部拖动手柄高度（px）；`0` 时不绘制手柄条 |
| `style.dots` | boolean | `true` | 是否显示连接端口圆点 |
| `style.dotSize` | number | `4` | 圆点半径 |
| `style.dotColor` | color | `#89b4fa` | 圆点颜色 |
| `style.bgColor` 等 | — | — | 节点背景、圆角等通用样式 |

`Draggable` 创建时会将自身布局设为 `absolute`，以便在画布内自由定位。

### connectable（通用图层属性）

任意图层 JSON 均可设置：

```json
"connectable": true
```

规则：

- `Draggable` 类型**始终**可连接，无需设置。
- 其他类型（`Input`、`View` 等）需 `connectable: true` 才会在端口列表中出现，并显示左右默认锚点。

## JavaScript 动态操作

### 添加连线

```javascript
YUI.update({
  target: "graphCanvas",
  change: {
    children: [{
      id: "edgeAC",
      type: "Connector",
      position: [0, 0],
      size: [0, 0],
      from: { id: "nodeA", anchor: "bottom" },
      to: { id: "nodeC", anchor: "bottom" },
      curve: "auto-vertical",
      style: { color: "#f9e2af", strokeWidth: 2 }
    }]
  }
});
```

### 移动节点

```javascript
YUI.update({
  target: "nodeA",
  change: { position: [120, 80] }
});
```

连线端点会随布局刷新自动重算，无需手动更新 `Connector`。

### 删除连线 / 节点

```javascript
YUI.update({
  target: "graphCanvas",
  change: { "children.edgeAC": null }
});
```

用户拖放创建的连线 id 为 `edge_drag_N`（自动递增），也可同样用 `children.edge_drag_1: null` 删除。

### 动态添加节点

参考 `test-connector.js` 中的 `addDynamicNode()`：向 `graphCanvas.children` 追加 `Draggable` 子树，子 `Input` 设置 `connectable: true` 即可参与连线。

## C API 摘要

头文件：`src/components/connector_component.h`、`src/components/draggable_component.h`

```c
/* 端点解析：全局 id 或点分路径 "nodeA.nameInput" */
Layer* connector_resolve_endpoint(const char* id_or_path);

/* 是否可作为连线端点 */
int connector_layer_is_connectable(Layer* layer);

/* 从端口开始拖线创建（一般由 Draggable dot_overlay 调用） */
int connector_interaction_start(Layer* from_layer, ConnectorAnchor from_anchor,
                                int dot_size, int mouse_x, int mouse_y);

/* 右键删除：命中端口或曲线 */
int connector_try_remove_at(Layer* canvas, int x, int y, int dot_size);
```

`connector_create_link()` 在内部用于拖放完成时创建连线图层，通常无需应用层直接调用。

## 布局与层级建议

1. **一个画布一个绝对布局容器**，所有 `Connector` 与 `Draggable` 同为该容器的直接子节点。
2. `Connector` 自动插入到子节点列表**最前**，保证线条在节点下方；`_connector_pick` 透明层保持在**最后**，用于线条命中。
3. 节点移动后调用 `layout_layer`（`YUI.update` 会自动触发）即可刷新连线几何。
4. 端口圆点绘制在 `Draggable` 内部的 `dot_overlay` 上，避免被节点内容裁剪，并保证交互在最上层。

## 常见问题

### 子组件拖不动 / 点不到端口？

- 拖动节点应点在**标题栏区域**（高度由 `dragHandleHeight` 决定），不是整个节点。
- 确认 `style.dots` 未设为 `false`。
- 子组件需 `connectable: true` 才有独立端口。

### 连线找不到端点？

- 检查 `from.id` / `to.id` 是否在 UI 树中存在。
- 跨层级子组件使用点分路径，如 `nodeA.nameInput`。
- 端点图层必须 `connectable`（或类型为 `Draggable`）。

### 移动节点后连线没跟上？

- 确认 `Connector` 与 `Draggable` 在同一画布容器下。
- 通过 `YUI.update` 改 `position` 会触发布局；直接改 C 层 rect 后需 `layout_layer(canvas)`。

### 多条线连同一端口？

- 支持。左键从端口拖出每次新建一条；右键端口会删除该端口上的全部连线。

## 事件

> **标准流程必读**：[event.md — 组件自定义事件标准流程](../event.md#组件自定义事件标准流程必读)  
> 与 `List` / `Sash` / `Input` 相同：`find_event_by_name` + `layer_set_text(payload)` + `EVENT_INVOKE`，**不要**另建 dispatch 层。

### 已实现

| 组件 | 事件 | 触发时机 |
|------|------|----------|
| `Connector` | `onConnectChange` | `attach` 新建、`detach` 删除、`rebind` 拖线改端点 |
| `Draggable` | `onDragChange` | 标题栏拖动 `start` / `end` |

未在 JSON 绑定 `events` 的图层（如拖放新建的 `edge_drag_N`）不会触发。

### JSON 注册

```json
{
  "id": "edgeAB",
  "type": "Connector",
  "from": { "id": "nodeA", "anchor": "right" },
  "to": { "id": "nodeB", "anchor": "left" },
  "events": {
    "onConnectChange": "@onEdgeABEvent"
  }
},
{
  "id": "nodeA",
  "type": "Draggable",
  "events": {
    "onDragChange": "@onNodeADrag"
  }
}
```

### JavaScript handler（读 payload）

payload 写在**触发图层的 `text`**，用 `YUI.getText(layerId)` 读取（与 `List.onSelect` 相同）：

```javascript
function onEdgeABEvent() {
  var d = JSON.parse(YUI.getText("edgeAB"));
  // d.action: "attach" | "detach" | "rebind"
  // d.from / d.to: { id, anchor }
}

function onNodeADrag() {
  var d = JSON.parse(YUI.getText("nodeA"));
  // d.phase: "start" | "end"
  // d.position / d.previous: [x, y]
}
```

测试页带 `eventLabel` 实时显示：`app/tests/test-connector.json`、`app/tests/test-connector.js`。

### Payload 结构（当前实现）

**`onConnectChange`**（`YUI.getText(connectorLayerId)`）：

```json
{
  "action": "attach",
  "from": { "id": "nodeA", "anchor": "right" },
  "to": { "id": "nodeB", "anchor": "left" }
}
```

**`onDragChange`**（`YUI.getText(draggableLayerId)`）：

```json
{
  "phase": "start",
  "position": [40, 50],
  "previous": [40, 50]
}
```

### C 层触发点

| 代码路径 | 事件 | action / phase |
|----------|------|----------------|
| `connector_create_link()` | `onConnectChange` | `attach` |
| `connector_remove_child_layer()`（CONNECTOR） | `onConnectChange` | `detach` |
| MODIFY 松手成功 | `onConnectChange` | `rebind` |
| `draggable` 标题栏 PRESSED | `onDragChange` | `start` |
| `draggable` 标题栏 RELEASED | `onDragChange` | `end` |

实现见 `connector_emit_connect_change` / `draggable_emit_drag_change`（`connector_component.c`、`draggable_component.c`）。

### 待扩展（未实现）

| 能力 | 说明 |
|------|------|
| `onDragChange` `move` | 拖动中节流 |
| `yui_update` 改 `position` | `phase: end`（程序移动） |
| 端口 / 节点冒泡 | 子 `connectable` 变更冒泡到 `Draggable` |
| `onGraphChange` / `onNodeMove` | 画布级聚合 |
| `connectable` 开关 | `enable` / `disable` action |
| `options.silent` | 静默更新防循环 |

---

*示例与自动化检查：`app/tests/test-connector.json`、`app/tests/test-connector.js`*
