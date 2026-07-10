# Sash 组件

可拖拽的分隔条，用于调整相邻面板的大小。支持垂直（上下）和水平（左右）两种方向。

## JSON 配置

```json
{
  "id": "mySash",
  "type": "Sash",
  "size": [660, 6],
  "target": "panelAboveOrLeft",
  "minSize": 60,
  "orientation": "vertical",
  "style": {
    "bgColor": "#313244"
  }
}
```

## 属性

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `target` | string | — | **必填**。要被 sash 调整大小的目标图层 ID。对面图层自动从 children 位置推断 |
| `minSize` | int | 60 | 目标图层的最小尺寸（像素），拖拽不会小于此值 |
| `orientation` | string | `"vertical"` | 拖拽方向。`"vertical"` 上下调整高度，`"horizontal"` 左右调整宽度 |
| `size` | [w, h] | — | sash 自身的尺寸。vertical 时通常高 4~8px；horizontal 时通常宽 4~8px |

## 布局规则

Sash 放在两个面板之间，`target` 指向**上方（vertical）或左侧（horizontal）**的面板，对面面板自动识别为 children 数组中紧接在 sash 后面的组件。

### 垂直布局示例

```json
{
  "type": "View",
  "layout": { "type": "vertical", "spacing": 0 },
  "children": [
    { "id": "topPanel", "type": "View", "size": [660, 200] },
    { "id": "sash", "type": "Sash", "size": [660, 6], "target": "topPanel" },
    { "id": "bottomPanel", "type": "View", "size": [660, 300] }
  ]
}
```

- 拖动 sash 上下移动
- `topPanel` 高度跟随变化
- `bottomPanel` 高度反向调整，保持总高度不变
- `topPanel` 不会小于 `minSize`；`bottomPanel` 至少保留 20px

### 水平布局示例

```json
{
  "type": "View",
  "layout": { "type": "horizontal", "spacing": 0 },
  "children": [
    { "id": "leftPanel", "type": "View", "size": [200, 400] },
    { "id": "sash", "type": "Sash", "size": [6, 400], "target": "leftPanel", "orientation": "horizontal" },
    { "id": "rightPanel", "type": "View", "size": [400, 400] }
  ]
}
```

- 拖动 sash 左右移动
- `leftPanel` 宽度跟随变化
- `rightPanel` 宽度反向调整

## 子面板自适应

被 sash 调整的容器内部子组件如需自动填充剩余空间，应设置 `"flex": 1`：

```json
{
  "id": "topPanel",
  "type": "View",
  "size": [660, 200],
  "layout": { "type": "vertical" },
  "children": [
    { "id": "toolbar", "type": "View", "size": [660, 32] },
    { "id": "content", "type": "Text", "size": [660, 140], "flex": 1 }
  ]
}
```

不加 `flex` 时子组件保持固定尺寸，容器 resize 后会出现空白或内容被裁剪。

## 视觉

| 状态 | 背景色 | 拖拽点 |
|------|--------|--------|
| 默认 | `#313244` | 灰色三个点 |
| 悬停 | 稍亮 | 亮色三个点 + 蓝色强调线 |
| 拖拽中 | 保持悬停样式 | — |
