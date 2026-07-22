/**
 * Photo AI 手机版对话
 * 气泡 + 时间戳，对接 sheep/cmd/photo
 */

var photoState = {
    sessionId: "",
    messages: [],
    msgSeq: 0,
    isLoading: false,
    streamBuffer: "",
    streamChunks: [],
    streamQueuedChars: 0,
    streamRenderTimer: null,
    streamDone: false,
    activeRequestId: 0,
    currentSection: "",
    online: false,
    streamingMsgId: "",
    imagePath: ""
};

var CHAT_WIDTH = 366;
var AVATAR_SIZE = 36;
var BUBBLE_MAX_W = 270;
var LINE_H = 22;
var TEXT_PAD_X = 24;
var TEXT_PAD_Y = 16;
var TEXT_INNER_PAD = 8; // Text 上下内边距合计（padding top+bottom）
var AI_LOADING_H = 28;
var STREAM_RENDER_INTERVAL = 20;
var MAX_RESPONSE_CHARS = 12000;

function generateSessionId() {
    return "photo_" + Date.now().toString(36) + "_" + Math.floor(Math.random() * 1e6).toString(36);
}

function shortSessionId(id) {
    if (!id) return "";
    if (id.length <= 12) return id;
    return id.substring(0, 8) + "…";
}

function pad2(n) {
    return n < 10 ? "0" + n : String(n);
}

function formatTime(ts) {
    var d = ts ? new Date(ts) : new Date();
    return pad2(d.getHours()) + ":" + pad2(d.getMinutes());
}

function displayText(text) {
    var value = String(text || "");
    // 数字键帽在部分字体里仍会退化成方框，先降级成普通编号。
    value = value.replace(/([0-9])\uFE0F?\u20E3/g, "$1.");
    if (value.length > MAX_RESPONSE_CHARS) {
        value = value.substring(0, MAX_RESPONSE_CHARS) + "\n\n[回答过长，已截断显示]";
    }
    return value;
}

function bubbleDims(msg) {
    var meta = roleMeta(msg.role);
    var isWide = (meta.label === "系统" || meta.label === "错误");
    var bubbleW = isWide ? (CHAT_WIDTH - AVATAR_SIZE - 16) : BUBBLE_MAX_W;
    var textW = bubbleW - TEXT_PAD_X;
    return { meta: meta, bubbleW: bubbleW, textW: textW };
}

function isAssistantWaiting(msg) {
    return !!msg && msg.role === "AI" && !String(msg.text || "");
}

function applyBubbleHeights(msg, textH) {
    var d = bubbleDims(msg);
    if (textH < LINE_H + TEXT_INNER_PAD) {
        textH = LINE_H + TEXT_INNER_PAD;
    }
    var bubbleH = textH + TEXT_PAD_Y;
    var bodyH = bubbleH + 20;
    var rowH = bodyH > AVATAR_SIZE ? bodyH : AVATAR_SIZE;

    YUI.update([
        {
            target: msg.id,
            change: { size: [CHAT_WIDTH, rowH] }
        },
        {
            target: msg.id + "_body",
            change: { size: [d.bubbleW, bodyH] }
        },
        {
            target: msg.id + "_bubble",
            change: { size: [d.bubbleW, bubbleH] }
        },
        {
            target: msg.id + "_text",
            change: { size: [d.textW, textH] }
        }
    ]);
    msg.renderedTextHeight = textH;
}

function syncAssistantBubbleState(msg) {
    if (!msg || msg.role !== "AI") {
        return;
    }
    var waiting = isAssistantWaiting(msg);
    YUI.update([
        {
            target: msg.id + "_loading",
            change: { visible: waiting }
        },
        {
            target: msg.id + "_text",
            change: { visible: !waiting }
        }
    ]);
}

/** 使用 Text 组件基于真实字体和换行计算的内容高度。 */
function syncBubbleHeight(msg) {
    if (isAssistantWaiting(msg)) {
        syncAssistantBubbleState(msg);
        applyBubbleHeights(msg, AI_LOADING_H);
        return;
    }
    var textH = Number(YUI.getProperty(msg.id + "_text", "contentHeight"));
    if (!isFinite(textH) || textH < 1) {
        return;
    }
    syncAssistantBubbleState(msg);
    if (msg.renderedTextHeight === textH) {
        return;
    }
    applyBubbleHeights(msg, textH);
}

