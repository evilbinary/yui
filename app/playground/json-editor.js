// JSON Editor JavaScript Logic
// JSON 编辑器 JavaScript 逻辑

// 编辑器状态
var editorState = {
    jsonContent: null,
    isValid: false,
    updateMode: 'incremental',  // 默认更新模式：incremental 或 full
    messageHistory: [],
    inspectEnabled: false  // Inspect 模式状态
};

// 存储增量更新历史
var jresponse = [];

// SSE 流式状态
var streamState = {
    active: false,
    appliedCount: 0,
    statusLines: [],
    typewriterText: ""
};

// AI 生成 loading 状态
var generatingState = {
    active: false
};

function showAiLoading() {
    generatingState.active = true;
    YUI.update({
        target: "aiLoadingMask",
        change: { visible: true }
    });
    YUI.update({
        target: "previewLabel",
        change: { visible: false }
    });
    YUI.update({
        target: "sendBtn",
        change: { text: "生成中..." }
    });
}

function hideAiLoading() {
    generatingState.active = false;
    YUI.update({
        target: "aiLoadingMask",
        change: { visible: false }
    });
    YUI.update({
        target: "previewLabel",
        change: { visible: true }
    });
    YUI.update({
        target: "sendBtn",
        change: { text: "生成" }
    });
}

function revealPreviewDuringStream() {
    if (!generatingState.active) {
        return;
    }
    YUI.update({
        target: "aiLoadingMask",
        change: { visible: false }
    });
    YUI.update({
        target: "previewLabel",
        change: { visible: true }
    });
}

// 判断是否为可渲染的 UI 组件 JSON（而非增量更新指令）
function isUiComponentJson(json) {
    return json && typeof json === 'object' && json.type;
}

// 开始 SSE 流：清空预览，流式结束后再一次性写入编辑器
function beginIncrementalStream(messageText) {
    streamState.active = true;
    streamState.appliedCount = 0;
    streamState.statusLines = [];
    streamState.typewriterText = "AI 生成中: " + messageText + "\n";

    jresponse = [];
    resetPreviewForIncremental();

    YUI.setText("jsonEditor", "[\n]");
    editorState.jsonContent = [];
    editorState.isValid = true;

    YUI.setText("inputLabel", streamState.typewriterText);
}

// 流式状态栏用简短摘要，避免把整段 change JSON 写入 inputLabel
function summarizeUpdateChange(change) {
    if (!change || typeof change !== "object") {
        return "";
    }
    if (Array.isArray(change.children)) {
        return "children×" + change.children.length;
    }
    var keys = Object.keys(change);
    if (keys.length === 0) {
        return "";
    }
    if (keys.length <= 3) {
        return keys.join(", ");
    }
    return keys.slice(0, 3).join(", ") + "…";
}

// 显示本条动态更新状态
function showStreamUpdateLine(update, meta) {
    streamState.appliedCount++;
    var prefix = "#" + streamState.appliedCount;
    if (meta && meta.index && meta.total) {
        prefix = "[" + meta.index + "/" + meta.total + "]";
    }
    var summary = summarizeUpdateChange(update.change);
    var line = prefix + " " + update.target + (summary ? " → " + summary : "");
    streamState.statusLines.push(line);

    var status = streamState.typewriterText + "\n── 动态更新 ──\n" + streamState.statusLines.join("\n");
    YUI.setText("inputLabel", status);
}

// 剥离 SSE 附带的序号字段，得到纯增量指令
function normalizeStreamUpdate(raw) {
    return {
        update: {
            target: raw.target,
            change: raw.change
        },
        meta: {
            index: raw.index,
            total: raw.total
        }
    };
}

function appendStreamToken(delta) {
    streamState.typewriterText += delta;
    if (streamState.appliedCount === 0) {
        revealPreviewDuringStream();
        YUI.setText("inputLabel", streamState.typewriterText);
    }
}

function finishIncrementalStream(info, messageText) {
    streamState.active = false;
    hideAiLoading();
    syncEditorWithJresponse();
    showPreviewRootIfPresent();
    var count = info && info.count ? info.count : streamState.appliedCount;
    var lines = streamState.statusLines.join("\n");
    var tail = "\n── 完成 ──\n共 " + count + " 条更新: " + messageText;
    YUI.setText("inputLabel", streamState.typewriterText + "\n── 动态更新 ──\n" + lines + tail);
    YUI.log("finishIncrementalStream: " + count + " updates");
}

