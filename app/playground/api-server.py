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

import sys
print(sys.getdefaultencoding())  # 应该是 'utf-8'
print(sys.stdout.encoding)       # 应该是 'utf-8'

import sys
import locale

# 强制使用 UTF-8
sys.stdin.reconfigure(encoding='utf-8')
sys.stdout.reconfigure(encoding='utf-8')
sys.stderr.reconfigure(encoding='utf-8')
locale.setlocale(locale.LC_ALL, 'en_US.UTF-8')


# 设置 UTF-8 编码（解决 Windows 控制台 Unicode 问题）
if sys.platform == "win32":
    import codecs
    sys.stdout = codecs.getwriter('utf-8')(sys.stdout.buffer, 'strict')
    sys.stderr = codecs.getwriter('utf-8')(sys.stderr.buffer, 'strict')

# 全局禁用输出缓冲，确保日志实时显示
sys.stdout = open(sys.stdout.fileno(), 'w', buffering=1)
sys.stderr = open(sys.stderr.fileno(), 'w', buffering=1)

model= 'Pro/zai-org/GLM-4.7' #'Qwen/Qwen3-VL-235B-A22B-Instruct' #'deepseek-ai/DeepSeek-V3.2' #'Qwen/Qwen2.5-7B-Instruct' #glm-4.7-free #Qwen/Qwen2.5-7B-Instruct #gpt-5-nano

# 尝试使用 httpx 客户端初始化 OpenAI
import httpx

# 检查是否有代理环境变量
http_proxy = os.environ.get('HTTP_PROXY') or os.environ.get('http_proxy')
https_proxy = os.environ.get('HTTPS_PROXY') or os.environ.get('https_proxy')

# 创建 httpx 客户端，考虑代理设置
if http_proxy or https_proxy:
    # 如果有代理设置
    proxies = {}
    if http_proxy:
        proxies['http://'] = http_proxy
    if https_proxy:
        proxies['https://'] = https_proxy
    
    http_client = httpx.Client(proxies=proxies)
    print("Using proxy configuration")
else:
    # 如果没有代理设置
    http_client = httpx.Client()
    print("No proxy configuration detected")

try:
    # 使用自定义 httpx 客户端初始化 OpenAI
    client = OpenAI(
        base_url="https://api.siliconflow.cn/v1", #"https://opencode.ai/zen/v1",
        api_key=os.getenv("OPENAI_API_KEY", ""), # export OPENAI_API_KEY=
        http_client=http_client
    )
except Exception as e:
    print(f"Failed to initialize OpenAI client with custom httpx client: {e}")
    print("Trying with default client...")
    
    # 最后尝试最基本的初始化
    client = OpenAI(
        api_key=os.getenv("OPENAI_API_KEY", ""),
    )
    # 手动设置 base_url
    client.base_url = "https://api.siliconflow.cn/v1"

app = Flask(__name__)
CORS(app)  # 启用CORS支持
app.config['JSON_AS_ASCII'] = False  # 关键设置！
# 关键配置：支持中文
app.config.update(
    JSON_AS_ASCII=False,          # JSON 输出不转义中文
    JSONIFY_PRETTYPRINT_REGULAR=True,
    JSON_SORT_KEYS=False,
    ENV='development',
    DEBUG=True
)