function roleMeta(role) {
    if (role === "你" || role === "user") {
        return {
            label: "我",
            align: "right",
            avatar: "👤",
            avatarBg: "#5b21b6",
            bubbleBg: "#7c3aed",
            textColor: "#ffffff",
            metaColor: "#a78bfa"
        };
    }
    if (role === "AI" || role === "assistant") {
        return {
            label: "Photo AI",
            align: "left",
            avatar: "📷",
            avatarBg: "#312e81",
            bubbleBg: "#1e1e32",
            textColor: "#e0e0e0",
            metaColor: "#94a3b8"
        };
    }
    if (role === "错误" || role === "error") {
        return {
            label: "错误",
            align: "left",
            avatar: "⚠️",
            avatarBg: "#7f1d1d",
            bubbleBg: "#3a1a1a",
            textColor: "#fca5a5",
            metaColor: "#f87171"
        };
    }
    return {
        label: "系统",
        align: "left",
        avatar: "💡",
        avatarBg: "#1e293b",
        bubbleBg: "#1a1a2e",
        textColor: "#94a3b8",
        metaColor: "#64748b"
    };
}

function setStatus(text, online) {
    photoState.online = !!online;
    YUI.setText("statusBadge", text || (online ? "在线" : "离线"));
    if (online) {
        YUI.setBgColor("statusBadge", "#1a3a2a");
    } else {
        YUI.setBgColor("statusBadge", "#3a1a1a");
    }
}

function setStreamHint(text) {
    YUI.setText("streamHint", text || "");
}

function syncApiBaseFromInput() {
    var url = YUI.getText("apiUrlInput");
    if (url) {
        PhotoAPI.setBaseURL(url);
    }
}

function refreshSessionLabel() {
    YUI.setText("sessionLabel", shortSessionId(photoState.sessionId));
}

function clearChatBubbles() {
    YUI.update({
        target: "chatScroll",
        change: { children: null }
    });
}

function buildAvatarJson(msgId, meta) {
    return {
        id: msgId + "_avatar",
        type: "View",
        size: [AVATAR_SIZE, AVATAR_SIZE],
        layout: {
            type: "vertical",
            align: "center",
            justify: "center",
            padding: [0, 0, 0, 0]
        },
        style: {
            bgColor: meta.avatarBg,
            borderRadius: 18
        },
        children: [
            {
                id: msgId + "_avatar_icon",
                type: "Label",
                text: meta.avatar,
                size: [AVATAR_SIZE, AVATAR_SIZE],
                style: {
                    color: "#ffffff",
                    fontSize: 18,
                    textAlign: "center"
                }
            }
        ]
    };
}

function buildBubbleJson(msg) {
    var d = bubbleDims(msg);
    var meta = d.meta;
    var text = msg.text || "";
    var timeStr = formatTime(msg.time);
    var bubbleW = d.bubbleW;
    var textW = d.textW;
    var waiting = isAssistantWaiting(msg);
    var textH = waiting ? AI_LOADING_H : (LINE_H + TEXT_INNER_PAD);
    var bubbleH = textH + TEXT_PAD_Y;
    var bodyH = bubbleH + 20;
    var rowH = bodyH > AVATAR_SIZE ? bodyH : AVATAR_SIZE;

    var metaLabel = {
        id: msg.id + "_meta",
        type: "Label",
        text: meta.label + "  ·  " + timeStr,
        size: [bubbleW, 16],
        style: {
            color: meta.metaColor,
            fontSize: 10,
            textAlign: meta.align
        }
    };

    var bubble = {
        id: msg.id + "_bubble",
        type: "View",
        size: [bubbleW, bubbleH],
        layout: {
            type: "vertical",
            spacing: 0,
            padding: [8, 12, 8, 12]
        },
        style: {
            bgColor: meta.bubbleBg,
            borderRadius: 16
        },
        children: [
            {
                id: msg.id + "_loading",
                type: "Loading",
                visible: waiting,
                size: [56, 20],
                variant: "dots",
                speed: 1,
                style: {
                    color: meta.textColor,
                    trackColor: "transparent"
                }
            },
            {
                id: msg.id + "_text",
                type: "Text",
                text: text,
                visible: !waiting,
                multiline: true,
                editable: false,
                scrollable: 0,
                size: [textW, textH],
                style: {
                    color: meta.textColor,
                    fontSize: 14,
                    bgColor: "transparent",
                    padding: [4, 4, 4, 4]
                }
            }
        ]
    };

    var body = {
        id: msg.id + "_body",
        type: "View",
        size: [bubbleW, bodyH],
        layout: {
            type: "vertical",
            spacing: 3,
            align: meta.align
        },
        style: { bgColor: "transparent" },
        children: [metaLabel, bubble]
    };

    var avatar = buildAvatarJson(msg.id, meta);
    var children;
    if (meta.align === "right") {
        children = [body, avatar];
    } else {
        children = [avatar, body];
    }

    return {
        id: msg.id,
        type: "View",
        size: [CHAT_WIDTH, rowH],
        layout: {
            type: "horizontal",
            spacing: 8,
            padding: [2, 0, 2, 0],
            align: "left",
            justify: meta.align === "right" ? "right" : "left"
        },
        style: { bgColor: "transparent" },
        children: children
    };
}

