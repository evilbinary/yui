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

> 设计稿，待评审后实现。沿用 YUI `events` + `@handler` 机制，详见 [event.md](../event.md)。

### 设计目标

**连接状态（`onConnectChange`）** — `Draggable`、`Connector`、`connectable: true` 子图层统一使用；payload 中 `action` 区分语义：

| 场景 | 示例 |
|------|------|
| 端口开关 | `connectable: true` ↔ `false`（`YUI.update`） |
| 新建连线 | 拖放端口、增量添加 `Connector` 子节点 |
| 删除连线 | 右键线/端口、`children.edgeX: null` |
| 修改端点 | 拖线改落点、`YUI.update` 改 `from`/`to` |

**节点拖动（`onDragChange`）** — 仅 `Draggable`；`phase` 区分 `start` / `move` / `end`；程序改 `position` 为 `phase: end` + `source: program`。

### 事件注册

```json
{
  "id": "nodeA",
  "type": "Draggable",
  "events": {
    "onConnectChange": "@onNodeAConnectChange",
    "onDragChange": "@onNodeADragChange"
  },
  "children": [
    {
      "id": "nameInput",
      "type": "Input",
      "connectable": true,
      "events": {
        "onConnectChange": "@onNameInputConnectChange"
      }
    }
  ]
},
{
  "id": "edgeAB",
  "type": "Connector",
  "from": { "id": "nodeA", "anchor": "right" },
  "to": { "id": "nodeB", "anchor": "left" },
  "events": {
    "onConnectChange": "@onEdgeABChange"
  }
}
```

画布级可选聚合：

```json
{
  "id": "graphCanvas",
  "events": {
    "onGraphChange": "@persistGraph",
    "onNodeMove": "@onCanvasNodeMove"
  }
}
```

`onNodeMove`：任意 `Draggable` 位置变更时触发（含拖放与 `YUI.update`），payload 与 `onDragChange` 相同并带 `nodeId`。

### 连接事件派发

| 层级 | 绑定对象 | 何时触发 |
|------|----------|----------|
| **连线层** | `Connector` | 本边创建 / 销毁 / `from`·`to` 变更 |
| **端口层** | 端点图层（`Draggable` 或 `connectable` 子组件） | 端口连上、断开、`connectable` 开关 |
| **节点层** | 所属 `Draggable` | 子树内任意端口变更（冒泡，`port` 标明具体端口） |

`nodeA.nameInput` 新建出边时触发顺序：`nameInput` → `nodeA`（冒泡）→ `edge_drag_N` → 对端 `nodeB` →（可选）`graphCanvas.onGraphChange`。

`Connector` 视角：`attach`（新建）、`detach`（删除）、`rebind`（改端点 / `YUI.update` 改 `from`·`to`）；仅改样式**不触发**。

### `action` 枚举（`onConnectChange`）

| action | 含义 |
|--------|------|
| `enable` / `disable` | `connectable` 开关 |
| `attach` | 新连线连到本端口 / Connector 创建 |
| `detach` | 连线移除 / Connector 销毁 |
| `rebind` | 端点从 A 改到 B（单次事件，不拆 detach+attach） |

### Payload（`YUI.getEventDetail()`）

连接变更：

```typescript
interface ConnectChangeDetail {
  action: "enable" | "disable" | "attach" | "detach" | "rebind";
  source: "user" | "program";
  canvasId: string;
  targetId: string;
  targetType: string;
  port?: { id: string; path?: string; anchor: string };
  connector?: { id: string; from: Endpoint; to: Endpoint };
  previous?: { connector?: { from: Endpoint; to: Endpoint } };
  connectable?: boolean;
}
```

拖动变更：

```typescript
interface DragChangeDetail {
  phase: "start" | "move" | "end";
  source: "user" | "program";
  targetId: string;
  canvasId?: string;
  nodeId?: string;           // 画布 onNodeMove 时
  position: [number, number];
  previous?: [number, number];
  delta?: [number, number];  // phase=move
}
```

**attach 示例**（`nodeA.onConnectChange`）：

```json
{
  "action": "attach",
  "source": "user",
  "canvasId": "graphCanvas",
  "targetId": "nodeA",
  "port": { "id": "nodeA", "anchor": "right" },
  "connector": {
    "id": "edge_drag_1",
    "from": { "id": "nodeA", "anchor": "right" },
    "to": { "id": "nodeB", "anchor": "left" }
  }
}
```

**rebind 示例**（`edgeAB.onConnectChange`）：

```json
{
  "action": "rebind",
  "source": "user",
  "targetId": "edgeAB",
  "connector": {
    "id": "edgeAB",
    "from": { "id": "nodeA", "anchor": "right" },
    "to": { "id": "nodeC", "anchor": "left" }
  },
  "previous": {
    "connector": {
      "from": { "id": "nodeA", "anchor": "right" },
      "to": { "id": "nodeB", "anchor": "left" }
    }
  }
}
```

### `onDragChange`

| phase | 触发时机 |
|-------|----------|
| `start` | 标题栏按下 |
| `move` | 拖动中（P1 节流） |
| `end` | 松手，或 `YUI.update` 改 `position` |

- 仅 `dragHandleHeight` 区域算拖动；拖线/点端口不触发。
- 拖动中刷新连线几何，**不**触发 `onConnectChange`。
- `phase: end` 在 `layout_layer` 之后触发。

```javascript
function onNodeADragChange(layer) {
  var d = JSON.parse(YUI.getEventDetail());
  if (d.phase === "start") {
    bringToFront(d.targetId);
    return;
  }
  if (d.phase === "end" && d.source === "user") {
    graphModel.setNodePosition(d.targetId, d.position);
  }
}

function onNodeAConnectChange(layer) {
  var d = JSON.parse(YUI.getEventDetail());
  if (d.action === "attach") graphModel.addEdge(d.connector);
  if (d.action === "detach") graphModel.removeEdge(d.connector.id);
  if (d.action === "rebind") graphModel.updateEdge(d.connector.id, d.connector, d.previous.connector);
}
```

### C 层触发点

| 代码路径 | 事件 |
|----------|------|
| `connector_create_link()` | `onConnectChange` attach |
| 删除 CONNECTOR | detach |
| MODIFY 松手 / `on_data_update` 改 `from`·`to` | rebind |
| `connectable` 属性更新 | enable / disable |
| `draggable` PRESSED / RELEASED（手柄） | `onDragChange` start / end |
| `draggable` MOTION | `onDragChange` move（节流） |
| `yui_update` 改 `position` | `onDragChange` end（`source: program`） |

**静默更新**（防循环）：

```json
{ "target": "nodeA", "change": { "position": [40, 50] }, "options": { "silent": true } }
```

### 实现结构（计划）

```
graph_event.c
  graph_emit_connect_change / link_attach / detach / rebind
  graph_emit_drag_change          // phase: start | move | end
```

`connectable: false` 默认只隐藏端口、**保留**已有连线；级联删边由业务在 handler 中处理。

### 实施阶段

| 阶段 | 内容 |
|------|------|
| **P0** | `graph_event`；attach/detach；`onDragChange` start/end；`YUI.getEventDetail()` |
| **P1** | rebind；`onDragChange` move；Draggable 冒泡；`onNodeMove` |
| **P2** | enable/disable；silent；`onGraphChange` |
| **P3** | `test-connector` 示例 |

---

*示例与自动化检查：`app/tests/test-connector.json`、`app/tests/test-connector.js`*
