#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
YUI Playground API Server
提供消息发送接口，支持增量和全量更新模式
"""

from flask import Flask, request, jsonify, Response, stream_with_context
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

model= 'deepseek-v4-flash' #'Pro/zai-org/GLM-4.7' #'Qwen/Qwen3-VL-235B-A22B-Instruct' #'deepseek-ai/DeepSeek-V3.2' #'Qwen/Qwen2.5-7B-Instruct' #glm-4.7-free #Qwen/Qwen2.5-7B-Instruct #gpt-5-nano

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
        base_url= "https://api.deepseek.com", # "https://api.siliconflow.cn/v1" "https://opencode.ai/zen/v1"
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
    client.base_url = "https://api.deepseek.com"

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

full_system_prompt = """你是 YUI 框架的 UI JSON 生成助手。根据用户需求输出完整、可运行的组件树 JSON。

## 输出规则（最重要）
1. **只输出 JSON**，不要 markdown、不要解释、不要代码块、不要注释
2. **尽量短小**：省略一切非必需字段，JSON 越小越好
3. **少写 style**：默认不写 style，依赖框架/主题默认样式
   - 仅当用户明确要求颜色/字号/圆角时才写 style
   - 即使写 style，也只写 1～2 个必要项（如 bgColor 或 color），不要堆砌
4. **不要 animation**，除非用户明确要求
5. **布局优先**：用 layout（vertical/horizontal + spacing）组织界面；padding 能省则省
6. **events 按需**：只有按钮/输入等需要交互时才写 events，事件名简短即可
7. id 简短有意义；组件数够用即可，避免重复装饰性容器

## 必需字段
- 每个组件：id, type, size（或 position + size）
- View 容器：layout + children
- 颜色若必须写：用 HEX，如 #2196f3

## 支持的 type
View, Panel, Button, Input, Label, Image, List, Grid, Progress, Loading, Checkbox, Radiobox, Text, Treeview, Tab, Slider, Listbox, Scrollbar

## 精简示例（优先模仿）
{
  "id": "root",
  "type": "View",
  "size": [400, 500],
  "layout": {"type": "vertical", "spacing": 8},
  "children": [
    {"id": "title", "type": "Label", "text": "标题", "size": [0, 32]},
    {"id": "name", "type": "Input", "placeholder": "姓名", "size": [0, 36]},
    {"id": "ok", "type": "Button", "text": "确定", "size": [120, 40]}
  ]
}

## 禁止
- 不要为每个组件写 style、borderRadius、fontFamily、多层 padding
- 不要追求「科技感/好看」而增加装饰字段
- 不要输出与 UI 无关的说明文字
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

### 布局属性（View/Panel 等容器）
- layout: 布局对象，可部分更新（只写字段即覆盖该项）
  - type: vertical | horizontal
  - spacing: 子项间距（number）
  - align: left | center | right（交叉轴）
  - justify / justifyContent: left | center | right | space-between | space-around（主轴）
  - padding: [8] 或 [8,12] 或 [t,r,b,l]
  - columns: Grid 列数
- flex: 子项弹性比例（number）
- padding: 容器内边距（同 layout.padding）

布局示例：
{"target": "root", "change": {"layout": {"type": "horizontal", "spacing": 12}}}
{"target": "toolbar", "change": {"layout": {"justify": "space-between"}}}
{"target": "root", "change": {"children": [{"id": "row", "type": "View", "size": [0, 48], "layout": {"type": "horizontal", "spacing": 8}, "children": [{"id": "btn1", "type": "Button", "text": "A", "size": [80, 36]}]}]}}

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

# 增量更新 SSE 流式间隔（秒），控制 UI 逐条更新节奏
INCREMENTAL_STREAM_UPDATE_DELAY = 0.18

def _sse_event(event_type, payload):
    """格式化为 SSE 事件块"""
    return "event: {}\ndata: {}\n\n".format(
        event_type,
        json.dumps(payload, ensure_ascii=False)
    )

def _normalize_json_config(json_config):
    """将请求中的 json 字段统一为字符串（供 system prompt 使用）"""
    if json_config is None:
        return ""
    if isinstance(json_config, str):
        return json_config
    return json.dumps(json_config, ensure_ascii=False)

def _strip_markdown_json_fence(content):
    if not isinstance(content, str):
        return content
    text = content.strip()
    if text.startswith('```json'):
        text = text[7:]
    elif text.startswith('```'):
        text = text[3:]
    if text.endswith('```'):
        text = text[:-3]
    return text.strip()

