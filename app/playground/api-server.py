#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
YUI Playground API Server
提供消息发送接口，支持增量和全量更新模式
"""

from flask import Flask, request, jsonify
from flask_cors import CORS
import json
import time
import traceback
import sys
import os
from datetime import datetime

from openai import OpenAI

# 设置 UTF-8 编码（解决 Windows 控制台 Unicode 问题）
if sys.platform == "win32":
    import codecs
    sys.stdout = codecs.getwriter('utf-8')(sys.stdout.buffer, 'strict')
    sys.stderr = codecs.getwriter('utf-8')(sys.stderr.buffer, 'strict')

# 全局禁用输出缓冲，确保日志实时显示
sys.stdout = open(sys.stdout.fileno(), 'w', buffering=1)
sys.stderr = open(sys.stderr.fileno(), 'w', buffering=1)

model= 'Pro/zai-org/GLM-4.7' #'Qwen/Qwen3-VL-235B-A22B-Instruct' #'deepseek-ai/DeepSeek-V3.2' #'Qwen/Qwen2.5-7B-Instruct' #glm-4.7-free #Qwen/Qwen2.5-7B-Instruct #gpt-5-nano

client = OpenAI(
    base_url="https://api.siliconflow.cn/v1", #"https://opencode.ai/zen/v1",
    api_key=os.getenv("OPENAI_API_KEY", ""), # export OPENAI_API_KEY=
)

app = Flask(__name__)
CORS(app)  # 启用CORS支持


full_system_prompt = """You are a professional UI designer. Based on the user's instructions, generate a complete UI JSON structure that adheres to the  specification. Ensure that the UI is user-friendly and visually appealing.

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
| LISTBOX | 列表框组件 | `"type": "List"` |
| SCROLLBAR | 滚动条组件 | `"type": "Scrollbar"` |

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
    "justifyContent": "center"        // 主轴对齐方式
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

生成json格式，生成完整得ui，只返回json结果很重要，注意要好看整洁，科技感,接口返回例子：
{
            "id": "submit_btn", // 元素ID
            "type": "Button", //View Button Input Label Image List Grid Progress Checkbox Radiobox Text Treeview Tab Slider List Scrollbar
            "size": [100, 40], // 元素尺寸 [宽, 高]
            "style": { // 样式配置,可选，正常有主题是不需要的
                "bgColor": "#4caf50", // 背景颜色
                "color": "#ffffff",   // 文字颜色
                "fontSize": 16,       // 字体大小
                "borderRadius": 5     // 边框圆角
            },
            "text": "提交", // 按钮文本
            "textAlign": "center",   // 文本对齐方式
            "layout": { // 布局配置，type是View的时候
                "type": "vertical", // 布局类型，horizontal 水平布局，vertical 垂直布局
                "spacing": 10, // 子元素间距
                "padding": [ // 内边距，顺序为上右下左
                    20,
                    20,
                    20,
                    20
                ]
            },
            children: []  // 子元素数组，
        }

