# YUI框架JSON格式规范说明

## 一、概述

YUI框架采用JSON（JavaScript Object Notation）格式定义用户界面，实现UI与业务逻辑的分离。本文档详细说明YUI框架支持的JSON配置格式规范，帮助开发者正确编写UI配置文件。

## 二、文件类型

YUI框架支持两种主要的JSON配置文件类型：

1. **主配置文件**：用于指定应用程序的基本设置和入口点
2. **UI定义文件**：用于描述具体的界面布局和组件配置

## 三、主配置文件结构

主配置文件是应用程序的入口，基本结构如下：

```json
{
  "type": "main",
  "assets": "app/assets",    // 资源目录路径
  "font": "Roboto-Regular.ttf",  // 默认字体文件
  "fontSize": "16",          // 默认字体大小
  "source": "app/test.json"   // UI定义文件路径
}
```

### 主配置文件属性说明

| 属性名 | 类型 | 说明 | 是否必需 |
|--------|------|------|----------|
| type | String | 固定值"main"，标识为主配置文件 | 是 |
| assets | String | 资源目录路径，用于加载图像、字体等资源 | 是 |
| font | String | 默认字体文件名 | 否 |
| fontSize | String/Number | 默认字体大小 | 否 |
| source | String | UI定义文件路径 | 是 |

## 四、UI定义文件结构

UI定义文件采用树形结构描述界面，基本结构如下：

```json
{
  "id": "main_panel",         // 组件唯一标识符
  "type": "Panel",           // 组件类型
  "position": [0, 0],         // 位置坐标 [x, y]
  "size": [800, 600],         // 大小 [width, height]
  "style": {                  // 样式设置
    "color": "#2C3E50"
  },
  "events": {                 // 事件绑定
    "onClick": "@handlePanelClick"
  },
  "children": [               // 子组件数组
    // 子组件定义...
  ],
  "metadata": {               // 元数据信息
    "version": "1.0.0",
    "dependencies": []
  }
}
```

## 五、核心组件类型

YUI框架支持以下核心组件类型：

| 组件类型 | 说明 | JSON 类型值 |
|----------|------|-------------|
| VIEW | 基础视图容器 | `"type": "View"` |
| BUTTON | 按钮组件 | `"type": "Button"` |
| INPUT | 输入框组件 | `"type": "Input"` |
| LABEL | 文本标签组件 | `"type": "Label"` |
| IMAGE | 图像组件 | `"type": "Image"` |
| LIST | 列表组件 | `"type": "List"` |
| GRID | 网格布局组件 | `"type": "Grid"` |
| PROGRESS | 进度条组件 | `"type": "Progress"` |
| CHECKBOX | 复选框组件 | `"type": "Checkbox"` |
| RADIOBOX | 单选框组件 | `"type": "Radiobox"` |
| TEXT | 文本组件 | `"type": "Text"` |
| TREEVIEW | 树形视图组件 | `"type": "Treeview"` |
| TAB | 选项卡组件 | `"type": "Tab"` |
| SLIDER | 滑块组件 | `"type": "Slider"` |
| SELECT | 选择器组件 | `"type": "Select"` |
| SCROLLBAR | 滚动条组件 | `"type": "Scrollbar"` |
| MENU | 菜单组件 | `"type": "Menu"` |
| DIALOG | 对话框组件 | `"type": "Dialog"` |
| CLOCK | 时钟组件 | `"type": "Clock"` |

## 六、通用属性

所有组件共享以下通用属性：

### 基本属性


| 属性名 | 类型 | 说明 | 是否必需 |
|--------|------|------|----------|
| name | String | 组件名称标识符 | 是 |
| type | String | 组件类型 | 是 |
| size | Array | 大小 [width, height] | 是 |
| position | Array | 位置 [x, y] | 是 |
| style | Object | 样式配置对象 | 否 |
| - color | String | 背景颜色，支持RGBA格式 | 否 |
| - fontSize | Integer | 字体大小 | 否 |
| - textColor | String | 文本颜色，支持RGBA格式 | 否 |
| children | Array | 子组件数组 | 否 |


### 颜色格式说明

YUI框架支持RGBA颜色格式：
- `rgba(R, G, B, A)` 其中：
  - R、G、B：0-255 的整数值
  - A：0-255 的透明度值（0为完全透明，255为完全不透明）

示例：
- `rgba(255, 255, 255, 255)` - 白色
- `rgba(0, 0, 0, 255)` - 黑色
- `rgba(0, 120, 215, 255)` - 蓝色
- `rgba(240, 240, 240, 255)` - 浅灰色

## 七、布局管理器