full_system_prompt = """你是一个ui 生成助手，生成json格式，生成完整得ui，
请根据以下需求，生成符合YUI框架规范的JSON格式UI定义文件。

**框架规范要求：**
- 组件类型：View, Panel, Button, Input, Label, Image, List, Grid, Progress, Checkbox, Radiobox, Text, Treeview, Tab, Slider, Listbox, Scrollbar
- 必需属性：id, type, rect或position+size
- 颜色格式：支持RGBA格式，如 rgba(255,255,255,255) 或 HEX格式 #FFFFFF
- 布局方式：绝对定位 [x, y, width, height] 或 Grid网格布局
- 事件支持：onClick, onMouseEnter, onMouseLeave, onChange, onFocus, onBlur
- 动画支持：duration, easing, properties等动画配置

**生成要求：**
1. 严格按照JSON格式输出，确保语法正确
2. 包含完整的组件层级结构
3. 使用合理的样式和布局
4. 包含必要的事件绑定
5. 遵循YUI框架的命名规范
6. 不需要特地带样式style

**请根据以下具体需求生成UI定义：**

[在此处描述具体的界面需求，例如：]

需求示例1：生成一个用户登录界面，包含标题、用户名输入框、密码输入框、登录按钮、记住密码复选框
需求示例2：生成一个照片相册界面，使用Grid布局显示照片，包含标题栏和底部导航
需求示例3：生成一个系统设置面板，使用Tab组件组织多个设置页面
需求示例4：生成一个商品列表页面，支持搜索、筛选和分页功能

**组件属性参考：**

按钮组件：
{
    "id": "button_id",
    "type": "Button",
    "position": [x, y],
    "size": [width, height],
    "text": "按钮文本",
    "style": {
    "color": "#3498DB",
    "textColor": "#FFFFFF",
    "fontSize": 16,
    "radius": 5
    },
    "events": {
    "onClick": "handleClick"
    }
}
输入框组件：
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

# 增量更新系统提示词（严格遵循 json-update-spec.md 规范）
incremental_system_prompt = """你是一个专业的YUI框架增量更新助手，专门生成符合JSON增量更新规范的更新指令。

请根据用户的自然语言指令，生成严格符合以下规范的增量更新JSON：

## 核心规范

1. **统一格式**：{ "target": "id", "change": {...} }
2. **路径支持**：children.0、children.id、layerId.a.b.c
3. **null语义**：null表示删除，不存在则添加，存在则更新

## 基本格式

### 单个更新
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
  }
]
```

## 支持的操作

### 属性更新
- text: 文本内容
- label: 标签文本
- color: 文字颜色 (HEX格式)
- bgColor: 背景颜色 (HEX格式)
- fontSize: 字体大小
- borderRadius: 圆角半径
- size: [宽度, 高度]
- position: [X坐标, Y坐标]
- visible: 是否可见 (boolean)
- enabled: 是否启用 (boolean)

### 删除操作
1. 删除属性：{ "target": "element", "change": {"visible": null} }
2. 删除元素：{ "target": "elementToRemove", "change": {"elementToRemove": null} }
3. 删除子元素：{ "target": "parent", "change": {"children.0": null} }
4. 清空容器：{ "target": "container", "change": {"children": null} }

## 路径操作示例

- 直接ID："target": "button1"
- 数组索引："target": "container.children.0"
- 嵌套属性："target": "panel.header.title"
- 子元素ID："target": "list.children.item1"

## 重要规则

1. **严格遵守格式**：必须使用 target + change 结构
2. **路径语法**：使用点号(.)分隔层级
3. **null语义**：明确区分添加/更新 vs 删除
4. **批量操作**：多个更新用数组包装
5. **只返回JSON**：不要包含解释文字
6. 根路径是 root 下需要更新到 添加children中
7.通过简单的 JSON 格式增量更新 UI 组件，无需重新渲染整个 UI 树。
    1、统一格式：{ "target": "id", "change": {...} }
    2、路径支持：children.0、children.id、layerId.a.b.c
    3、null 表示删除，不存在则添加，存在则更新

## 示例对话

用户："把提交按钮的文本改成保存"
助手：{"target": "submitBtn", "change": {"text": "保存"}}

用户："隐藏错误提示标签"
助手：{"target": "errorLabel", "change": {"visible": false}}

用户："批量更新：标题改为新界面，按钮改为蓝色"
助手：[{"target": "titleLabel", "change": {"text": "新界面"}}, {"target": "actionBtn", "change": {"bgColor": "#2196f3"}}]

用户："删除第三个列表项"
助手：{"target": "listContainer", "change": {"children.2": null}}

用户："清空整个列表"
助手：{"target": "listContainer", "change": {"children": null}}

用户："添加新元素"
助手：{"target": "listContainer", "change": {"children": [{"id": "newPage", "type": "View"}]}}

请严格按照上述规范生成JSON更新指令，确保格式正确且语义清晰,注意第一个更新指令的target是root。"""

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
            data = request.get_json(force=True)
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
    根据消息内容生成增量更新（严格遵循 json-update-spec.md 规范）
    
    返回格式：
    - 单个更新：{"target": "id", "change": {"属性": "值"}}
    - 批量更新：[{"target": "id1", "change": {...}}, {"target": "id2", "change": {...}}]
    - 删除操作：{"target": "id", "change": {"属性": null}}
    - 路径操作：{"target": "parent.children.0", "change": {...}}
    """
    updates = []
    lower_msg = message.lower()
    
    try:
        print(f"[DEBUG] Calling AI API with model: {model}")
        print(f"[DEBUG] Message: {message}")
        
        completion = client.chat.completions.create(
            model=model,
            messages=[
                {
                    "role": "system", 
                    "content": json_config+' '+incremental_system_prompt
                },
                {"role": "user", "content": message}
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

        # 处理可能包含的Markdown代码块标记
        if isinstance(content, str):
            # 移除可能的Markdown代码块标记
            if content.startswith('```json'):
                content = content[7:]  # 移除开头的```json
            if content.startswith('```'):
                content = content[3:]   # 移除开头的```
            if content.endswith('```'):
                content = content[:-3]  # 移除结尾的```
            content = content.strip()  # 移除前后空白

        # 尝试解析为JSON（可能是对象或数组）
        try:
            parsed_result = json.loads(content) if isinstance(content, str) else content
            # 标准化为数组格式返回
            if isinstance(parsed_result, dict):
                # 单个更新对象转为数组
                updates = [parsed_result]
            elif isinstance(parsed_result, list):
                # 已经是数组格式
                updates = parsed_result
            else:
                print(f"[ERROR] Parsed content is neither dict nor list, using fallback")
                updates = []
        except json.JSONDecodeError as json_err:
            print(f"[ERROR] Failed to parse model response as JSON: {json_err}")
            print(f"[ERROR] Response content: {content}")
            updates = []
            
    except Exception as ai_err:
        print(f"[ERROR] AI API call failed: {type(ai_err).__name__}: {str(ai_err)}")
        print(traceback.format_exc())
        updates = []
    
    # 如果AI没有返回有效结果，使用关键词匹配的回退方案
    if not updates:
        updates = _fallback_incremental_updates(message, lower_msg)
    
    return updates

