# 组件图标布局方案

## 目标

为 Label、Button、Input 等基础组件统一添加 `icon` 属性，支持图标与文本的排列，替代手动嵌套 View + Label 的 DIY 方式。

## 现状

- **Button**: 已支持 `icon_text` / `icon_path`，图标固定渲染在文字左侧
- **Label**: 无图标支持，需手动嵌套 View
- **Input**: 无图标支持

## 方案：统一 `icon` 属性

### 新增属性

| 属性 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `icon` | string | null | 图标文本（emoji 或单字符），为空则不渲染图标 |
| `iconAlign` | string | "left" | 图标与文字的相对位置: `left` / `right` / `top` / `bottom` / `center` |
| `iconSize` | number | 0 (=跟随 fontSize) | 图标尺寸，0 表示与文字同号 |
| `iconGap` | number | 4 | 图标与文字间距 |

### 布局规则

`iconAlign` 分五组模式，icon+text 整体始终在组件区域内居中：

```
iconAlign: "left"          iconAlign: "right"
┌──────────────────┐       ┌──────────────────┐
│ [icon]  text      │       │  text  [icon]    │
└──────────────────┘       └──────────────────┘

iconAlign: "top"           iconAlign: "bottom"
┌──────────────────┐       ┌──────────────────┐
│     [icon]       │       │     text         │
│     text         │       │     [icon]       │
└──────────────────┘       └──────────────────┘

iconAlign: "center"  (icon 为主要内容，text 为 caption，垂直排列)
┌──────────────────┐
│     [icon]       │
│     text         │
└──────────────────┘
```

| 值 | 布局方向 | icon 位置 | text 位置 | 特点 |
|----|----------|-----------|-----------|------|
| `left` | 水平 | 左 | 右 | 按钮/输入框常用 |
| `right` | 水平 | 右 | 左 | 带箭头/跳转的按钮 |
| `top` | 垂直 | 上 | 下 | 卡片式入口 |
| `bottom` | 垂直 | 下 | 上 | 反向卡片 |
| `center` | 垂直 | 上（大） | 下（caption） | Launcher 网格专用，icon 为主视觉 |

### 各值典型场景

| 值 | 典型场景 | 效果 |
|----|----------|------|
| `left` | 菜单项、搜索框 | `🔍 Search` |
| `right` | "下一步"按钮 | `Next →` |
| `top` | 功能卡片 | icon 在上，标题在下 |
| `bottom` | 照片说明 | 标题在上，icon 在下 |
| `center` | Launcher 网格、主屏幕 | 大 icon 居中，caption 小字在下 |

### 使用示例

**Label:**
```json
{
    "type": "Label",
    "text": "设置",
    "icon": "⚙️",
    "iconAlign": "left",
    "iconSize": 16
}
```

**Button:**
```json
{
    "type": "Button",
    "text": "保存",
    "icon": "💾",
    "iconAlign": "right"
}
```

**Input:**
```json
{
    "type": "Input",
    "placeholder": "搜索...",
    "icon": "🔍",
    "iconAlign": "left"
}
```

**Launcher 网格（替代当前 View+2*Label 方案）:**
```json
{
    "type": "Button",
    "size": [140, 140],
    "icon": "🧮",
    "text": "Calculator",
    "iconAlign": "center",
    "iconSize": 32,
    "fontSize": 10
}
```

一行 JSON 替代原来 View + 两个 Label 的手动嵌套，效果同 watch-os launcher 的 `Button + dock-flat` 但自带 caption 文字。

**功能卡片:**
```json
{
    "type": "Button",
    "size": [200, 60],
    "icon": "📷",
    "text": "我的相册",
    "iconAlign": "left",
    "iconSize": 24
}
```

**带图标的输入框:**
```json
{
    "type": "Input",
    "placeholder": "搜索...",
    "icon": "🔍",
    "iconAlign": "left"
}
```

### 向后兼容

- `icon` 为 null 或空串时行为不变
- Button 现有的 `icon_text` / `icon_path` 继续支持，与 `icon` 互斥（优先 `icon_path` → `icon` → `icon_text`）
- Label / Input 新增 `icon` 属性，不影响已有功能

### 实现要点

1. 在每个组件 render 函数内，测量 icon 和 text 尺寸后按 `iconAlign` 计算各自 rect 并渲染
2. icon 始终用 `render_text()` 渲染（复用字体管线，支持 emoji fallback）
3. `iconAlign: "center"` 时垂直排列 icon 在上 text 在下，整体水平居中
4. 组件有背景/border 时，icon+text 在内容区内居中
