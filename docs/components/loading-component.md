# Loading 加载组件

## 概述

Loading 用于异步请求、页面切换等场景的**加载中**指示。组件自绘动画，不创建子 Layer，主循环每帧重绘即可流畅旋转/跳动。

支持两种样式：

| variant | 说明 | 典型尺寸 |
|---------|------|----------|
| `spinner`（默认） | 旋转圆环 | 32–64 px 正方形 |
| `dots` | 三点跳动 | 48–80 px 宽 |

可配合 `text` 在指示器下方显示「加载中...」等提示文案。

## 基本用法

### 旋转环（spinner）

```json
{
  "id": "loader",
  "type": "Loading",
  "size": [48, 48],
  "variant": "spinner",
  "color": "#2196f3",
  "strokeWidth": 4,
  "speed": 1.0
}
```

### 带文案

```json
{
  "id": "pageLoader",
  "type": "Loading",
  "size": [120, 72],
  "text": "加载中...",
  "variant": "spinner",
  "color": "#2196f3",
  "trackColor": "#e0e0e0",
  "strokeWidth": 4
}
```

### 三点跳动（dots）

```json
{
  "id": "dotsLoader",
  "type": "Loading",
  "size": [64, 32],
  "variant": "dots",
  "color": "#4caf50",
  "speed": 1.2
}
```

### 居中覆盖在容器内

```json
{
  "id": "content",
  "type": "View",
  "size": [400, 300],
  "layout": {"type": "center"},
  "children": [
    {
      "id": "loader",
      "type": "Loading",
      "size": [48, 48],
      "variant": "spinner",
      "color": "#2196f3"
    }
  ]
}
```

## 属性说明

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `variant` | string | `"spinner"` | `spinner` 旋转环 / `dots` 三点跳动 |
| `text` | string | — | 底部提示文字（可选） |
| `color` | string | `#2196f3` | 指示器主色，支持 `#RRGGBB` |
| `trackColor` | string | `#dcdcdc` | spinner 环底色 |
| `strokeWidth` | number | `4` | spinner 环粗细（像素） |
| `speed` | number | `1.0` | 动画速度倍率（`animationSpeed` 同义） |
| `size` | array | — | `[宽, 高]`，有 `text` 时建议高度 ≥ 56 |
| `style.color` | string | — | 同 `color`，写在 style 内亦可 |

颜色也可写在 `style` 中：

```json
{
  "id": "loader",
  "type": "Loading",
  "size": [40, 40],
  "style": {
    "color": "#ff9800",
    "trackColor": "#f5f5f5"
  }
}
```

## 增量更新

通过 `YUI.update()` 显示/隐藏或修改样式，无需重建整棵树。

```json
[
  {
    "target": "root",
    "change": {
      "children": [
        {
          "id": "loader",
          "type": "Loading",
          "size": [48, 48],
          "variant": "spinner",
          "text": "请稍候"
        }
      ]
    }
  }
]
```

请求结束后隐藏：

```json
{"target": "loader", "change": {"visible": false}}
```

或删除：

```json
{"target": "root", "change": {"children.loader": null}}
```

修改文案与颜色：

```json
{
  "target": "loader",
  "change": {
    "text": "正在保存...",
    "color": "#4caf50",
    "speed": 1.5
  }
}
```

## JavaScript 用法

```javascript
// 显示加载
YUI.update({
  target: "root",
  change: {
    children: [{
      id: "loader",
      type: "Loading",
      size: [48, 48],
      variant: "spinner",
      text: "加载中"
    }]
  }
});

// 隐藏
YUI.update({ target: "loader", change: { visible: false } });
```

## 实现说明

- 类型枚举：`LOADING`，JSON `"type": "Loading"`
- 源文件：`src/components/loading_component.c`
- 动画基于 `backend_get_ticks()`，在 `render` 中计算角度/相位
- 无交互事件，纯展示组件

## 相关文档

- [JSON 格式规范](../json-format-spec.md)
- [JSON 增量更新规范](../json-update-spec.md)
