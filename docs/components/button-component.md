# Button Component 按钮组件

## 概述

Button 组件是 YUI 中最基础也是最重要的交互组件之一，用于触发用户操作。支持多种样式、状态和交互效果。

## 功能特性

- ✅ **多种样式**: 文本按钮、图标按钮、图文结合按钮
- ✅ **状态管理**: 默认、悬停、按下、禁用状态
- ✅ **事件支持**: 点击、悬停、焦点事件
- ✅ **自定义外观**: 可自定义颜色、大小、边框、圆角等
- ✅ **快捷键支持**: 可绑定键盘快捷键
- ✅ **动画效果**: 支持点击反馈动画

## 基本用法

### JSON 配置方式

```json
{
    "id": "primary_button",
    "type": "Button",
    "position": [100, 100],
    "size": [120, 40],
    "text": "确认提交",
    "style": {
        "bgColor": "#3498DB",
        "color": "#FFFFFF",
        "fontSize": 16,
        "borderRadius": 4,
        "borderWidth": 1,
        "borderColor": "#2980B9"
    },
    "events": {
        "onClick": "handleSubmit"
    }
}
```

### C 代码创建方式

```c
#include "components/button.h"

// 创建按钮
Layer* button_layer = layer_create(parent_layer, 100, 100, 120, 40);
ButtonComponent* button = button_component_create(button_layer);

// 设置属性
button_component_set_text(button, "确认提交");
button_component_set_bg_color(button, COLOR_BLUE);
button_component_set_text_color(button, COLOR_WHITE);

// 注册事件处理器
button_component_on_click(button, handle_submit_click, user_data);
```

### JavaScript 操作方式

```javascript
// 设置按钮文本
YUI.setText("primary_button", "保存更改");

// 获取按钮状态
var isEnabled = YUI.getEnabled("primary_button");

// 禁用按钮
YUI.setEnabled("primary_button", false);

// 添加点击事件
YUI.addEventListener("primary_button", "click", function() {
    console.log("按钮被点击了");
});
```

## 组件属性

### 基础属性

| 属性名 | 类型 | 默认值 | 说明 |
|--------|------|--------|------|
| `text` | String | "" | 按钮显示文本 |
| `icon` | String | null | 按钮图标路径 |
| `iconPosition` | String | "left" | 图标位置(left/right/top/bottom) |
| `enabled` | Boolean | true | 是否启用 |
| `visible` | Boolean | true | 是否可见 |

### 样式属性

| 属性名 | 类型 | 默认值 | 说明 |
|--------|------|--------|------|
| `bgColor` | Color | "#E0E0E0" | 背景颜色 |
| `hoverBgColor` | Color | "#D0D0D0" | 悬停背景色 |
| `activeBgColor` | Color | "#C0C0C0" | 按下背景色 |
| `disabledBgColor` | Color | "#F0F0F0" | 禁用背景色 |
| `color` | Color | "#333333" | 文字颜色 |
| `fontSize` | Number | 14 | 字体大小 |
| `fontFamily` | String | "Arial" | 字体族 |
| `borderRadius` | Number | 0 | 圆角半径 |
| `borderWidth` | Number | 0 | 边框宽度 |
| `borderColor` | Color | "#CCCCCC" | 边框颜色 |

### 布局属性

| 属性名 | 类型 | 默认值 | 说明 |
|--------|------|--------|------|
| `padding` | Array | [8, 16, 8, 16] | 内边距[top, right, bottom, left] |
| `minWidth` | Number | 0 | 最小宽度 |
| `minHeight` | Number | 0 | 最小高度 |
| `maxWidth` | Number | Infinity | 最大宽度 |
| `maxHeight` | Number | Infinity | 最大高度 |

## 事件系统

### 支持的事件