function appendBubble(msg) {
    var json = buildBubbleJson(msg);
    var code = YUI.renderFromJson("chatScroll", JSON.stringify(json), true);
    if (typeof YUI.show === "function") {
        YUI.show(msg.id);
    }
    msg.renderedTextHeight = 0;
    syncBubbleHeight(msg);
    return code;
}

function rebuildAllBubbles() {
    clearChatBubbles();
    for (var i = 0; i < photoState.messages.length; i++) {
        appendBubble(photoState.messages[i]);
    }
}

function resizeBubble(msg) {
    syncBubbleHeight(msg);
}

function pushMessage(role, text) {
    photoState.msgSeq += 1;
    var msg = {
        id: "msg_" + photoState.msgSeq,
        role: role,
        text: text || "",
        time: Date.now()
    };
    photoState.messages.push(msg);
    appendBubble(msg);
    return msg;
}

function updateLastAssistant(text) {
    var visibleText = displayText(text);
    for (var i = photoState.messages.length - 1; i >= 0; i--) {
        if (photoState.messages[i].role === "AI") {
            photoState.messages[i].text = visibleText;
            photoState.streamingMsgId = photoState.messages[i].id;
            // Text 会根据真实字体更新 contentHeight，仅在实际高度变化时重排气泡。
            YUI.setText(photoState.messages[i].id + "_text", visibleText);
            syncBubbleHeight(photoState.messages[i]);
            return photoState.messages[i];
        }
    }
    return pushMessage("AI", visibleText);
}

function clearPendingStreamRender() {
    if (photoState.streamRenderTimer !== null) {
        clearTimeout(photoState.streamRenderTimer);
        photoState.streamRenderTimer = null;
    }
}

function flushAssistantStream() {
    clearPendingStreamRender();
    if (photoState.streamChunks.length === 0) {
        return;
    }
    photoState.streamBuffer += photoState.streamChunks.join("");
    photoState.streamChunks = [];
    photoState.streamQueuedChars = 0;
    updateLastAssistant(photoState.streamBuffer);
}

function queueAssistantChunk(chunk) {
    var value;
    var remaining;
    if (!chunk || photoState.streamBuffer.length >= MAX_RESPONSE_CHARS) {
        return;
    }
    remaining = MAX_RESPONSE_CHARS - photoState.streamBuffer.length - photoState.streamQueuedChars;
    if (remaining <= 0) {
        return;
    }
    value = String(chunk);
    if (value.length > remaining) {
        value = value.substring(0, remaining);
    }
    photoState.streamChunks.push(value);
    photoState.streamQueuedChars += value.length;
    if (photoState.streamRenderTimer === null) {
        photoState.streamRenderTimer = setTimeout(flushAssistantStream, STREAM_RENDER_INTERVAL);
    }
}

function ensureAssistantBubble() {
    var last = photoState.messages[photoState.messages.length - 1];
    if (!last || last.role !== "AI") {
        return pushMessage("AI", "");
    }
    return last;
}

function buildFilesPayload() {
    var files = [];
    if (photoState.imagePath) {
        files.push({
            type: "image",
            url: photoState.imagePath,
            name: "image"
        });
    }
    return files;
}