// 将 jresponse 同步回编辑器（非流式批量时使用）
function syncEditorWithJresponse() {
    var text = JSON.stringify(jresponse, null, 4);
    YUI.setText("jsonEditor", text);
    editorState.jsonContent = jresponse.slice();
    editorState.isValid = true;
}

// 应用单条增量更新（流式时只 YUI.update，结束后再写编辑器）
function applySingleIncrementalUpdate(rawUpdate) {
    if (!rawUpdate || !rawUpdate.target) {
        return;
    }

    var normalized = normalizeStreamUpdate(rawUpdate);
    var update = normalized.update;
    var meta = normalized.meta;

    jresponse.push(update);
    YUI.update(JSON.stringify(update));

    if (streamState.active) {
        if (streamState.appliedCount === 0) {
            revealPreviewDuringStream();
        }
        showStreamUpdateLine(update, meta);
    } else {
        showLayersFromUpdate(update);
        syncEditorWithJresponse();
    }

    YUI.log("applySingleIncrementalUpdate: " + update.target + " -> " + summarizeUpdateChange(update.change));
}

// 批量应用增量更新（单次批量 YUI.update，避免逐条 show 触发布局）
function applyIncrementalUpdates(updates) {
    if (!Array.isArray(updates) || updates.length === 0) {
        return;
    }
    resetPreviewForIncremental();
    jresponse = updates.slice();
    YUI.update(JSON.stringify(updates));
    syncEditorWithJresponse();
    showPreviewRootIfPresent();
}

// 全量模式默认：单个 UI 组件树
function getDefaultFullJson() {
    return {
        "id": "root",
        "type": "View",
        "size": [400, 500],
        "layout": {"type": "vertical", "spacing": 8},
        "children": [
            {"id": "title", "type": "Label", "text": "标题", "size": [0, 32]},
            {"id": "ok", "type": "Button", "text": "确定", "size": [120, 40]}
        ]
    };
}

// 增量模式默认：空数组，SSE 逐条追加 {target, change}
function getDefaultIncrementalJson() {
    return [];
}

// 按更新模式写入编辑器模板并刷新预览
function applyEditorTemplate(mode) {
    jresponse = [];
    var text;
    var json;

    if (mode === 'incremental') {
        json = getDefaultIncrementalJson();
        text = "[\n]";
        YUI.setText("inputLabel", "增量模式：[] 起始，SSE 每条 {target, change} 逐条追加");
    } else {
        json = getDefaultFullJson();
        text = JSON.stringify(json, null, 4);
        YUI.setText("inputLabel", "全量模式：编辑完整 UI JSON");
    }

    YUI.setText("jsonEditor", text);
    editorState.jsonContent = json;
    editorState.isValid = true;
    refreshPreviewInternal(json);

    YUI.log("applyEditorTemplate: mode=" + mode);
}

// 初始化JSON编辑器 - onLoad 事件触发
function initJsonEditor() {
    YUI.log("initJsonEditor: Initializing JSON editor...");

    var mode = editorState.updateMode || 'incremental';
    applyEditorTemplate(mode);

    YUI.log("initJsonEditor: JSON editor initialized!");
}

// JSON文本变更时触发 - onChange 事件
function onJsonChange() {
    YUI.log("onJsonChange: JSON text changed");
    
    var jsonText = YUI.getText("jsonEditor");
    
    // 尝试解析并验证
    validateJsonInternal(jsonText);
    
    // 如果有效，更新预览
    if (editorState.isValid && editorState.jsonContent) {
        refreshPreviewInternal(editorState.jsonContent);
    }
}

// 更新模式变更时触发 - onChange 事件
function onUpdateModeChange() {
    var selectedValue = YUI.getProperty("updateModeSelect", "value");
    var prevMode = editorState.updateMode;

    editorState.updateMode = selectedValue;
    YUI.log("onUpdateModeChange: Update mode changed to " + selectedValue);

    if (prevMode !== selectedValue) {
        applyEditorTemplate(selectedValue);
    }
}