YUI框架提供强大的布局管理器，通过 `layout` 属性控制子组件的排列方式。

### 基本结构

```json
{
  "id": "container",
  "type": "View",
  "layout": {
    "type": "vertical",              // 布局类型
    "spacing": 10,                    // 子组件间距
    "padding": [10, 10, 10, 10],      // 内边距 [上, 右, 下, 左]
    "align": "center",                // 对齐方式 
    "justifyContent": "center"        // 主轴对齐方式 应用主轴对齐- 垂直方向
  },
  "children": [
    // 子组件
  ]
}
```

### 布局类型

| 类型值 | 说明 | 适用场景 |
|--------|------|----------|
| `horizontal` | 水平排列，子组件从左到右排列 | 工具栏、按钮组 |
| `vertical` | 垂直排列，子组件从上到下排列 | 表单、列表 |
| `center` | 居中布局，子组件在容器中心 | 对话框、提示信息 |
| `left` |  居左布局，子组件在容器左侧 | 菜单、导航栏 |
| `right` | 居右布局，子组件在容器右侧 | 侧边栏、工具栏 |

### 布局属性

| 属性名 | 类型 | 说明 | 默认值 |
|--------|------|------|--------|
| `type` | String | 布局类型 | `absolute` |
| `spacing` | Number | 子组件之间的间距 | `0` |
| `padding` | Array | 内边距 `[上, 右, 下, 左]` | `[0, 0, 0, 0]` |
| `align` | String | 垂直于排列方向的对齐(交叉轴) | `left` |
| `justifyContent` | String | 主轴对齐方式, 沿着排列方向的对齐(主轴) | `flex-start` |

### 对齐方式选项

#### align 属性（交叉轴对齐）
- `left` - 左对齐
- `center` - 居中对齐
- `right` - 右对齐

#### justifyContent 属性（主轴对齐）
- `flex-start` - 起始位置对齐
- `center` - 居中对齐
- `flex-end` - 结束位置对齐
- `space-between` - 两端对齐，子组件之间间距相等
- `space-around` - 每个子组件两侧间距相等

### 布局示例

#### 水平布局示例
```json
{
  "id": "toolbar",
  "type": "View",
  "layout": {
    "type": "horizontal",
    "spacing": 10,
    "padding": [10, 10, 10, 10],
    "justifyContent": "space-between"
  },
  "children": [
    {"type": "Button", "text": "新增"},
    {"type": "Button", "text": "编辑"},
    {"type": "Button", "text": "删除"}
  ]
}
```

#### 垂直布局示例
```json
{
  "id": "form",
  "type": "View",
  "layout": {
    "type": "vertical",
    "spacing": 15,
    "padding": [20, 20, 20, 20],
    "align": "center"
  },
  "children": [
    {
      "type": "Label",
      "text": "用户名",
      "style": {"fontSize": 14}
    },
    {
      "type": "Input",
      "placeholder": "请输入用户名",
      "size": [300, 35]
    }
  ]
}
```

#### 嵌套布局示例
```json
{
  "id": "main",
  "type": "View",
  "layout": {
    "type": "vertical",
    "spacing": 0
  },
  "children": [
    {
      "id": "header",
      "type": "View",
      "size": [800, 60],
      "layout": {
        "type": "horizontal",
        "justifyContent": "space-between",
        "padding": [10, 20, 10, 20]
      }
    },
    {
      "id": "content",
      "type": "View",
      "layout": {
        "type": "horizontal",
        "spacing": 10
      }
    }
  ]
}
```

## 九、组件特有属性

### 1. Button组件特有属性

```json
{
  "id": "submit_btn",
  "type": "Button",
  "text": "提交",           // 按钮文本
  "textAlign": "center",   // 文本对齐方式
  "style": {
    "textColor": "#FFFFFF", // 文本颜色
    "fontSize": 16           // 文本大小
  }
}
```

### 2. Input组件特有属性

```json
{
  "id": "username_input",
  "type": "Input",
  "label": "用户名",        // 输入框标签
  "placeholder": "请输入用户名", // 占位文本
  "value": "",              // 默认值
  "maxLength": 50,           // 最大长度限制
  "password": false          // 是否为密码框
}
```

### 3. Label组件特有属性

```json
{
  "id": "title_label",
  "type": "Label",
  "text": "欢迎使用YUI框架",  // 显示文本
  "style": {
    "textColor": "#333333", // 文本颜色
    "fontSize": 24,          // 字体大小
    "textAlign": "center"    // 文本对齐方式
  }
}
```

### 4. Image组件特有属性

