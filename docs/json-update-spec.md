# JSON 增量更新规范

## 概述

通过简单的 JSON 格式增量更新 UI 组件，无需重新渲染整个 UI 树。

1、统一格式：{ "target": "id", "change": {...} }
2、路径支持：children.0、children.id、layerId.a.b.c
3、null 表示删除，不存在则添加，存在则更新

## 基本格式

### 单个更新 

直接指定 `target` ，target可以指定id 或者child.0 array  layerid.a.b.c，默认是如果存在属性则，默认更新，不存在则是添加，也可以对象为null则删除

```json
{
  "target": "layerId",
  "change":{
    "text": "新文本",
    "bgColor": "#ff0000",
    "visible": true
  }
}
```

### 批量更新

使用数组包含多个更新对象：

```json
[
  {
    
    "target": "button1",
    "change":{
        "text": "点击我",
        "bgColor": "#4caf50"
    }
  },
  {
    "target": "label1",
    "change":{
        "text": "状态：成功",
        "color": "#00ff00"
    }
  },
  {
    "target": "formatBtn",
    "change":{
        "type": "Button",
        "text": "格式化",
        "size": [70, 25],
        "style": {
            "bgColor": "#2196f3",
            "color": "#ffffff",
            "fontSize": 12,
            "borderRadius": 4
        }
    }
  }
]
```

## 支持的属性

| 属性 | 类型 | 示例 | 说明 |
|-----|------|------|------|
| `text` | string | `"新文本"` | 文本内容 |
| `label` | string | `"标签"` | 标签文本 |
| `color` | string | `"#ffffff"` | 文字颜色 |
| `bgColor` | string | `"#1e1e1e"` | 背景颜色 |
| `fontSize` | number | `16` | 字体大小 |
| `borderRadius` | number | `8` | 圆角半径 |
| `size` | array | `[200, 50]` | 宽度和高度 |
| `position` | array | `[100, 100]` | X 和 Y 坐标 |
| `visible` | boolean | `true` | 是否可见 |
| `enabled` | boolean | `true` | 是否启用 |

## 多属性更新

一次更新多个属性：

```json
{
  "target": "submitBtn",
  "change":{
    "text": "提交成功",
    "bgColor": "#4caf50",
    "enabled": false,
    "visible": true
  }
}
```

## 删除元素

### 方式1：使用 null

```json
{
  "target": "elementToRemove",
  "change":{
    "visible": null
  }
}
```

### 方式2：批量删除

```json
[
  { "target": "button1","change":{ "button1":null } },
  { "target": "label2","change":{ "button1":null } },
  { "target": "icon3","change":{ "button1":null } }
]
```

### 方式3：从父容器删除指定子元素

```json
{
  "target": "parentContainer",
  "change":{
    "children.0":null
  }
}
```

```json
{
  "target": "parentContainer",
   "change":{
    "children.id":null
  }
}
```

### 方式4：清空容器的所有子元素

```json
{
  "target": "listContainer",
  "change":{
    "children":null
  }
}
```

## JavaScript 调用

```javascript
// 方式1：单个更新
YUI.update({ target: "myButton", text: "已点击", bgColor: "#ff0000" });

// 方式2：批量更新
YUI.update([
  { target: "label1", text: "更新1" },
  { target: "label2", text: "更新2" }
]);

// 方式3：直接传递对象（自动序列化）
YUI.update({
  target: "status",
  text: "连接成功",
  color: "#4caf50",
  visible: true
});

// 方式4：删除元素
YUI.update({ target: "oldButton"});

// 方式5：批量删除
YUI.update([
  { target: "btn1" },
  { target: "btn2" }
]);

// 方式6：清空容器
YUI.update({ target: "listContainer"});
```

## C API

```c
// 应用 JSON 更新（自动识别单个或批量）
int yui_update(Layer* root, const char* update_json);

// 更新单个属性并自动标记脏
void yui_set_text(Layer* layer, const char* text);
void yui_set_bg_color(Layer* layer, const char* color);
void yui_set_visible(Layer* layer, int visible);

// 删除元素
int yui_remove_layer(Layer* root, const char* layer_id);
int yui_remove_child(Layer* parent, const char* child_id);
int yui_remove_all_children(Layer* parent);
```

## 性能优化

更新时自动标记脏区域，只重绘变化的部分：
- 修改文本：只重绘该组件的文本区域
- 修改颜色：只重绘背景
- 批量更新：一次性重新布局，避免多次计算

## 完整示例

### 表单验证成功后的 UI 更新

```javascript
YUI.update([
  {
    target: "emailInput",
    borderColor: "#4caf50",
    borderWidth: 2
  },
  {
    target: "validationIcon",
    visible: true,
    text: "✓",
    color: "#4caf50"
  },
  {
    target: "submitBtn",
    enabled: true,
    bgColor: "#4caf50",
    text: "提交表单"
  },
  {
    target: "errorLabel",
    visible: false
  }
]);
```

### JSON 文件格式

保存为 `update.json`：

```json
[
  { "target": "button1", "text": "点击我", "bgColor": "#4caf50" },
  { "target": "label1", "text": "状态：成功", "color": "#00ff00" },
  { "target": "status", "visible": true }
]
```

加载并应用：

```javascript
// 从文件加载
YUI.updateFromFile("update.json");

// 或从字符串
const updates = JSON.stringify([
  { target: "btn1", text: "更新" }
]);
YUI.update(updates);
```
