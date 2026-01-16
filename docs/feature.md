# YUI框架功能文档描述与开发说明

## 一、框架概述

YUI是一个专为AI设计的轻量级GUI框架，采用C语言开发，通过JSON配置文件描述UI界面，具有高性能、易扩展、跨平台等特点。框架采用分层设计架构，支持丰富的组件类型和动画效果，适用于各类AI应用的界面开发。

## 二、核心功能

### 1. 分层设计架构

- **JSON解析层**：通过`layer_create_from_json()`函数递归构建图层树，实现UI与逻辑分离
- **资源管理层**：通过`load_textures()`函数异步加载纹理资源，支持多种图像格式
- **渲染管线层**：通过`render_layer()`函数实现深度优先绘制，支持复杂嵌套界面
- **事件处理层**：通过`handle_event()`函数支持事件冒泡机制，实现交互功能

### 2. 关键优化技术

- **纹理缓存**：重复资源仅加载一次，提高内存使用效率
- **脏矩形渲染**：仅重绘变更区域，提升渲染性能
- **空间分割**：优化图层点击检测效率，加速交互响应
- **组件复用**：支持模块化拆分UI，通过"source"字段引用外部JSON文件

### 3. 组件系统

YUI框架支持以下核心组件类型（定义在ytype.h中）：
- VIEW：基础视图容器
- BUTTON：按钮组件
- INPUT：输入框组件
- LABEL：文本标签组件
- IMAGE：图像组件
- LIST：列表组件
- GRID：网格布局组件
- PROGRESS：进度条组件

每个组件都支持属性设置、样式定制和事件绑定，可组合构建复杂界面。

### 4. 动画系统

YUI框架提供了完善的动画系统，支持以下功能：
- 多种动画属性：位置、大小、透明度、旋转、缩放等
- 丰富的缓动函数：支持ease_in_quad、ease_out_quad、ease_in_out_quad、ease_out_elastic等
- 动画控制：支持开始、暂停、恢复、停止等操作
- 动画填充模式：支持NONE、FORWARDS、BACKWARDS、BOTH等模式
- 动画重复设置：支持无限循环、指定次数循环、反向播放等

### 5. 布局系统

YUI框架内置布局管理器，支持灵活的界面布局：
- 支持固定尺寸布局
- 支持弹性布局（flex_ratio属性）
- 通过LayoutManager进行布局计算和调整
- 支持父子组件嵌套布局

## 三、开发环境配置

### 1. 依赖项

YUI框架依赖以下外部库：
- SDL2：用于窗口创建和基础绘图
- SDL2_ttf：用于文本渲染
- SDL2_image：用于图像加载
- cJSON：用于JSON解析
- 可选：webp.framework、avif.framework等用于额外图像格式支持

### 2. 环境变量设置

在运行YUI应用前，需要设置以下环境变量：
```bash
# 设置动态库搜索路径
export DYLD_FRAMEWORK_PATH=../libs
export DYLD_LIBRARY_PATH="../libs"

# 移除macOS安全隔离属性（首次运行时可能需要）
xattr -dr com.apple.quarantine ../libs/avif.framework
xattr -dr com.apple.quarantine ../libs/webp.framework
xattr -dr com.apple.quarantine ../libs/SDL2.framework
xattr -dr com.apple.quarantine ../libs/SDL2_ttf.framework/
xattr -dr com.apple.quarantine ../libs/SDL2_image.framework/
```

## 四、项目结构说明

YUI框架采用清晰的项目结构，主要包含以下部分：

### 1. 源代码目录（src/）

核心功能实现，包含以下关键文件：
- **ytype.h**：定义基础数据类型和组件类型
- **layer.h/.c**：图层系统实现，是框架的核心
- **render.h/.c**：渲染系统实现
- **backend.h/.c**：后端抽象层，对接具体渲染API
- **event.h/.c**：事件系统实现
- **animate.h/.c**：动画系统实现
- **layout.h/.c**：布局系统实现
- **util.h/.c**：工具函数集合
- **main.c**：应用入口点

### 2. 应用目录（app/）