def _fallback_incremental_updates(message, lower_msg):
    """关键词匹配的回退更新生成方案"""
    if "批量" in message or "batch" in lower_msg:
        return [
            {"target": "statusLabel", "change": {"text": "状态：批量更新完成", "color": "#2196f3"}},
            {"target": "item1", "change": {"text": "批量更新 - 项目 1", "bgColor": "#2196f3"}},
            {"target": "item2", "change": {"text": "批量更新 - 项目 2", "bgColor": "#ff9800"}},
            {"target": "item3", "change": {"text": "批量更新 - 项目 3", "bgColor": "#9c27b0"}}
        ]
    elif "项目1" in message or "item1" in lower_msg:
        return [
            {"target": "statusLabel", "change": {"text": "状态：项目1已更新", "color": "#4caf50"}},
            {"target": "item1", "change": {"text": f"已更新: {message}", "bgColor": "#4caf50"}}
        ]
    elif "项目2" in message or "item2" in lower_msg:
        return [
            {"target": "statusLabel", "change": {"text": "状态：项目2已更新", "color": "#ff9800"}},
            {"target": "item2", "change": {"text": f"已更新: {message}", "bgColor": "#ff9800"}}
        ]
    elif "删除" in message or "remove" in lower_msg or "delete" in lower_msg:
        # 删除操作示例
        if "第一个" in message or "first" in lower_msg:
            return [{"target": "listContainer", "change": {"children.0": null}}]
        elif "清空" in message or "clear" in lower_msg:
            return [{"target": "listContainer", "change": {"children": null}}]
        else:
            return [{"target": "tempElement", "change": {"tempElement": null}}]
    elif "重置" in message or "reset" in lower_msg:
        return [
            {"target": "statusLabel", "change": {"text": "状态：已重置", "color": "#333333"}},
            {"target": "item1", "change": {"text": "项目 1", "bgColor": "#f0f0f0"}},
            {"target": "item2", "change": {"text": "项目 2", "bgColor": "#f0f0f0"}},
            {"target": "item3", "change": {"text": "项目 3", "bgColor": "#f0f0f0"}}
        ]
    else:
        # 默认更新状态标签
        return [
            {"target": "statusLabel", "change": {"text": f"状态：收到消息 - {message[:20]}...", "color": "#673ab7"}}
        ]

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