// 验证JSON - 验证按钮 onClick 事件
function validateJson() {
    
    var jsonText = YUI.getText("jsonEditor");
    var result = validateJsonInternal(jsonText);
    YUI.log("validateJson: Validating JSON..."+jsonText);

    if (result) {
        YUI.log("validateJson: JSON is valid! jsonText: "+jsonText);
        // 可以显示成功提示
    } else {
        YUI.log("validateJson: JSON is invalid!");
        // 可以显示错误提示
    }
}

// 格式化JSON - 格式化按钮 onClick 事件
function formatJson() {
    YUI.log("formatJson: Formatting JSON...");
    
    var jsonText = YUI.getText("jsonEditor");
    
    // 尝试解析
    var json = null;
    try {
        json = JSON.parse(jsonText);
    } catch (e) {
        YUI.log("formatJson: Cannot format invalid JSON - " + e.message);
        return;
    }
    
    // 格式化为4空格缩进的JSON字符串
    // mquickjs 的 JSON.stringify 不支持缩进参数
    var formattedJson = JSON.stringify(json, null, 4);
    
    // 更新编辑器文本
    YUI.setText("jsonEditor", formattedJson);
    
    // 更新状态
    editorState.jsonContent = json;
    editorState.isValid = true;
    
    // 更新预览
    refreshPreviewInternal(json);
    
    YUI.log("formatJson: JSON formatted successfully!");
}

// 刷新预览 - 刷新按钮 onClick 事件
function refreshPreview() {
    YUI.log("refreshPreview: Refreshing preview...");
    
    var jsonText = YUI.getText("jsonEditor");
    
    YUI.log("refreshPreview: JSON Text: " + jsonText);
    
    // 尝试解析
    var json = null;
    try {
        json = JSON.parse(jsonText);
    } catch (e) {
        YUI.log("refreshPreview: Cannot preview invalid JSON - " + e.message);
        return;
    }
    
    // 更新预览
    refreshPreviewInternal(json);
    
    YUI.log("refreshPreview: Preview refreshed!");
}

// 内部验证函数
function validateJsonInternal(jsonText) {
    try {
        // 尝试解析JSON
        var json = JSON.parse(jsonText);
        editorState.jsonContent = json;
        editorState.isValid = true;
        YUI.log("validateJsonInternal: JSON is valid");
        return true;
    } catch (e) {
        editorState.jsonContent = null;
        editorState.isValid = false;
        YUI.log("validateJsonInternal: JSON is invalid - " + e.message);
        return false;
    }
}

// renderFromJson 创建的新图层默认隐藏，需显式 show（与 Router 行为一致）
function showRenderedLayer(json) {
    if (json && json.id) {
        YUI.log("showRenderedLayer: Showing rendered layer: " + json.id);
        YUI.show(json.id);
    }
}

// 将 UI JSON 渲染到预览区
function renderPreviewUi(uiJson) {
    var jsonString = JSON.stringify(uiJson, null, 4);
    var result = YUI.renderFromJson("previewLabel", jsonString);
    if (result === 0) {
        showRenderedLayer(uiJson);
        return true;
    }
    var previewText = jsonToPreviewText(uiJson);
    YUI.setText("previewLabel", previewText);
    return false;
}

// 增量预览：先重置 outlet，再按顺序重放所有 update
function resetPreviewForIncremental() {
    var emptyRoot = {
        "id": "root",
        "type": "View",
        "size": [400, 500],
        "layout": {"type": "vertical", "spacing": 8},
        "children": []
    };
    renderPreviewUi(emptyRoot);
}

function isIncrementalUpdateItem(item) {
    return item && item.target && item.change;
}

// 增量更新后确保预览根节点可见（仅一次，避免逐条 show 触发布局）
function showPreviewRootIfPresent() {
    YUI.show("root");
}

// 非流式单条更新时仍可显式 show 目标节点
function showLayersFromUpdate(update) {
    if (!update) {
        return;
    }
    if (update.target) {
        YUI.show(update.target);
    }
    if (update.change && update.change.children && Array.isArray(update.change.children)) {
        for (var i = 0; i < update.change.children.length; i++) {
            var child = update.change.children[i];
            if (child && child.id) {
                YUI.show(child.id);
            }
        }
    }
}