function onPhotoLoad() {
    photoState.activeRequestId += 1;
    clearPendingStreamRender();
    photoState.sessionId = generateSessionId();
    photoState.messages = [];
    photoState.msgSeq = 0;
    photoState.streamBuffer = "";
    photoState.streamChunks = [];
    photoState.streamQueuedChars = 0;
    photoState.streamDone = false;
    photoState.streamingMsgId = "";
    refreshSessionLabel();
    clearChatBubbles();
    setStatus("检测中…", false);
    setStreamHint("就绪 · 输入消息开始对话");

    pushMessage("系统", "欢迎使用 Photo AI。可点评照片、策划方案、搜索灵感。");

    syncApiBaseFromInput();
    onHealthCheck();
}

function onHealthCheck() {
    syncApiBaseFromInput();
    setStatus("检测中…", false);
    setStreamHint("连接 " + PhotoAPI.getBaseURL() + " …");

    PhotoAPI.health(function(res) {
        if (res && res.status === "ok") {
            setStatus("在线", true);
            setStreamHint("服务正常");
            PhotoAPI.listTools(function(toolsRes) {
                if (toolsRes && toolsRes.count >= 0) {
                    setStreamHint("在线 · " + toolsRes.count + " 个工具");
                }
            });
        } else {
            setStatus("离线", false);
            var err = (res && res.error) ? res.error : "无法连接";
            setStreamHint("连接失败: " + err);
        }
    });
}

function onNewSession() {
    photoState.activeRequestId += 1;
    clearPendingStreamRender();
    photoState.sessionId = generateSessionId();
    photoState.messages = [];
    photoState.msgSeq = 0;
    photoState.streamBuffer = "";
    photoState.streamChunks = [];
    photoState.streamQueuedChars = 0;
    photoState.streamDone = false;
    photoState.streamingMsgId = "";
    setSending(false);
    refreshSessionLabel();
    clearChatBubbles();
    setStreamHint("已新建会话");
    pushMessage("系统", "新会话已开始。");
}

function onClearChat() {
    photoState.activeRequestId += 1;
    clearPendingStreamRender();
    photoState.messages = [];
    photoState.msgSeq = 0;
    photoState.streamBuffer = "";
    photoState.streamChunks = [];
    photoState.streamQueuedChars = 0;
    photoState.streamDone = false;
    photoState.streamingMsgId = "";
    setSending(false);
    clearChatBubbles();
    setStreamHint("对话已清空");
}

function fillPrompt(text) {
    YUI.setText("chatInput", text);
}

function onPromptReview() {
    fillPrompt("请点评这张摄影图片，从构图、光影、色彩、主题表达给出建议。");
}

function onPromptPlan() {
    fillPrompt("请帮我策划一套适合小红书发布的摄影方案，包含主题、场景、道具和拍摄步骤。");
}

function onPromptSearch() {
    fillPrompt("帮我搜索相关摄影灵感和参考图风格建议。");
}

function onPromptXiaohongshu() {
    fillPrompt("根据当前对话内容，写一篇小红书风格的摄影笔记文案，带标题和标签。");
}

function onPickImage() {
    YUI.pickFile({
        title: "选择图片",
        filter: "*.*",
        startPath: "app"
    }, function(path) {
        if (path) {
            photoState.imagePath = path;
            var parts = path.replace(/\\/g, "/").split("/");
            var name = parts[parts.length - 1];
            YUI.setText("imagePathLabel", "📷 " + (name || path));
            YUI.update({ target: "imagePathLabel", change: { visible: 1 } });
            YUI.setText("streamHint", "已选择图片: " + name);
        }
    });
}



function setSending(busy) {
    photoState.isLoading = !!busy;
    if (busy) {
        YUI.setText("btnSend", "…");
        setStatus("思考中", photoState.online);
    } else {
        YUI.setText("btnSend", "发送");
        if (photoState.online) {
            setStatus("在线", true);
        }
    }
}