```json
{
  "id": "logo_image",
  "type": "Image",
  "source": "assets/logo.png", // 图像文件路径
  "mode": "contain"            // 图像显示模式：stretch(拉伸)、contain(保持比例)、cover(覆盖)
}
```

### 5. Grid组件特有属性

```json
{
  "id": "photo_grid",
  "type": "Grid",
  "columns": 3,              // 列数
  "children": [
    // 网格项
  ]
}
```

### 6. Progress组件特有属性

```json
{
  "id": "progress",
  "type": "Progress",
  "data": 30,// 进度值%，范围0-100
  "style": {
              "color": "#2C3E50",
              "bgColor":"#CCCCCC",
              "radius": 40
          }
}

```

### 7. CheckBox组件特有属性

```json
{
  "id": "checkbox",
  "type": "Checkbox",
  "data": true,              // 是否选中
  "label": "选项文本",      // 标签文本
  "style": {
    "color": "#2C3E50"      // 文本颜色
  }
}
```

### 8. RadioBox组件特有属性

```json
{
  "id": "radio1",
  "type": "Radiobox",
  "group": "group1",         // 单选框组ID
  "checked": true,           // 是否选中
  "label": "选项文本",      // 标签文本
  "style": {
    "color": "#2C3E50"      // 文本颜色
  }
}
```

### 9. Text组件特有属性

```json
{
  "id": "text_content",
  "type": "Text",
  "text": "文本内容",        // 文本内容
  "style": {
    "textColor": "#333333", // 文本颜色
    "fontSize": 16,          // 字体大小
    "textAlign": "left"     // 文本对齐方式
  }
}
```

### 10. TreeView组件特有属性

```json
{
  "id": "treeview",
  "type": "Treeview",
  "itemHeight": 30,          // 项目高度
  "indentWidth": 20,         // 缩进宽度
  "style": {
    "textColor": "#000000",              // 文本颜色
    "selectedTextColor": "#FFFFFF",      // 选中文本颜色
    "selectedBgColor": "#3399FF",        // 选中背景颜色
    "hoverBgColor": "#DCDCDC",          // 悬停背景颜色
    "expandIconColor": "#000000"        // 展开图标颜色
  },
  "data": [                  // 树节点数据
    {
      "text": "节点1",
      "expanded": true,
      "children": [
        {"text": "子节点1"},
        {"text": "子节点2"}
      ]
    }
  ]
}
```

### 11. Tab组件特有属性

```json
{
  "id": "tab_container",
  "type": "Tab",
  "tabHeight": 30,           // 标签高度
  "activeTab": 0,            // 当前激活的标签索引
  "closable": false,         // 是否可关闭
  "style": {
    "tabColor": "#F0F0F0",           // 标签背景颜色
    "activeTabColor": "#FFFFFF",     // 激活标签背景颜色
    "textColor": "#333333",          // 文本颜色
    "activeTextColor": "#000000",    // 激活文本颜色
    "borderColor": "#CCCCCC"         // 边框颜色
  },
  "data": [                  // 标签页数据
    {"text": "标签页1"},
    {"text": "标签页2"}
  ]
}
```

### 12. Slider组件特有属性

```json
{
  "id": "slider",
  "type": "Slider",
  "min": 0,                  // 最小值
  "max": 100,                // 最大值
  "data": 50,                // 当前值
  "step": 1,                 // 步长
  "orientation": "horizontal",  // 方向：horizontal/vertical
  "style": {
    "trackColor": "#E0E0E0",     // 轨道颜色
    "thumbColor": "#0078D7",     // 滑块颜色
    "activeThumbColor": "#005A9E" // 激活滑块颜色
  }
}
```

### 13. Select组件特有属性

```json
{
  "id": "select",
  "type": "Select",
  "maxVisibleItems": 5,      // 最大可见项目数
  "itemHeight": 30,          // 项目高度
  "borderWidth": 1,          // 边框宽度
  "borderRadius": 4,         // 边框圆角
  "fontSize": 14,            // 字体大小
  "data": [                  // 选项数据
    {"text": "选项1", "value": "value1"},
    {"text": "选项2", "value": "value2"}
  ],
  "style": {
    "textColor": "#333333",              // 文本颜色
    "selectedTextColor": "#FFFFFF",      // 选中文本颜色
    "selectedBgColor": "#3399FF",        // 选中背景颜色
    "disabledTextColor": "#AAAAAA",      // 禁用文本颜色
    "hoverBgColor": "#F5F5F5"            // 悬停背景颜色
  }
}
```

### 14. Menu组件特有属性

