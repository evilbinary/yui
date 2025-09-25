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
    "onClick": "handlePanelClick"
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

| 组件类型 | 说明 | 关键字 |
|----------|------|--------|
| VIEW | 基础视图容器 | `"type": "Panel"` |
| BUTTON | 按钮组件 | `"type": "Button"` |
| INPUT | 输入框组件 | `"type": "Input"` |
| LABEL | 文本标签组件 | `"type": "Label"` |
| IMAGE | 图像组件 | `"type": "Image"` |
| LIST | 列表组件 | `"type": "List"` |
| GRID | 网格布局组件 | `"type": "Grid"` |
| PROGRESS | 进度条组件 | `"type": "Progress"` |

## 六、通用属性

所有组件共享以下通用属性：

### 基本属性

| 属性名 | 类型 | 说明 | 是否必需 |
|--------|------|------|----------|
| id | String | 组件唯一标识符 | 是 |
| type | String | 组件类型 | 是 |
| position | Array | 位置坐标 [x, y] | 否 |
| size | Array | 大小 [width, height] | 否 |
| style | Object | 样式设置 | 否 |
| events | Object | 事件绑定 | 否 |
| children | Array | 子组件数组 | 否 |
| source | String | 模块化组件引用路径 | 否 |
| metadata | Object | 元数据信息 | 否 |

### style样式属性

组件支持以下通用样式属性：

| 样式属性 | 类型 | 说明 | 默认值 |
|----------|------|------|--------|
| color | String | 背景颜色，支持HEX格式 | "#FFFFFF" |
| opacity | Number | 透明度，取值范围0-1 | 1 |
| borderRadius | Number | 圆角半径 | 0 |
| borderWidth | Number | 边框宽度 | 0 |
| borderColor | String | 边框颜色，支持HEX格式 | "#000000" |

## 七、组件特有属性

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
  "rowHeight": 300,          // 行高
  "gap": 10,                 // 间距
  "children": [
    // 网格项
  ]
}
```

## 八、事件绑定

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
| onChange | 值变化事件 | Input组件 |
| onFocus | 获得焦点事件 | Input组件 |
| onBlur | 失去焦点事件 | Input组件 |

## 九、动画配置

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

## 十、模块化设计

YUI框架支持将复杂UI拆分为独立模块，通过`source`字段引用外部JSON文件：

```json
{
  "id": "user_form",
  "type": "Panel",
  "source": "components/user_form.json" // 外部模块文件路径
}
```

## 十一、数据绑定

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

## 十二、示例：照片相册界面

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

## 十三、最佳实践

1. **组件命名规范**：使用有意义的ID，采用小写字母和下划线组合
2. **模块化拆分**：将复杂界面拆分为多个小型、可复用的组件
3. **合理使用样式**：统一配色方案，保持界面风格一致性
4. **事件处理分离**：UI配置与事件处理函数分离，提高代码可维护性
5. **避免过度嵌套**：控制组件层级深度，避免过深嵌套影响性能

## 十四、版本历史

| 版本 | 日期 | 更新内容 |
|------|------|----------|
| 1.0.0 | 2023-10-01 | 初始版本，支持基础组件和布局 |
| 1.1.0 | 2023-11-15 | 新增动画系统和数据绑定功能 |
| 1.2.0 | 2023-12-20 | 增强模块化支持和样式配置选项 |

## 十五、附录：颜色值参考

YUI框架支持以下颜色表示方式：

1. **HEX格式**：如`#FF0000`表示红色
2. **RGB格式**：如`rgb(255, 0, 0)`表示红色
3. **RGBA格式**：如`rgba(255, 0, 0, 0.5)`表示半透明红色

## 十六、常见问题解答

**Q: 如何实现响应式布局？**
**A:** 使用Grid组件配合flex_ratio属性，或者通过代码动态调整组件大小和位置。

**Q: 如何加载网络图片？**
**A:** 当前版本仅支持本地图片，需先下载到本地assets目录后引用。

**Q: 如何自定义动画效果？**
**A:** 通过配置animation属性，选择合适的缓动函数和动画参数。