def parse_incremental_content(content):
    """将模型回复解析为增量更新数组"""
    content = _strip_markdown_json_fence(content)
    if not content:
        return []
    try:
        parsed = json.loads(content) if isinstance(content, str) else content
        if isinstance(parsed, dict):
            return [parsed]
        if isinstance(parsed, list):
            return parsed
    except json.JSONDecodeError as err:
        print(f"[ERROR] Failed to parse incremental JSON: {err}")
        print(f"[ERROR] Response content: {content}")
    return []

def _stream_incremental_updates(message, json_config):
    """SSE 生成器：先打字机输出 token，再逐条推送 update 事件"""
    lower_msg = message.lower()
    config_text = _normalize_json_config(json_config)
    full_content = ""

    try:
        print(f"[DEBUG] SSE stream calling AI API with model: {model}")
        stream = client.chat.completions.create(
            model=model,
            messages=[
                {"role": "system", "content": config_text + ' ' + incremental_system_prompt},
                {"role": "user", "content": message}
            ],
            stream=True,
        )

        for chunk in stream:
            choice = chunk.choices[0]
            delta = ""
            if choice.delta and getattr(choice.delta, "content", None):
                delta = choice.delta.content
            if delta:
                full_content += delta
                yield _sse_event("token", {"delta": delta})

        print(f"[DEBUG] SSE stream completed, content length: {len(full_content)}")
    except Exception as ai_err:
        print(f"[ERROR] SSE AI stream failed: {type(ai_err).__name__}: {ai_err}")
        print(traceback.format_exc())
        yield _sse_event("error", {"error": str(ai_err)})
        full_content = ""

    updates = parse_incremental_content(full_content)
    if not updates:
        updates = _fallback_incremental_updates(message, lower_msg)
        preview = json.dumps(updates, ensure_ascii=False)
        for ch in preview:
            yield _sse_event("token", {"delta": ch})
            time.sleep(0.008)

    for i, update in enumerate(updates):
        apply_updates([update])
        payload = dict(update)
        payload['index'] = i + 1
        payload['total'] = len(updates)
        yield _sse_event("update", payload)
        time.sleep(INCREMENTAL_STREAM_UPDATE_DELAY)

    yield _sse_event("done", {"count": len(updates), "message": message})

@app.route('/api/message/incremental/stream', methods=['POST'])
def send_message_incremental_stream():
    """增量更新 SSE 流式接口：token 打字机 + 逐条 update"""
    try:
        try:
            data = request.get_json()
        except Exception as json_error:
            return jsonify({
                "error": "Invalid JSON format",
                "details": str(json_error),
            }), 400

        if not data:
            return jsonify({"error": "Invalid JSON"}), 400

        message = data.get('message', '').strip()
        json_config = data.get('json', {})

        if not message:
            return jsonify({"error": "Message cannot be empty"}), 400

        message_entry = {
            "id": len(message_history) + 1,
            "message": message,
            "timestamp": datetime.now().isoformat(),
            "type": "incremental_stream"
        }
        message_history.append(message_entry)

        return Response(
            stream_with_context(_stream_incremental_updates(message, json_config)),
            mimetype='text/event-stream',
            headers={
                'Cache-Control': 'no-cache',
                'Connection': 'keep-alive',
                'X-Accel-Buffering': 'no',
            }
        )
    except Exception as e:
        print(f"[ERROR] Exception in /api/message/incremental/stream: {e}")
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

        content = completion.choices[0].message.content
        print(f"===== 模型回复 =====\n{content}")
        updates = parse_incremental_content(content)
            
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
            return [{"target": "listContainer", "change": {"children.0": None}}]
        elif "清空" in message or "clear" in lower_msg:
            return [{"target": "listContainer", "change": {"children": None}}]
        else:
            return [{"target": "tempElement", "change": {"tempElement": None}}]
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
    print("  POST /api/message/incremental         - 增量更新接口")
    print("  POST /api/message/incremental/stream  - 增量更新 SSE 流式接口")
    print("  POST /api/message/full         - 全量更新接口")
    print("  GET  /api/history              - 获取消息历史")
    print("  GET  /api/state                - 获取当前UI状态")
    print("  POST /api/reset                - 重置状态")
    print("  GET  /api/status               - 服务器状态")
    print("=" * 60)
    
    app.run(debug=True, host='0.0.0.0', port=5000)
