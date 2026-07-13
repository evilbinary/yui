# Table 表格组件

## 概述

Table 组件用于展示多列结构化数据，支持表头、斑马纹、网格线、双向滚动、行选择与 `onItemClick` 事件。适用于查询结果、数据面板等场景。

与 `Grid`（布局容器）不同，`Table` 是数据驱动组件：通过 `columns` 定义列、`data` 填充行。

## 功能特性

- 多列表头与单元格对齐（`left` / `center` / `right`）
- 固定列宽（`width`）与弹性列宽（`flex`）
- `autoColumns`：根据首行对象字段自动生成列
- 垂直 / 水平滚动（`scrollable: 3`）
- 行 hover / 选中高亮
- `onItemClick` 行点击事件
- JS 运行时 `layer.data = rows` 更新数据

## 基本用法

### 固定列定义

```json
{
  "id": "userTable",
  "type": "Table",
  "size": [600, 300],
  "scrollable": 3,
  "columns": [
    { "key": "id", "title": "ID", "width": 60, "align": "right" },
    { "key": "name", "title": "名称", "flex": 1 },
    { "key": "email", "title": "邮箱", "width": 180 }
  ],
  "data": [
    { "id": 1, "name": "Alice", "email": "a@example.com" },
    { "id": 2, "name": "Bob", "email": "b@example.com" }
  ],
  "events": {
    "onItemClick": "@onRowSelected"
  },
  "style": {
    "bgColor": "#11111B",
    "color": "#CDD6F4",
    "fontSize": 13,
    "headerBgColor": "#181825",
    "headerColor": "#A6ADC8",
    "rowAltBgColor": "#1a1a24",
    "gridLineColor": "#313244"
  }
}
```

### 动态列（SQL 查询结果）

列名不固定时，使用 `autoColumns: true`，在首次赋值 `data` 时根据首行字段自动生成列：

```json
{
  "id": "resultTable",
  "type": "Table",
  "autoColumns": true,
  "scrollable": 3,
  "data": []
}
```

```javascript
var table = yui.find("resultTable");
table.data = [
  { id: 1, name: "users", rows: 120 },
  { id: 2, name: "orders", rows: 540 }
];
```

## 属性说明

### 组件属性

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `columns` | array | — | 列定义，见下表 |
| `data` | array | `[]` | 行数据，每项为对象 |
| `autoColumns` | boolean | `false` | 从首行对象推断列 |
| `rowHeight` | number | `28` | 数据行高度（px） |
| `headerHeight` | number | `32` | 表头高度（px） |
| `stripeRows` | boolean | `true` | 斑马纹隔行变色 |
| `showGridLines` | boolean | `true` | 显示网格线 |
| `scrollable` | number | `0` | `1` 垂直，`2` 水平，`3` 双向 |

### 列定义 `columns[]`

| 字段 | 类型 | 说明 |
|------|------|------|
| `key` | string | 行对象字段名，必填 |
| `title` | string | 表头文字，默认同 `key` |
| `width` | number | 固定列宽（px） |
| `flex` | number | 弹性权重，无 `width` 时生效，默认 `1` |
| `align` | string | `left` / `center` / `right` |

### 样式 `style`

| 字段 | 说明 |
|------|------|
| `bgColor` | 表格背景 / 默认行背景 |
| `color` | 单元格文字颜色 |
| `fontSize` | 字体大小 |
| `headerBgColor` | 表头背景 |
| `headerColor` | 表头文字颜色 |
| `rowAltBgColor` | 斑马纹行背景 |
| `gridLineColor` | 网格线颜色 |

## 事件

### onItemClick

点击数据行时触发（表头、滚动条区域不触发）。

JSON 配置：

```json
"events": {
  "onItemClick": "@onRowSelected"
}
```

JS 处理函数：

```javascript
function onRowSelected(layerId) {
    var layer = yui.find(layerId);
    if (!layer || !layer.text) return;
    var row = JSON.parse(layer.text);  // 当前行对象
    console.log("选中行:", row);
}
```

引擎在触发事件前将行 JSON 写入 `layer.text`，与 Menu、List 的项点击模式一致。

## JS API

```javascript
var table = yui.find("resultTable");

// 更新数据（列名变化且 autoColumns 为 true 时会重建列）
table.data = rows;

// 清空
table.data = [];
```

## 与 Grid / List / Treeview 的区别

| 组件 | 用途 |
|------|------|
| **Grid** | 子组件网格布局（计算器按键等），非数据表 |
| **List** | 单列虚拟列表 + `itemTemplate` |
| **Treeview** | 树形/层级数据 |
| **Table** | 多列结构化表格 |

## 设计说明

### 架构

Table 遵循 YUI 标准组件模式：

- `table_component.c`：`TableComponent` 状态、渲染、交互
- `Layer.on_data_update`：响应 `data` 变更
- `Layer.render`：表头 + 可见行绘制
- `Layer.handle_mouse_event`：行命中、滚动条穿透屏蔽

### 布局

```
┌──────────────────────────────────┬─┐
│  Header（固定，水平随滚动）        │█│
├──────┬───────────┬───────────────┤█│
│ col0 │   col1    │     col2      │█│
├──────┼───────────┼───────────────┤ │
│ ...  │   ...     │     ...       │ │
└──────┴───────────┴───────────────┴─┘
```

- 表头不参与垂直滚动，与数据区同步水平滚动
- 列宽：先分配 `width` 固定列，剩余空间按 `flex` 比例分配
- `content_width` / `content_height` 驱动引擎滚动条

### 数据流

```
JSON data / JS table.data = rows
        ↓
layer_set_data → on_data_update
        ↓
（autoColumns）从首行生成 columns
        ↓
table_component_update_content_size
        ↓
重绘
```

## 示例

- 单元测试：`app/tests/test-table.json`
- DB 编辑器查询结果：`app/db/db.json` 中 `resultTable`

运行测试：

```bash
./build/db app/tests/test-table.json
```

## 后续规划

- 列排序（点击表头）
- 键盘 ↑↓ 行导航
- `selectedRow` JS 只读属性
- 单元格文本省略号 + tooltip
- 大数据量虚拟滚动