function replayIncrementalUpdates(updates) {
    resetPreviewForIncremental();
    var batch = [];
    for (var i = 0; i < updates.length; i++) {
        if (!isIncrementalUpdateItem(updates[i])) {
            continue;
        }
        batch.push(normalizeStreamUpdate(updates[i]).update);
    }
    if (batch.length === 1) {
        YUI.update(JSON.stringify(batch[0]));
    } else if (batch.length > 1) {
        YUI.update(JSON.stringify(batch));
    }
    showPreviewRootIfPresent();
}

// 内部刷新预览函数
function refreshPreviewInternal(json) {
    if (!json) {
        return;
    }

    // 增量模式：纯 update 数组
    if (Array.isArray(json)) {
        if (json.length === 0) {
            resetPreviewForIncremental();
            return;
        }
        if (isIncrementalUpdateItem(json[0])) {
            YUI.log("refreshPreviewInternal: Replaying " + json.length + " incremental updates");
            replayIncrementalUpdates(json);
            return;
        }
    }

    // 全量模式：单个 UI 组件树
    var uiJson = isUiComponentJson(json) ? json : null;
    if (uiJson) {
        YUI.log("refreshPreviewInternal: Rendering UI preview for id=" + uiJson.id);
        renderPreviewUi(uiJson);
    }
}

// 将JSON对象转换为预览文本
function jsonToPreviewText(json, indent) {
    if (typeof indent === 'undefined') {
        indent = 0;
    }
    
    var indentStr = "";
    for (var i = 0; i < indent; i++) {
        indentStr += "  ";
    }
    
    if (json === null) {
        return "null";
    } else if (typeof json === 'string') {
        return '"' + json + '"';
    } else if (typeof json === 'number') {
        return String(json);
    } else if (typeof json === 'boolean') {
        return json ? "true" : "false";
    } else if (Array.isArray(json)) {
        var result = "[\n";
        for (var i = 0; i < json.length; i++) {
            result += indentStr + "  " + jsonToPreviewText(json[i], indent + 1);
            if (i < json.length - 1) {
                result += ",";
            }
            result += "\n";
        }
        result += indentStr + "]";
        return result;
    } else if (typeof json === 'object') {
        var keys = Object.keys(json);
        var result = "{\n";
        for (var i = 0; i < keys.length; i++) {
            var key = keys[i];
            result += indentStr + "  " + key + ": " + jsonToPreviewText(json[key], indent + 1);
            if (i < keys.length - 1) {
                result += ",";
            }
            result += "\n";
        }
        result += indentStr + "}";
        return result;
    }
    
    return String(json);
}

// 发送消息函数 - 发送按钮 onClick 事件
function sendMessage() {
    YUI.log("sendMessage: Sending message...");

    if (generatingState.active) {
        YUI.log("sendMessage: Already generating, ignored");
        return;
    }
    
    var messageText = YUI.getText("messageInput");
    
    if (!messageText || messageText.trim() === "") {
        YUI.log("sendMessage: Empty message, not sending");
        return;
    }
    
    YUI.log("sendMessage: Message content: " + messageText);
    
    // 检查编辑器中的JSON是否有效
    var jsonText = YUI.getText("jsonEditor");
    var json = jsonText;
    // 获取更新模式（可以从配置或用户选择中获取）
    var updateMode = editorState.updateMode || 'incremental'; // 默认为增量更新
    
    // 显示发送中状态
    YUI.setText("inputLabel", "生成中: " + messageText + " | 模式: " + updateMode);
    showAiLoading();
    
    // 根据更新模式调用相应的API
    if (updateMode === 'incremental') {
        beginIncrementalStream(messageText);

        APIClient.sendMessageIncrementalStream(messageText, json, {
            onToken: function(delta) {
                appendStreamToken(delta);
            },
            onUpdate: function(update) {
                applySingleIncrementalUpdate(update);
            },
            onDone: function(info) {
                finishIncrementalStream(info, messageText);
            },
            onError: function(err) {
                streamState.active = false;
                hideAiLoading();
                if (jresponse.length > 0) {
                    syncEditorWithJresponse();
                }
                YUI.setText("inputLabel", "生成失败: " + err);
                YUI.setText("previewLabel", "❌ 增量流式更新失败\n\n" + err);
            }
        });
    } else if (updateMode === 'full') {
        // 使用全量更新模式
        APIClient.sendMessageFull(messageText, json, function(response) {
            handleApiResponse(response, messageText, updateMode);
        });
    }else{
        YUI.log("sendMessage: Unknown update mode: " + updateMode);
        hideAiLoading();
    }
    
    // 清空输入框
    YUI.setText("messageInput", "");
    
    YUI.log("sendMessage: Message sent to API!");
}