```json
{
  "id": "menu",
  "type": "Menu",
  "items": [                 // 菜单项数组
    {
      "text": "菜单项1",    // 显示文本
      "shortcut": "Ctrl+N",  // 快捷键提示
      "enabled": true,       // 是否启用
      "checked": false,      // 是否选中（勾选）
      "separator": false     // 是否为分隔线
    },
    {
      "text": "分隔线",
      "separator": true
    }
  ],
  "style": {
    "textColor": "#323232",   // 文本颜色
    "hoverColor": "#4682B4",  // 悬停颜色
    "disabledColor": "#969696" // 禁用颜色
  }
}
```

### 15. Dialog组件特有属性

```json
{
  "id": "dialog",
  "type": "Dialog",
  "title": "对话框标题",     // 标题文本
  "message": "消息内容",     // 消息内容
  "type": "info",            // 类型：info/warning/error/question/custom
  "modal": true,             // 是否为模态对话框
  "buttons": [               // 按钮配置
    {
      "text": "确定",
      "default": true,
      "cancel": false
    },
    {
      "text": "取消",
      "default": false,
      "cancel": true
    }
  ],
  "style": {
    "textColor": "#000000",          // 文本颜色
    "buttonTextColor": "#FFFFFF"     // 按钮文本颜色
  }
}
```

### 16. ScrollBar组件特有属性

```json
{
  "id": "scrollbar",
  "type": "Scrollbar",
  "direction": "vertical",   // 方向：vertical/horizontal
  "thickness": 10,           // 滚动条厚度
  "trackColor": "#F0F0F0",   // 轨道颜色
  "thumbColor": "#B4B4B4"    // 滑块颜色
}
```

### 17. Clock组件特有属性

```json
{
  "id": "clock",
  "type": "Clock",
  "clockConfig": {
    "radius": 100,           // 时钟半径
    "borderWidth": 3,        // 边框宽度
    "hourHandLength": 60,    // 时针长度
    "minuteHandLength": 80,  // 分针长度
    "secondHandLength": 85,  // 秒针长度
    "handWidth": 4,          // 指针宽度
    "centerRadius": 8,       // 中心圆半径
    "showNumbers": true,     // 是否显示数字
    "fontSize": 12,          // 字体大小
    "smoothSecond": true     // 秒针是否平滑移动
  },
  "style": {
    "hourHandColor": "#000000",      // 时针颜色
    "minuteHandColor": "#000000",    // 分针颜色
    "secondHandColor": "#FF0000",    // 秒针颜色
    "centerColor": "#000000",        // 中心圆颜色
    "borderColor": "#000000",        // 边框颜色
    "backgroundColor": "#FFFFFF",    // 背景颜色
    "numberColor": "#000000"         // 数字颜色
  }
}
```


## 十、事件绑定

YUI框架支持事件绑定机制，通过`events`属性定义组件的事件处理函数：

```json
{
  "id": "submit_btn",
  "type": "Button",
  "events": {
    "onClick": "handleSubmit",      // 点击事件
    "onMouseEnter": "onButtonHover", // 鼠标悬停事件
    "onMouseLeave": "onButtonOut"    // 鼠标离开事件
  }
}
```

### 支持的事件类型

| 事件名称 | 说明 | 适用组件 |
|----------|------|----------|
| onClick | 点击事件 | 所有可交互组件 |
| onMouseEnter | 鼠标进入事件 | 所有组件 |
| onMouseLeave | 鼠标离开事件 | 所有组件 |
| onChange | 值变化事件 | Input、Slider、Select等组件 |
| onFocus | 获得焦点事件 | Input、Select等组件 |
| onBlur | 失去焦点事件 | Input、Select等组件 |
| onScroll | 滚动事件 | 可滚动组件 |
| onTouch | 触屏事件 | 支持触摸的组件 |

## 十一、动画配置

YUI框架支持丰富的动画效果，通过`animation`属性定义：

```json
{
  "id": "animated_element",
  "type": "Panel",
  "animation": {
    "duration": 1.0,         // 动画时长（秒）
    "easing": "easeOutElastic", // 缓动函数
    "fillMode": "forwards", // 填充模式
    "repeatType": "count",  // 重复类型
    "repeatCount": 3,        // 重复次数
    "reverseOnRepeat": true, // 反向重复
    "properties": {
      "x": 300,              // 目标X坐标
      "y": 200,              // 目标Y坐标
      "opacity": 0.8,        // 目标透明度
      "rotation": 180        // 目标旋转角度
    }
  }
}
```

### 动画属性说明