| 事件名 | 触发条件 | 事件对象属性 |
|--------|----------|--------------|
| `onClick` | 鼠标点击按钮 | `{x, y, button}` |
| `onMouseDown` | 鼠标按下 | `{x, y, button}` |
| `onMouseUp` | 鼠标释放 | `{x, y, button}` |
| `onMouseEnter` | 鼠标进入 | `{x, y}` |
| `onMouseLeave` | 鼠标离开 | `{x, y}` |
| `onFocus` | 获得焦点 | `{}` |
| `onBlur` | 失去焦点 | `{}` |
| `onKeyDown` | 按键按下 | `{keyCode, key}` |
| `onKeyUp` | 按键释放 | `{keyCode, key}` |

### 事件处理示例

```javascript
// JSON 配置中的事件绑定
{
    "id": "action_button",
    "type": "Button",
    "events": {
        "onClick": "handleAction",
        "onMouseEnter": "showTooltip",
        "onKeyDown": "handleShortcut"
    }
}

// JavaScript 事件处理器
function handleAction(event) {
    console.log("按钮被点击", event);
    // 执行相应操作
}

function showTooltip(event) {
    // 显示提示信息
    YUI.setText("tooltip", "这是一个操作按钮");
    YUI.show("tooltip");
}

function handleShortcut(event) {
    if (event.keyCode === 13) { // Enter键
        // 触发按钮点击
        YUI.triggerEvent("action_button", "click");
    }
}
```

## 状态管理

### 按钮状态

按钮支持四种基本状态：

1. **Normal (正常)** - 默认状态
2. **Hover (悬停)** - 鼠标悬停时
3. **Active (激活)** - 鼠标按下时
4. **Disabled (禁用)** - 禁用状态

### 状态切换

```javascript
// 禁用按钮
YUI.setEnabled("my_button", false);

// 检查按钮状态
var isEnabled = YUI.getEnabled("my_button");
var isVisible = YUI.getVisible("my_button");

// 切换按钮状态
function toggleButton() {
    var current = YUI.getEnabled("toggle_button");
    YUI.setEnabled("toggle_button", !current);
}
```

### 状态样式自定义

```json
{
    "id": "styled_button",
    "type": "Button",
    "text": "样式按钮",
    "states": {
        "normal": {
            "bgColor": "#3498DB",
            "color": "#FFFFFF"
        },
        "hover": {
            "bgColor": "#2980B9",
            "color": "#FFFFFF"
        },
        "active": {
            "bgColor": "#2573A7",
            "color": "#FFFFFF"
        },
        "disabled": {
            "bgColor": "#BDC3C7",
            "color": "#7F8C8D"
        }
    }
}
```

## 高级功能

### 图标按钮

```json
{
    "id": "icon_button",
    "type": "Button",
    "icon": "assets/icons/save.png",
    "iconPosition": "top",
    "size": [60, 60],
    "style": {
        "bgColor": "transparent",
        "borderRadius": 30
    }
}
```

### 按钮组

```json
{
    "id": "button_group",
    "type": "View",
    "layout": {
        "type": "horizontal",
        "spacing": 10
    },
    "children": [
        {
            "type": "Button",
            "text": "新建",
            "style": {"bgColor": "#27AE60"}
        },
        {
            "type": "Button", 
            "text": "编辑",
            "style": {"bgColor": "#3498DB"}
        },
        {
            "type": "Button",
            "text": "删除",
            "style": {"bgColor": "#E74C3C"}
        }
    ]
}
```

### 带进度的按钮

```javascript
// 创建带进度指示的按钮
function createProgressButton() {
    var button = {
        id: "progress_button",
        type: "Button",
        text: "下载",
        events: {
            onClick: startDownload
        }
    };
    
    return button;
}

function startDownload() {
    // 禁用按钮
    YUI.setEnabled("progress_button", false);
    YUI.setText("progress_button", "下载中...");
    
    // 模拟下载过程
    simulateDownload(function(progress) {
        if (progress < 100) {
            YUI.setText("progress_button", `下载中... ${progress}%`);
        } else {
            YUI.setText("progress_button", "下载完成");
            YUI.setEnabled("progress_button", true);
        }
    });
}
```