// 处理API响应
function handleApiResponse(response, messageText, updateMode) {
    YUI.log("handleApiResponse: Processing API response");
    
    if (response && typeof response === 'object' && !response.error) {
        // 处理成功的响应
        if (updateMode === 'incremental') {
            // 处理增量更新响应（直接是 updates 数组）
            if (Array.isArray(response) && response.length > 0) {
                applyIncrementalUpdates(response);

                var details = "✅ 增量更新成功！\n\n";
                details += "更新详情:\n";
                jresponse.forEach(function(update, index) {
                    details += (index + 1) + ". " + update.target + ": " + JSON.stringify(update.change) + "\n";
                });
                
                YUI.log("handleApiResponse: Incremental updates applied");
                YUI.log("handleApiResponse: Total jresponse array length: " + jresponse.length);
            } else if (Array.isArray(response) && response.length === 0) {
                YUI.setText("previewLabel", "✅ 消息已发送，但没有收到UI更新\n\n消息: " + messageText);
            } else {
                YUI.setText("previewLabel", "❌ 增量更新失败 - 无效的响应格式\n\n响应: " + JSON.stringify(response));
            }
        } else if (updateMode === 'full') {
            // 处理全量更新响应（直接是 UI JSON 对象）
            if (response && typeof response === 'object' && response.type) {
                // 清空增量更新数组，因为这是全量更新
                jresponse = [];
                
                // 直接渲染完整的 UI JSON（符合 json-format-spec.md 规范）
                YUI.log("handleApiResponse: Full UI JSON received");
                YUI.log("handleApiResponse: UI Type: " + response.type);
                

                // 显示详细信息
                var details = "✅ 全量更新成功！\n\n";
                details += "UI 类型: " + response.type + "\n";
                details += "子组件数量: " + (response.children ? response.children.length : 0) + "\n\n";
                details += "完整 UI JSON:\n" + JSON.stringify(response, null, 2);
                
                // /YUI.setText("previewResult", details);
                YUI.log("handleApiResponse: Full UI update applied", details);

                YUI.setText("jsonEditor", JSON.stringify(response, null, 2));
                editorState.jsonContent = response;
                editorState.isValid = true;
                refreshPreviewInternal(response);
            } else {
                //YUI.setText("previewResult", "❌ 全量更新失败 - 无效的响应格式\n\n响应: " + JSON.stringify(response));
            }
        }
        
        // 记录到历史（可选）
        var historyEntry = {
            message: messageText,
            mode: updateMode,
            timestamp: new Date().toLocaleString(),
            response: response
        };
        
        if (!editorState.messageHistory) {
            editorState.messageHistory = [];
        }
        editorState.messageHistory.push(historyEntry);
    } else {
        // 处理错误
        var errorMsg = "❌ API 请求失败\n\n";
        errorMsg += "错误信息: " + (response.error || 'Unknown error') + "\n";
        errorMsg += "消息内容: " + messageText;
        
        YUI.setText("previewLabel", errorMsg);
        YUI.log("handleApiResponse: API request failed - " + (response.error || 'Unknown error'));
    }

    hideAiLoading();
}

// Toggle Inspect 模式 - inspectBtn.onClick 事件
function toggleInspect() {
    YUI.log("toggleInspect: Toggling inspect mode...");
    
    // 切换状态
    if (editorState.inspectEnabled) {
        YUI.inspect.disable();
        editorState.inspectEnabled = false;
        YUI.setText("inspectBtn", "Inspect");
        YUI.log("toggleInspect: Inspect mode disabled");
    } else {
        YUI.inspect.enable();
        editorState.inspectEnabled = true;
        YUI.setText("inspectBtn", "Inspect ✓");
        YUI.log("toggleInspect: Inspect mode enabled");
    }
}