| 属性名 | 类型 | 说明 | 默认值 |
|--------|------|------|--------|
| duration | Number | 动画时长（秒） | 0.5 |
| easing | String | 缓动函数名称 | "linear" |
| fillMode | String | 填充模式：none、forwards、backwards、both | "none" |
| repeatType | String | 重复类型：none、count、infinite | "none" |
| repeatCount | Number | 重复次数 | 1 |
| reverseOnRepeat | Boolean | 反向重复 | false |
| properties | Object | 动画目标属性 | {} |

### 支持的缓动函数

- linear：线性
- easeInQuad：二次方缓入
- easeOutQuad：二次方缓出
- easeInOutQuad：二次方缓入缓出
- easeOutElastic：弹性缓出

## 十二、模块化设计

YUI框架支持将复杂UI拆分为独立模块，通过`source`字段引用外部JSON文件：

```json
{
  "id": "user_form",
  "type": "Panel",
  "source": "components/user_form.json" // 外部模块文件路径
}
```

## 十三、数据绑定

YUI框架支持简单的数据绑定功能，通过`dataBinding`属性实现：

```json
{
  "id": "username_input",
  "type": "Input",
  "dataBinding": {
    "path": "user.username"  // 数据路径
  }
}
```

## 十四、示例：照片相册界面

以下是一个完整的照片相册界面配置示例：

```json
{
  "id": "photo_album",
  "type": "Panel",
  "position": [0, 0],
  "size": [800, 1000],
  "style": {
    "color": "#F5F5F5",
    "borderRadius": 10,
    "borderWidth": 2,
    "borderColor": "#E0E0E0"
  },
  "children": [
    {
      "id": "title_bar",
      "type": "Panel",
      "position": [0, 0],
      "size": [800, 60],
      "style": {
        "color": "#3498DB",
        "borderRadius": [10, 10, 0, 0]
      },
      "children": [
        {
          "id": "title_label",
          "type": "Label",
          "position": [400, 30],
          "text": "我的相册",
          "style": {
            "textColor": "#FFFFFF",
            "fontSize": 24,
            "textAlign": "center"
          }
        }
      ]
    },
    {
      "id": "photo_grid",
      "type": "Grid",
      "position": [30, 80],
      "size": [740, 800],
      "columns": 3,
      "gap": 20,
      "children": [
        {
          "id": "photo1",
          "type": "Image",
          "size": [220, 300],
          "source": "duck.png",
          "mode": "contain",
          "style": {
            "borderRadius": 5
          }
        },
        // 更多照片项...
      ]
    },
    {
      "id": "footer",
      "type": "Panel",
      "position": [0, 940],
      "size": [800, 60],
      "style": {
        "color": "#34495E",
        "borderRadius": [0, 0, 10, 10]
      },
      "children": [
        {
          "id": "prev_btn",
          "type": "Button",
          "position": [300, 30],
          "size": [80, 30],
          "text": "上一页",
          "events": {
            "onClick": "prevPage"
          }
        },
        {
          "id": "next_btn",
          "type": "Button",
          "position": [420, 30],
          "size": [80, 30],
          "text": "下一页",
          "events": {
            "onClick": "nextPage"
          }
        }
      ]
    }
  ]
}
```

## 十五、最佳实践

1. **组件命名规范**：使用有意义的ID，采用小写字母和下划线组合
2. **模块化拆分**：将复杂界面拆分为多个小型、可复用的组件
3. **合理使用样式**：统一配色方案，保持界面风格一致性
4. **事件处理分离**：UI配置与事件处理函数分离，提高代码可维护性
5. **避免过度嵌套**：控制组件层级深度，避免过深嵌套影响性能

## 十六、版本历史

| 版本 | 日期 | 更新内容 |
|------|------|----------|
| 1.0.0 | 2023-10-01 | 初始版本，支持基础组件和布局 |
| 1.1.0 | 2023-11-15 | 新增动画系统和数据绑定功能 |
| 1.2.0 | 2023-12-20 | 增强模块化支持和样式配置选项 |

## 十七、附录：颜色值参考

YUI框架支持以下颜色表示方式：

1. **HEX格式**：如`#FF0000`表示红色
2. **RGB格式**：如`rgb(255, 0, 0)`表示红色
3. **RGBA格式**：如`rgba(255, 0, 0, 0.5)`表示半透明红色

## 十八、常见问题解答

**Q: 如何实现响应式布局？**
**A:** 使用Grid组件配合flex_ratio属性，或者通过代码动态调整组件大小和位置。

**Q: 如何加载网络图片？**
**A:** 当前版本仅支持本地图片，需先下载到本地assets目录后引用。

**Q: 如何自定义动画效果？**
**A:** 通过配置animation属性，选择合适的缓动函数和动画参数。