## 主题支持

### 主题样式

```json
{
    "name": "dark_theme",
    "styles": [
        {
            "selector": "Button",
            "style": {
                "bgColor": "#2C3E50",
                "color": "#ECF0F1",
                "borderColor": "#34495E",
                "hoverBgColor": "#34495E",
                "activeBgColor": "#233240"
            }
        },
        {
            "selector": "#primary_button",
            "style": {
                "bgColor": "#3498DB",
                "hoverBgColor": "#2980B9"
            }
        }
    ]
}
```

### 动态主题切换

```javascript
// 切换按钮主题
function switchButtonTheme(darkMode) {
    if (darkMode) {
        YUI.themeSetCurrent("dark_theme");
    } else {
        YUI.themeSetCurrent("light_theme");
    }
    YUI.themeApplyToTree();
}
```

## 性能优化

### 批量操作

```javascript
// 避免频繁更新
function updateMultipleButtons() {
    // 先禁用重绘
    YUI.setBatchUpdate(true);
    
    // 批量更新按钮
    YUI.setText("btn1", "新文本1");
    YUI.setText("btn2", "新文本2");
    YUI.setEnabled("btn3", false);
    
    // 重新启用重绘
    YUI.setBatchUpdate(false);
}
```

### 事件委托

```javascript
// 对于大量按钮，使用事件委托提高性能
{
    "id": "button_container",
    "type": "View",
    "events": {
        "onClick": "handleButtonClick"
    },
    "children": [
        {"type": "Button", "id": "btn1", "text": "按钮1"},
        {"type": "Button", "id": "btn2", "text": "按钮2"},
        {"type": "Button", "id": "btn3", "text": "按钮3"}
    ]
}

function handleButtonClick(event) {
    // 通过事件目标确定具体按钮
    var buttonId = event.target.id;
    console.log("点击了按钮:", buttonId);
}
```

## 常见问题

### Q: 按钮点击没有反应？
A: 检查以下几点：
- 按钮是否处于启用状态
- 是否正确绑定了事件处理器
- 按钮是否在可视区域内
- 是否有其他元素遮挡了按钮

### Q: 如何实现按钮的防重复点击？
A: 可以通过禁用按钮来防止重复点击：

```javascript
function handleButtonClick() {
    // 立即禁用按钮
    YUI.setEnabled("my_button", false);
    
    // 执行操作
    performAction(function() {
        // 操作完成后重新启用
        YUI.setEnabled("my_button", true);
    });
}
```

### Q: 按钮文字显示不完整？
A: 检查按钮尺寸是否足够容纳文字，或调整字体大小和内边距。

## 示例应用

### 登录表单提交按钮

```json
{
    "id": "login_button",
    "type": "Button",
    "position": [250, 200],
    "size": [100, 40],
    "text": "登录",
    "style": {
        "bgColor": "#3498DB",
        "color": "#FFFFFF",
        "fontSize": 16,
        "borderRadius": 4
    },
    "events": {
        "onClick": "handleLogin"
    }
}
```

### 工具栏按钮组

```json
{
    "id": "toolbar",
    "type": "View",
    "position": [0, 0],
    "size": [800, 50],
    "layout": {
        "type": "horizontal",
        "spacing": 5,
        "padding": [10, 10, 10, 10]
    },
    "children": [
        {
            "type": "Button",
            "text": "📁 新建",
            "events": {"onClick": "newDocument"}
        },
        {
            "type": "Button", 
            "text": "💾 保存",
            "events": {"onClick": "saveDocument"}
        },
        {
            "type": "Button",
            "text": "📤 导出",
            "events": {"onClick": "exportDocument"}
        }
    ]
}
```

---

*更多示例请查看 `app/tests/test-button.json`*