存放应用配置和UI定义文件，如：
- **test.json**：示例UI配置文件
- **test-progress.json**：进度条组件示例
- **assets/**：存放图像、字体等资源文件

### 3. 文档目录（docs/）

存放框架文档，如：
- **feature.md**：功能文档
- **READ.md**：使用说明

### 4. 构建脚本

- **ya.py**：使用Python实现的构建系统

## 五、核心API说明

### 1. 应用初始化与运行

```c
// 初始化后端
backend_init();

// 解析UI配置
Layer* root = layer_create_from_json(json_string);

// 加载资源
load_textures(root);
load_font(root);

// 主循环
while (running) {
    // 处理事件
handle_event(event);
    
    // 更新布局
    layout_layer(root);
    
    // 渲染界面
    render_layer(root);
}

// 清理资源
destroy_layer(root);
backend_destroy();
```

### 2. 图层操作API

```c
// 创建图层
Animation* animation_create(float duration, float (*easing_func)(float));

// 设置动画目标
void animation_set_target(Animation* animation, AnimationProperty property, float value);

// 控制动画
void animation_start(Layer* layer, Animation* animation);
void animation_stop(Layer* layer);
void animation_pause(Layer* layer);
void animation_resume(Layer* layer);
void animation_update(Layer* layer, float delta_time);
```

### 3. 事件处理API

```c
// 注册事件处理器
int register_event_handler(const char* name, EventHandler handler);

// 查找事件处理器
EventHandler find_event_by_name(const char* name);

// 处理事件
bool handle_event(Event* event);
```

## 六、UI配置文件结构

YUI使用JSON格式定义UI界面，基本结构如下：

### 1. 主配置文件

```json
{
    "type":"main",
    "assets":"app/assets",    // 资源目录
    "font":"Roboto-Regular.ttf",  // 字体文件
    "fontSize":"16",          // 字体大小
    "source":"app/test.json"   // UI定义文件
}
```

### 2. UI定义文件

```json
{
  "id": "main_panel",         // 组件ID
  "type": "Panel",           // 组件类型
  "position": [0, 0],         // 位置
  "size": [800, 600],         // 大小
  "style": {                  // 样式设置
    "color": "#2C3E50"
  },
  "events": {                 // 事件绑定
    "onClick": "handlePanelClick"
  },
  "children": [               // 子组件
    // 子组件定义...
  ],
  "metadata": {               // 元数据
    "version": "1.0.0",
    "dependencies": []
  }
}
```

## 七、动画配置说明

YUI支持丰富的动画配置选项：

```json
{
  "id": "animatedButton",
  "type": "Button",
  "animation": {
    "duration": 1.0,         // 动画时长（秒）
    "easing": "easeOutElastic",  // 缓动函数
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

## 八、模块化设计

YUI支持将复杂UI拆分为独立模块，通过"source"字段引用外部JSON文件：

```json
{
  "id": "userForm",
  "type": "Form",
  "source": "components/user_form.json" // 外部模块化设计
}
```

## 九、开发最佳实践

### 1. 性能优化建议

- 合理使用组件层级，避免过深嵌套
- 对静态内容使用缓存策略
- 适当使用脏矩形标记，减少不必要的重绘
- 对于大量相似元素，考虑使用对象池模式

### 2. 代码组织建议

- 遵循框架的分层设计原则
- 将UI定义与业务逻辑分离
- 使用事件驱动模式处理交互
- 合理封装可复用组件

### 3. 调试技巧

- 使用日志记录关键操作和状态
- 利用框架提供的调试工具查看图层树
- 关注内存使用，及时释放不再使用的资源
- 对于复杂动画，可先简化测试再逐步完善

## 十、构建与运行

YUI使用自定义的构建系统（ya.py），构建和运行命令如下：

```bash
# 构建项目
yaa

# 运行项目
ya -r yui
```

## 十一、后续扩展方向

- 支持更多UI组件类型
- 增强布局系统，支持更复杂的布局需求
- 优化渲染性能，支持硬件加速
- 增加主题系统，支持换肤功能
- 提供可视化UI编辑器

---

## 附录：已有的分层设计说明

JSON解析层： layer_create_from_json()  递归构建图层树

资源管理层： load_textures()  异步加载纹理资源

渲染管线层： render_layer()  实现深度优先绘制

事件处理层： handle_event()  支持事件冒泡机制

关键优化点

纹理缓存：重复资源仅加载一次

脏矩形渲染：仅重绘变更区域（示例未展示）

空间分割：优化图层点击检测效率


quickjs 支持


使用c实现一套叫yui的json解析，直接使用json库，根据上面的json ui 编写一个ui backend 渲染指出
如何设计JSON资源文件的结构，才能更好地支持动态UI更新？
1、​​树状层级结构​​


2、模块化拆分


二、组件化声明：属性与样式分离

三、事件驱动机制：动态交互核心



将复杂UI拆分为独立模块，通过"source"字段引用外部JSON文件：
{
  "id": "userForm",
  "type": "Form",
  "source": "components/user_form.json" // 外部模块化设计
}



```json
{
  "type": "Button",
  "position": [300, 250],
  "size": [200, 100],
  "events": {
    "onClick": "handleButtonClick"
  }
}


{
  "id": "main_panel",
  "type": "Panel",
  "position": [0, 0],
  "size": [800, 600],
  "style": {
    "color": "#2C3E50"
  },
  "children": [
    {
      "id": "header",
      "type": "Header",
      "position": [50, 20],
      "size": [700, 60],
      "style": {
        "color": "#3498DB"
      }
    },
    {
      "id": "submit_btn",
      "type": "Button",
      "position": [350, 500],
      "size": [100, 50],
      "style": {
        "color": "#27AE60"
      },
      "events": {
        "onClick": "@handleSubmit"
      }
    },
    {
      "id": "logo",
      "type": "Image",
      "position": [300, 100],
      "size": [200, 200],
      "source": "assets/logo.bmp"
    }
  ]
}


{
  "id": "userFormPanel",
  "type": "Panel",
  "theme": "default",
  "eventHandlers": {
    "@submitForm": "userService.saveData",
    "@validateEmail": "utils.checkEmailFormat"
  },
  "children": [
    {
      "id": "emailInput",
      "type": "Input",
      "label": "邮箱",
      "placeholder": "输入邮箱",
      "events": {
        "onChange": "@validateEmail"
      },
      "dataBinding": {
        "path": "user.email"
      }
    },
    {
      "id": "submitBtn",
      "type": "Button",
      "text": "提交",
      "events": {
        "onClick": "@submitForm"
      }
    }
  ],
  "metadata": {
    "version": "1.0.2",
    "dependencies": ["userService", "utils"]
  }
}




       
```



xattr -dr com.apple.quarantine ../libs/avif.framework
xattr -dr com.apple.quarantine ../libs/webp.framework
xattr -dr com.apple.quarantine ../libs/SDL2.framework
xattr -dr com.apple.quarantine ../libs/SDL2_ttf.framework/
xattr -dr com.apple.quarantine ../libs/SDL2_image.framework/


export DYLD_FRAMEWORK_PATH=../libs
export DYLD_LIBRARY_PATH="../libs"