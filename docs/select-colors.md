# Select 组件颜色支持文档

## 概述

Select 组件现在支持丰富的颜色配置，包括输入框和下拉菜单的各种状态颜色。

## 支持的颜色配置

### 基础颜色

| 颜色字段 | 描述 | 默认值 | 示例 |
|---------|------|--------|------|
| `bgColor` | 输入框背景色 | `rgba(255, 255, 255, 255)` | `"#FFFFFF"` |
| `textColor` | 输入框文本色 | `rgba(51, 51, 51, 255)` | `"#333333"` |
| `borderColor` | 默认边框色 | `rgba(204, 204, 204, 255)` | `"#CCCCCC"` |
| `arrowColor` | 下拉箭头色 | `rgba(102, 102, 102, 255)` | `"#666666"` |

### 状态颜色

| 颜色字段 | 描述 | 默认值 | 示例 |
|---------|------|--------|------|
| `focusBorderColor` | 焦点状态边框色 | `rgba(0, 123, 255, 255)` | `"#007BFF"` |
| `hoverBorderColor` | 悬停状态边框色 | `rgba(0, 123, 255, 255)` | `"#007BFF"` |

### 下拉菜单颜色

| 颜色字段 | 描述 | 默认值 | 示例 |
|---------|------|--------|------|
| `dropdownBgColor` | 下拉菜单背景色 | `rgba(255, 255, 255, 255)` | `"#FFFFFF"` |
| `hoverBgColor` | 选项悬停背景色 | `rgba(245, 245, 245, 255)` | `"#F5F5F5"` |
| `selectedBgColor` | 选项选中背景色 | `rgba(0, 120, 215, 255)` | `"#0078D7"` |
| `selectedTextColor` | 选项选中文本色 | `rgba(255, 255, 255, 255)` | `"#FFFFFF"` |
| `disabledTextColor` | 禁用选项文本色 | `rgba(170, 170, 170, 255)` | `"#AAAAAA"` |

### 滚动条颜色

| 颜色字段 | 描述 | 默认值 | 示例 |
|---------|------|--------|------|
| `scrollbarColor` | 滚动条滑块色 | `rgba(180, 180, 180, 255)` | `"#B4B4B4"` |
| `scrollbarBgColor` | 滚动条轨道色 | `rgba(240, 240, 240, 255)` | `"#F0F0F0"` |

## 配置示例

```json
{
  "type": "Select",
  "name": "mySelect",
  "position": [50, 50],
  "size": [200, 40],
  "selectConfig": {
    "placeholder": "请选择选项",
    "items": ["选项1", "选项2", "选项3"],
    "colors": {
      "bgColor": "#FFFFFF",
      "textColor": "#333333",
      "borderColor": "#DDDDDD",
      "focusBorderColor": "#007BFF",
      "hoverBorderColor": "#0056B3",
      "arrowColor": "#666666",
      "dropdownBgColor": "#FFFFFF",
      "hoverBgColor": "#F8F9FA",
      "selectedBgColor": "#007BFF",
      "selectedTextColor": "#FFFFFF",
      "disabledTextColor": "#999999",
      "scrollbarColor": "#CCCCCC",
      "scrollbarBgColor": "#F1F1F1"
    }
  }
}
```

## 状态行为

1. **正常状态**: 使用 `borderColor`
2. **悬停状态**: 鼠标悬停在输入框上时使用 `hoverBorderColor`
3. **焦点状态**: 输入框获得焦点时使用 `focusBorderColor`
4. **展开状态**: 下拉菜单展开时输入框使用 `focusBorderColor`

## API 函数

### 设置基础颜色
```c
void select_component_set_colors(SelectComponent* component, 
                                Color bg, Color text, Color border, Color arrow,
                                Color dropdown_bg, Color hover, Color selected_bg, 
                                Color selected_text, Color disabled_text);
```

### 设置扩展颜色
```c
void select_component_set_extended_colors(SelectComponent* component,
                                       Color focus_border, Color hover_border,
                                       Color scrollbar, Color scrollbar_bg);
```

## 视觉特性

- **下拉边框**: 自动添加阴影效果增强视觉层次
- **圆角支持**: 输入框和下拉菜单都支持圆角
- **状态反馈**: 根据交互状态动态改变边框颜色
- **可定制滚动条**: 支持自定义滚动条颜色

## 注意事项

1. 颜色值支持 RGB、RGBA、十六进制格式
2. 状态颜色会覆盖默认边框颜色
3. 下拉菜单背景始终使用 `dropdownBgColor`
4. 禁用选项始终使用 `disabledTextColor`