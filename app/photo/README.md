# Photo AI 对话应用

对接 [`shadowkitten/cmd/photo`](../../../shadowkitten/cmd/photo) 的 HTTP 接口，在 YUI 里做摄影助手聊天。

## 功能

- 流式对话：`POST /api/chat`（`stream: true`，SSE）
- 健康检查：`GET /api/health`
- 工具列表：`GET /api/tools`
- 会话 ID 本地生成，多轮上下文由后端 `session_id` 维护
- 可选图片 URL（绝对地址或后端 `/uploads/photo/...`）随消息作为 `files` 发送

## 启动后端

在 shadowkitten 仓库：

```bash
# HTTP 默认会找可用端口，当前常见为 :8082
go run ./cmd/photo -http
# 或你项目里实际的启动方式（http / http+cli）
```

确认：

```bash
curl http://127.0.0.1:8082/api/health
```

## 启动本应用

```bash
# 在 yui 仓库
ya -b photo
ya -r photo
# 或
./build/None/None/None/photo.exe app/photo/app.json
```

Makefile：

```bash
make photo
```

## 界面说明

手机尺寸 **390×844**，微信式对话气泡：

| 区域 | 说明 |
|------|------|
| 顶栏 | 标题 / 在线状态 / 检测·新会话·清空 |
| API 行 | 服务地址 + 可选图片 URL |
| 对话区 | 头像 + 气泡（多行换行）+ 时间；左侧 AI/系统，右侧「我」 |
| 快捷栏 | 点评 / 策划 / 灵感 / 小红书 |
| 输入栏 | 圆角输入框 + 发送 |

头像：我 👤 / Photo AI 📷 / 系统 💡 / 错误 ⚠️  
文字：只读 `Text`（`multiline`），按内容高度自适应气泡，不显示内部滚动条。  
字体：`simhei.ttf`（中文黑体，放在 `app/assets`），emoji 回退 `NotoEmoji-Regular.ttf`。

默认 API：`http://127.0.0.1:8082`。

## API 请求示例

```json
POST /api/chat
{
  "message": "请点评这张摄影图片",
  "session_id": "photo_xxx",
  "stream": true,
  "files": [
    { "type": "image", "url": "http://127.0.0.1:8082/uploads/photo/xxx.jpg" }
  ]
}
```

SSE 事件：`start` / `chunk` / `response` / `action` / `observation` / `done` / `error`。

## 文件

```
app/photo/
  main.c      # 入口
  app.json    # UI
  api.js      # PhotoAPI 客户端
  photo.js    # 交互逻辑
  README.md
```