function handleStreamEvent(eventType, data) {
    if (eventType === "start") {
        setStreamHint("AI 思考中…");
        ensureAssistantBubble();
        clearPendingStreamRender();
        photoState.streamBuffer = "";
        photoState.streamChunks = [];
        photoState.streamQueuedChars = 0;
        photoState.streamDone = false;
        photoState.currentSection = "";
        return;
    }

    if (eventType === "chunk") {
        var chunk = "";
        if (data && typeof data === "object") {
            chunk = data.content || "";
        } else if (typeof data === "string") {
            chunk = data;
        }
        if (!chunk) return;

        if (photoState.currentSection === "response" || photoState.currentSection === "") {
            queueAssistantChunk(chunk);
            setStreamHint("响应中…");
        } else {
            setStreamHint("[" + photoState.currentSection + "] " + String(chunk).substring(0, 40));
        }
        return;
    }

    if (eventType === "response") {
        photoState.currentSection = "response";
        if (data && data.content) {
            queueAssistantChunk(data.content);
        } else {
            setStreamHint("生成回答中…");
        }
        return;
    }

    if (eventType === "goal" || eventType === "plan" || eventType === "task" ||
        eventType === "thought" || eventType === "action" || eventType === "observation") {
        photoState.currentSection = eventType;
        var labelMap = {
            goal: "目标",
            plan: "计划",
            task: "任务",
            thought: "思考",
            action: "工具",
            observation: "观察"
        };
        var extra = "";
        if (data) {
            if (data.tool) extra = " " + data.tool;
            else if (data.content) extra = " " + String(data.content).substring(0, 24);
        }
        setStreamHint((labelMap[eventType] || eventType) + extra);
        return;
    }

    if (eventType === "iteration") {
        if (data && data.iteration) {
            setStreamHint("迭代 " + data.iteration);
        }
        return;
    }

    if (eventType === "done") {
        photoState.streamDone = true;
        clearPendingStreamRender();
        var answer = photoState.streamBuffer;
        if (data && data.answer) {
            answer = displayText(data.answer);
            photoState.streamBuffer = answer;
            photoState.streamChunks = [];
            photoState.streamQueuedChars = 0;
            updateLastAssistant(answer);
        } else {
            flushAssistantStream();
            answer = photoState.streamBuffer;
        }
        if (answer) {
            // 已在上面刷新，保留完整答案到消息对象即可。
        } else {
            ensureAssistantBubble();
            updateLastAssistant("(空响应)");
        }
        var tokens = "";
        if (data && data.total_tokens) {
            tokens = " · " + data.total_tokens + " tokens";
        }
        setStreamHint("完成" + tokens);
        photoState.streamBuffer = "";
        photoState.streamChunks = [];
        photoState.streamQueuedChars = 0;
        photoState.currentSection = "";
        photoState.streamingMsgId = "";
        return;
    }

    if (eventType === "error") {
        var err = data;
        if (data && typeof data === "object" && data.error) err = data.error;
        setStreamHint("错误: " + err);
    }
}

function onSendMessage() {
    if (photoState.isLoading) {
        setStreamHint("请等待当前回复完成");
        return;
    }

    syncApiBaseFromInput();

    var text = YUI.getText("chatInput") || "";
    text = String(text).replace(/^\s+|\s+$/g, "");
    var files = buildFilesPayload();

    if (!text && files.length === 0) {
        setStreamHint("请输入消息，或填写图片 URL");
        return;
    }

    var displayText = text;
    if (!displayText && files.length > 0) {
        displayText = "请点评这张摄影图片";
    }
    if (files.length > 0) {
        var parts = (photoState.imagePath || "").replace(/\\/g, "/").split("/");
        var name = parts[parts.length - 1] || "";
        displayText = displayText + "\n[图片] " + name;
    }

    pushMessage("你", displayText);
    YUI.setText("chatInput", "");
    setSending(true);
    setStreamHint("请求中…");

    var requestId = photoState.activeRequestId + 1;
    photoState.activeRequestId = requestId;
    PhotoAPI.chatStream({
        message: text,
        session_id: photoState.sessionId,
        files: files,
        stream: true
    }, {
        onEvent: function(type, data) {
            if (photoState.activeRequestId !== requestId) {
                return;
            }
            handleStreamEvent(type, data);
        },
        onDone: function(data) {
            if (photoState.activeRequestId !== requestId) {
                return;
            }
            if (data && data.answer && !photoState.streamDone) {
                handleStreamEvent("done", data);
            }
            setSending(false);
            photoState.activeRequestId = 0;
            if (photoState.online) {
                setStatus("在线", true);
            }
        },
        onError: function(err) {
            if (photoState.activeRequestId !== requestId) {
                return;
            }
            pushMessage("错误", String(err));
            setStreamHint("失败: " + err);
            setSending(false);
            photoState.activeRequestId = 0;
            setStatus("离线", false);
        }
    });
}
