# Grid 组件

网格布局容器，用于计算器按键、图标墙、游戏棋盘等按行列排列子组件的场景。

与 `Table`（数据表）不同，Grid 通过显式 `children` 排列子组件，不绑定 `data`。

## 两种写法（等价）

### 方式 1：`type: "Grid"` 组件

```json
{
  "id": "buttons",
  "type": "Grid",
  "size": [320, 320],
  "columns": 4,
  "gap": 8,
  "children": [
    { "id": "btn7", "type": "Button", "text": "7" }
  ]
}
```

### 方式 2：任意容器 + `layout.type: "grid"`

```json
{
  "id": "buttons",
  "type": "View",
  "size": [320, 320],
  "layout": {
    "type": "grid",
    "columns": 4,
    "spacing": 8
  },
  "children": [
    { "id": "btn7", "type": "Button", "text": "7" }
  ]
}
```

`type: "Grid"` 内部会自动设置 `layout.type = grid`，与方式 2 走同一套布局算法。

## 属性

| 属性 | 位置 | 类型 | 说明 |
|------|------|------|------|
| `columns` | 顶层 或 `layout` | number | 列数，默认 `1` |
| `spacing` | `layout` | number | 单元格间距（px） |
| `gap` | 顶层 或 `layout` | number | 同 `spacing` 的别名 |
| `padding` | `layout` 或 `style` | number / array | 内边距 |

子组件按 `children` 数组顺序从左到右、从上到下填充网格。

## 增量更新

```json
{ "target": "buttons", "change": { "layout": { "type": "grid", "columns": 3, "spacing": 6 } } }
```

```json
{ "target": "photoGrid", "change": { "columns": 4 } }
```

## 与 Table 的区别

| | Grid | Table |
|--|------|-------|
| 子项来源 | 显式 `children` | `columns` + `data` 动态生成 |
| 典型场景 | 计算器、相册图标 | 订单列表、数据管理 |

## 示例

见 `app/tests/test-grid-component.json`、`app/tests/test-grid.json`、`app/calc/calc.json`。