"""


# 模拟数据存储（实际应用中应该使用数据库）
message_history = []
ui_state = {}  # 当前UI状态

# 初始UI状态示例
INITIAL_UI_STATE = {
    "statusLabel": {
        "text": "状态：准备就绪",
        "color": "#333333"
    },
    "item1": {
        "text": "项目 1",
        "bgColor": "#f0f0f0"
    },
    "item2": {
        "text": "项目 2",
        "bgColor": "#f0f0f0"
    },
    "item3": {
        "text": "项目 3",
        "bgColor": "#f0f0f0"
    }
}

def initialize_state():
    """初始化状态"""
    global ui_state, message_history
    ui_state = INITIAL_UI_STATE.copy()
    message_history = []

@app.route('/api/status', methods=['GET'])
def get_status():
    """获取服务器状态"""
    return jsonify({
        "status": "running",
        "timestamp": datetime.now().isoformat(),
        "message_count": len(message_history),
        "supports_incremental": True,
        "supports_full": True
    })

@app.route('/api/message/incremental', methods=['POST'])
def send_message_incremental():
    """
    增量更新接口
    只返回发生变化的UI元素更新
    
    请求格式:
    {
        "message": "用户发送的消息",
        "json": { /* 当前的JSON配置 */ }
    }
    
    返回格式:
    {
        "status": "success",
        "updates": [
            {"target": "elementId", "change": {"property": "value"}},
            ...
        ],
        "timestamp": "..."
    }
    """
    try:
        # 改进JSON解析错误处理
        try:
            data = request.get_json()
        except Exception as json_error:
            print(f"[ERROR] JSON解析失败: {str(json_error)}")
            print(f"[ERROR] 请求数据: {request.data.decode('utf-8', errors='replace')}")
            return jsonify({
                "error": "Invalid JSON format", 
                "details": str(json_error),
                "hint": "请检查JSON格式是否正确，特别是字符串是否闭合"
            }), 400
            
        if not data:
            return jsonify({"error": "Invalid JSON"}), 400
        
        message = data.get('message', '').strip()
        json_config = data.get('json', {})
        
        if not message:
            return jsonify({"error": "Message cannot be empty"}), 400
        
        print(f"[DEBUG] Received message: '{message}' (length: {len(message)})")
        
        print(f"[DEBUG] Received message: '{message}' (length: {len(message)})")
        
        # 记录消息
        message_entry = {
            "id": len(message_history) + 1,
            "message": message,
            "timestamp": datetime.now().isoformat(),
            "type": "incremental"
        }
        message_history.append(message_entry)
        
        # 根据消息内容生成增量更新
        updates = generate_incremental_updates(message, json_config)
        
        # 应用更新到状态
        apply_updates(updates)
        
        # 直接返回更新数组（符合 json-update-spec.md 规范）
        return jsonify(updates)
        
    except Exception as e:
        print(f"[ERROR] Exception in /api/message/incremental: {str(e)}")
        print(traceback.format_exc())
        return jsonify({"error": str(e)}), 500

@app.route('/api/message/full', methods=['POST'])
def send_message_full():
    """
    全量更新接口
    返回完整的UI状态
    
    请求格式:
    {
        "message": "用户发送的消息",
        "json": { /* 当前的JSON配置 */ }
    }
    
    返回格式:
    {
        "status": "success",
        "ui_state": { /* 完整的UI状态 */ },
        "updates": [...],  // 可选，变更记录
        "timestamp": "..."
    }
    """
    global ui_state
    try:
        # 改进JSON解析错误处理
        try:
            data = request.get_json()
        except Exception as json_error:
            print(f"[ERROR] JSON解析失败: {str(json_error)}")
            print(f"[ERROR] 请求数据: {request.data.decode('utf-8', errors='replace')}")
            return jsonify({
                "error": "Invalid JSON format",
                "details": str(json_error),
                "hint": "请检查JSON格式是否正确，特别是字符串是否闭合"
            }), 400
            
        if not data:
            return jsonify({"error": "Invalid JSON"}), 400
        
        

        message = data.get('message', '').strip()
        json_config = data.get('json', {})
        
        if not message:
            return jsonify({"error": "Message cannot be empty"}), 400
        
        print(f"[DEBUG] Received message: '{message}' (length: {len(message)})")
        
        print(f"[DEBUG] Received message: '{message}' (length: {len(message)})")
        
        # 记录消息
        message_entry = {
            "id": len(message_history) + 1,
            "message": message,
            "timestamp": datetime.now().isoformat(),
            "type": "full"
        }
        message_history.append(message_entry)
        
        # 生成完整的UI状态
        new_ui_state = generate_full_ui_state(message, json_config)
        
        # 计算变更
        changes = calculate_changes(ui_state, new_ui_state)
        
        # 更新状态
        ui_state = new_ui_state
        
        # 直接返回完整的 UI JSON 对象（符合 json-format-spec.md 规范）
        return jsonify(ui_state)
        
    except Exception as e:
        print(f"[ERROR] Exception in /api/message/full: {str(e)}")
        print(traceback.format_exc())
        return jsonify({"error": str(e)}), 500

@app.route('/api/history', methods=['GET'])
def get_history():
    """获取消息历史"""
    limit = request.args.get('limit', 50, type=int)
    return jsonify({
        "status": "success",
        "messages": message_history[-limit:],
        "total_count": len(message_history)
    })

@app.route('/api/state', methods=['GET'])
def get_state():
    """获取当前UI状态"""
    return jsonify({
        "status": "success",
        "ui_state": ui_state
    })

@app.route('/api/reset', methods=['POST'])
def reset_state():
    """重置状态到初始值"""
    initialize_state()
    return jsonify({
        "status": "success",
        "message": "State reset to initial values"
    })

def generate_incremental_updates(message, json_config):
    """
    根据消息内容生成增量更新
    这是一个示例实现，实际应用中可以根据业务逻辑定制
    """
    updates = []
    lower_msg = message.lower()
    
    

    try:
        print(f"[DEBUG] Calling AI API with model: {model}")
        print(f"[DEBUG] Message: {message}")
        
        completion = client.chat.completions.create(
            model =model,
            messages=[
                {"role": "user", "content": f"生成{message},基本的布局，简洁的json输出" }
            ]
        )

        print(f"[DEBUG] AI API call succeeded")

        # 获取推理思考过程（Reasoner特有字段，可能不存在）
        if hasattr(completion.choices[0].message, 'reasoning_content'):
            reasoning_content = completion.choices[0].message.reasoning_content
            print(f"===== 模型推理过程 =====\n{reasoning_content}")

        # 获取模型回复内容
        content = completion.choices[0].message.content
        print(f"===== 模型回复 =====\n{content}")

        # 尝试解析为JSON数组
        try:
            updates = json.loads(content) if isinstance(content, str) else content
            if not isinstance(updates, list):
                print(f"[ERROR] Parsed content is not a list, using empty updates")
                updates = []
        except json.JSONDecodeError as json_err:
            print(f"[ERROR] Failed to parse model response as JSON: {json_err}")
            print(f"[ERROR] Response content: {content}")
            updates = []
    except Exception as ai_err:
        print(f"[ERROR] AI API call failed: {type(ai_err).__name__}: {str(ai_err)}")
        print(traceback.format_exc())
        updates = []
    # 关键词到更新的映射
    if "批量" in message or "batch" in lower_msg:
        updates = [
            {"target": "statusLabel", "change": {"text": "状态：批量更新完成", "color": "#2196f3"}},
            {"target": "item1", "change": {"text": "批量更新 - 项目 1", "bgColor": "#2196f3"}},
            {"target": "item2", "change": {"text": "批量更新 - 项目 2", "bgColor": "#ff9800"}},
            {"target": "item3", "change": {"text": "批量更新 - 项目 3", "bgColor": "#9c27b0"}}
        ]
    elif "项目1" in message or "item1" in lower_msg:
        updates = [
            {"target": "statusLabel", "change": {"text": "状态：项目1已更新", "color": "#4caf50"}},
            {"target": "item1", "change": {"text": f"已更新: {message}", "bgColor": "#4caf50"}}
        ]
    elif "项目2" in message or "item2" in lower_msg:
        updates = [
            {"target": "statusLabel", "change": {"text": "状态：项目2已更新", "color": "#ff9800"}},
            {"target": "item2", "change": {"text": f"已更新: {message}", "bgColor": "#ff9800"}}
        ]
    elif "重置" in message or "reset" in lower_msg:
        updates = [
            {"target": "statusLabel", "change": {"text": "状态：已重置", "color": "#333333"}},
            {"target": "item1", "change": {"text": "项目 1", "bgColor": "#f0f0f0"}},
            {"target": "item2", "change": {"text": "项目 2", "bgColor": "#f0f0f0"}},
            {"target": "item3", "change": {"text": "项目 3", "bgColor": "#f0f0f0"}}
        ]
    else:
        # 默认更新状态标签
        updates = [
            {"target": "statusLabel", "change": {"text": f"状态：收到消息 - {message[:20]}...", "color": "#673ab7"}}
        ]
    
    return updates

def generate_full_ui_state(message, json_config):
    """
    生成完整的UI状态（符合 json-format-spec.md 规范的组件树结构）
    """
    lower_msg = message.lower()
    print(f"Processing message: {message}")

    try:
        print(f"[DEBUG] Calling AI API with model: {model}")
        print(f"[DEBUG] Message length: {len(message)}")
        
        completion = client.chat.completions.create(
            model =model,
            messages=[
                {
                    "role": "system",
                    "content": full_system_prompt
                },
                {"role": "user", "content": message}  # 移除多余的""+
            ]
        )

        print(f"[DEBUG] AI API call succeeded")

        # 获取推理思考过程（Reasoner特有字段，可能不存在）
        if hasattr(completion.choices[0].message, 'reasoning_content'):
            reasoning_content = completion.choices[0].message.reasoning_content
            print(f"===== 模型推理过程 =====\n{reasoning_content}")

        # 获取模型回复内容
        content = completion.choices[0].message.content
        print(f"===== 模型回复 =====\n{content}")
        content=content.replace('```json','').replace('```','')
        # 尝试解析为JSON对象
        try:
            base_ui = json.loads(content) if isinstance(content, str) else content
            if not isinstance(base_ui, dict):
                print(f"[ERROR] Parsed content is not a dict, using default UI")
                base_ui = {
                    "id": "root",
                    "type": "View",
                    "size": [400, 600], 
                    "style": {"bgColor": "#f5f5f5"},
                    "children": []
                }
        except json.JSONDecodeError as json_err:
            print(f"[ERROR] Failed to parse model response as JSON: {json_err}")
            print(f"[ERROR] Response content: {content}")
            base_ui = {
                "id": "root",
                "type": "View",
                "size": [400, 600], 
                "style": {"bgColor": "#f5f5f5"},
                "children": []
            }
    except Exception as ai_err:
        print(f"[ERROR] AI API call failed: {type(ai_err).__name__}: {str(ai_err)}")
        print(traceback.format_exc())
        base_ui = {
            "id": "root",
            "type": "View",
            "size": [400, 600], 
            "style": {"bgColor": "#f5f5f5"},
            "children": []
        }
        
    return base_ui

def apply_updates(updates):
    """应用增量更新到状态"""
    global ui_state
    for update in updates:
        target = update.get("target")
        change = update.get("change", {})
        
        if target:
            if target not in ui_state:
                ui_state[target] = {}
            ui_state[target].update(change)

def calculate_changes(old_state, new_state):
    """计算两个状态之间的差异"""
    changes = []
    
    for key, new_value in new_state.items():
        old_value = old_state.get(key, {})
        if old_value != new_value:
            changes.append({
                "target": key,
                "old": old_value,
                "new": new_value
            })
    
    return changes

if __name__ == '__main__':
    # 初始化状态
    initialize_state()
    
    # 启动服务器
    print("=" * 60)
    print("YUI Playground API Server")
    print("=" * 60)
    print("Server is running on http://localhost:5000")
    print("\nAvailable endpoints:")
    print("  POST /api/message/incremental  - 增量更新接口")
    print("  POST /api/message/full         - 全量更新接口")
    print("  GET  /api/history              - 获取消息历史")
    print("  GET  /api/state                - 获取当前UI状态")
    print("  POST /api/reset                - 重置状态")
    print("  GET  /api/status               - 服务器状态")
    print("=" * 60)
    
    app.run(debug=True, host='0.0.0.0', port=5000)
