# YUI Playground API 使用文档

## 简介

本文档介绍 YUI Playground 的 Flask API 接口，支持增量和全量两种更新模式。

## 快速开始

### 1. 安装依赖

```bash
cd app/playground
pip install -r requirements.txt
```

### 2. 启动服务器

```bash
python api-server.py
```

服务器将在 `http://localhost:5000` 运行。

### 3. 在 json-editor.js 中使用

在 `json-editor.js` 中引入 API 客户端：

```javascript
// 在文件开头引入
load("api-client.js");

// 修改 sendMessage 函数
function sendMessage() {
    YUI.log("sendMessage: Sending message...");
    
    var messageText = YUI.getText("messageInput");
    
    if (!messageText || messageText.trim() === "") {
        YUI.log("sendMessage: Empty message, not sending");
        return;
    }
    
    YUI.log("sendMessage: Message content: " + messageText);
    
    // 检查编辑器中的JSON是否有效
    var jsonText = YUI.getText("jsonEditor");
    var json = null;
    try {
        json = JSON.parse(jsonText);
    } catch (e) {
        YUI.log("sendMessage: JSON is invalid - " + e.message);
        YUI.setText("previewLabel", "错误：请先验证JSON内容后再发送消息");
        return;
    }
    
    // 使用增量更新模式
    APIClient.sendMessageIncremental(messageText, json, function(response) {
        // 应用更新到UI
        var updateString = JSON.stringify(response.updates);
        YUI.update(updateString);
        
        // 更新预览标签
        var currentPreviewText = YUI.getText("previewLabel");
        var newPreviewText = currentPreviewText + 
            "\n\n===== 发送的消息 =====\n" + messageText + 
            "\n\n===== 增量更新 =====\n" + JSON.stringify(response.updates, null, 2);
        YUI.setText("previewLabel", newPreviewText);
        
        YUI.log("sendMessage: Incremental update applied!");
    });
    
    // 清空输入框
    YUI.setText("messageInput", "");
}
```

## API 接口说明

### 1. 增量更新接口

**URL**: `POST /api/message/incremental`

**请求格式**:
```json
{
    "message": "用户发送的消息",
    "json": { /* 当前的JSON配置 */ }
}
```

**返回格式**:
```json
{
    "status": "success",
    "message_id": 1,
    "updates": [
        {"target": "elementId", "change": {"property": "value"}},
        ...
    ],
    "timestamp": "2025-01-20T10:30:00"
}
```

**特点**:
- 只返回发生变化的UI元素
- 节省带宽，适合频繁更新
- 使用 YUI.update() 格式

### 2. 全量更新接口

**URL**: `POST /api/message/full`

**请求格式**:
```json
{
    "message": "用户发送的消息",
    "json": { /* 当前的JSON配置 */ }
}
```

**返回格式**:
```json
{
    "status": "success",
    "message_id": 2,
    "ui_state": {
        "elementId": {"property": "value"},
        ...
    },
    "changes": [...],
    "timestamp": "2025-01-20T10:30:00"
}
```

**特点**:
- 返回完整的UI状态
- 适合需要同步全部状态的场景
- 包含变更记录

### 3. 获取消息历史

**URL**: `GET /api/history?limit=50`

**返回格式**:
```json
{
    "status": "success",
    "messages": [
        {
            "id": 1,
            "message": "测试消息",
            "timestamp": "2025-01-20T10:30:00",
            "type": "incremental"
        }
    ],
    "total_count": 100
}
```

### 4. 获取当前UI状态

**URL**: `GET /api/state`

**返回格式**:
```json
{
    "status": "success",
    "ui_state": {
        "elementId": {"property": "value"}
    }
}
```

### 5. 重置状态

**URL**: `POST /api/reset`

**返回格式**:
```json
{
    "status": "success",
    "message": "State reset to initial values"
}
```

### 6. 服务器状态

**URL**: `GET /api/status`

**返回格式**:
```json
{
    "status": "running",
    "timestamp": "2025-01-20T10:30:00",
    "message_count": 42,
    "supports_incremental": true,
    "supports_full": true
}
```

## 使用示例

### 增量更新示例

```javascript
var jsonText = YUI.getText("jsonEditor");
var json = JSON.parse(jsonText);

APIClient.sendMessageIncremental("批量更新", json, function(response) {
    // response.updates 格式: [{"target": "id", "change": {...}}]
    var updateString = JSON.stringify(response.updates);
    YUI.update(updateString);  // 直接应用到UI
});
```

### 全量更新示例

```javascript
var jsonText = YUI.getText("jsonEditor");
var json = JSON.parse(jsonText);

APIClient.sendMessageFull("全量更新", json, function(response) {
    // 将 ui_state 转换为 YUI.update 格式
    var updates = [];
    for (var key in response.ui_state) {
        updates.push({
            "target": key,
            "change": response.ui_state[key]
        });
    }
    
    var updateString = JSON.stringify(updates);
    YUI.update(updateString);
});
```

## 测试接口

可以使用 curl 测试接口：

```bash
# 测试增量更新
curl -X POST http://localhost:5000/api/message/incremental \
  -H "Content-Type: application/json" \
  -d '{"message": "批量更新", "json": {}}'

# 测试全量更新
curl -X POST http://localhost:5000/api/message/full \
  -H "Content-Type: application/json" \
  -d '{"message": "全量更新", "json": {}}'

# 获取历史
curl http://localhost:5000/api/history?limit=10
```

## 扩展说明

### 自定义更新逻辑

在 `api-server.py` 中修改以下函数来自定义更新逻辑：

- `generate_incremental_updates()` - 增量更新逻辑
- `generate_full_ui_state()` - 全量更新逻辑

### 添加新的接口

在 `api-server.py` 中添加新的路由：

```python
@app.route('/api/your-endpoint', methods=['POST'])
def your_endpoint():
    data = request.get_json()
    # 处理逻辑
    return jsonify({"status": "success", "data": {}})
```

### 生产环境部署

生产环境部署建议：

1. 使用 gunicorn 替代 Flask 内置服务器：
```bash
pip install gunicorn
gunicorn -w 4 -b 0.0.0.0:5000 api-server:app
```

2. 添加数据库支持替换内存存储
3. 添加认证机制
4. 使用环境变量配置

## 注意事项

1. **跨域支持**: 服务器已启用 CORS，支持跨域请求
2. **数据存储**: 当前使用内存存储，重启服务器会丢失数据
3. **错误处理**: 所有接口都包含错误处理，返回统一的错误格式
4. **JSON 格式**: 确保发送和接收的数据都是有效的 JSON

## 支持的更新格式

增量更新使用 YUI.update() 格式：

```json
[
    {"target": "elementId", "change": {"property": "value"}},
    {"target": "anotherId", "change": {"text": "New Text", "color": "#ff0000"}}
]
```

支持的 change 属性：
- `text`: 文本内容
- `color`: 文字颜色
- `bgColor`: 背景颜色
- `fontSize`: 字体大小
- `visible`: 是否可见
- `enable`: 是否启用
