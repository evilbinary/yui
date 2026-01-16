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
from datetime import datetime

app = Flask(__name__)
CORS(app)  # 启用CORS支持

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
        data = request.get_json()
        if not data:
            return jsonify({"error": "Invalid JSON"}), 400
        
        message = data.get('message', '').strip()
        json_config = data.get('json', {})
        
        if not message:
            return jsonify({"error": "Message cannot be empty"}), 400
        
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
        
        return jsonify({
            "status": "success",
            "message_id": message_entry["id"],
            "updates": updates,
            "timestamp": message_entry["timestamp"]
        })
        
    except Exception as e:
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
        data = request.get_json()
        if not data:
            return jsonify({"error": "Invalid JSON"}), 400
        
        message = data.get('message', '').strip()
        json_config = data.get('json', {})
        
        if not message:
            return jsonify({"error": "Message cannot be empty"}), 400
        
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
        
        return jsonify({
            "status": "success",
            "message_id": message_entry["id"],
            "ui_state": ui_state,
            "changes": changes,
            "timestamp": message_entry["timestamp"]
        })
        
    except Exception as e:
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
    生成完整的UI状态
    实际应用中可以根据消息内容和配置生成全新的状态
    """
    lower_msg = message.lower()
    
    if "批量" in message or "batch" in lower_msg:
        return {
            "statusLabel": {"text": "状态：批量更新完成", "color": "#2196f3"},
            "item1": {"text": "批量更新 - 项目 1", "bgColor": "#2196f3"},
            "item2": {"text": "批量更新 - 项目 2", "bgColor": "#ff9800"},
            "item3": {"text": "批量更新 - 项目 3", "bgColor": "#9c27b0"}
        }
    elif "项目1" in message or "item1" in lower_msg:
        return {
            "statusLabel": {"text": "状态：项目1已更新", "color": "#4caf50"},
            "item1": {"text": f"已更新: {message}", "bgColor": "#4caf50"},
            "item2": ui_state.get("item2", {"text": "项目 2", "bgColor": "#f0f0f0"}),
            "item3": ui_state.get("item3", {"text": "项目 3", "bgColor": "#f0f0f0"})
        }
    else:
        # 默认状态
        return INITIAL_UI_STATE.copy()

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
