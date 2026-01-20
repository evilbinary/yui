// JSON Editor JavaScript Logic
// JSON 编辑器 JavaScript 逻辑

// 编辑器状态
var editorState = {
    jsonContent: null,
    isValid: false,
    updateMode: 'full',  // 默认更新模式：incremental 或 full
    messageHistory: []
};

// 初始化JSON编辑器 - onLoad 事件触发
function initJsonEditor() {
    YUI.log("initJsonEditor: Initializing JSON editor...");

    // 设置默认的示例JSON
    var defaultJson = {
        "type": "Label",
        "text": "Hello World"
    };
    // 格式化并设置到编辑器
    // 注意: mquickjs 的 JSON.stringify 不支持第三个参数（缩进）
    var formattedJson = JSON.stringify(defaultJson, null, 4);
    YUI.setText("jsonEditor", formattedJson);

    // 初始验证
    validateJsonInternal(formattedJson);

    // 初始预览
    refreshPreviewInternal(defaultJson);
    YUI.setText('jsonEditor', formattedJson);

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
    var selectedValue = YUI.getText("updateModeSelect");
    editorState.updateMode = selectedValue;
    YUI.log("onUpdateModeChange: Update mode changed to " + selectedValue);
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

// 内部刷新预览函数
function refreshPreviewInternal(json) {
    if (!json) {
        return;
    }

    // 将 JSON 转换为字符串
    var jsonString = JSON.stringify(json, null, 4);

    YUI.log("refreshPreviewInternal: JSON Text: " + jsonString);

    // 使用新的 renderFromJson 函数动态渲染 UI
    var result = YUI.renderFromJson("previewLabel", jsonString);

    if (result === 0) {
        YUI.log("refreshPreviewInternal: Successfully rendered JSON to UI");
    } else {
        YUI.log("refreshPreviewInternal: Failed to render JSON, result = " + result);
        // 如果渲染失败，回退到文本显示
        var previewText = jsonToPreviewText(json);
        YUI.setText("previewLabel", previewText);
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
    YUI.setText("inputLabel", "生成文本: " + messageText + " 更新模式: " + updateMode);
    
    // 根据更新模式调用相应的API
    if (updateMode === 'incremental') {
        // 使用增量更新模式
        APIClient.sendMessageIncremental(messageText, json, function(response) {
            handleApiResponse(response, messageText, updateMode);
        });
    } else if (updateMode === 'full') {
        // 使用全量更新模式
        APIClient.sendMessageFull(messageText, json, function(response) {
            handleApiResponse(response, messageText, updateMode);
        });
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
                // 将更新转换为 YUI.update() 格式
                var updateString = JSON.stringify(response);
                YUI.update(updateString);
                
                // 显示详细信息
                var details = "✅ 增量更新成功！\n\n";
                details += "更新详情:\n";
                response.forEach(function(update, index) {
                    details += (index + 1) + ". " + update.target + ": " + JSON.stringify(update.change) + "\n";
                });
                
                YUI.setText("previewLabel", details);
                YUI.log("handleApiResponse: Incremental updates applied: " + updateString);
            } else if (Array.isArray(response) && response.length === 0) {
                YUI.setText("previewLabel", "✅ 消息已发送，但没有收到UI更新\n\n消息: " + messageText);
            } else {
                YUI.setText("previewLabel", "❌ 增量更新失败 - 无效的响应格式\n\n响应: " + JSON.stringify(response));
            }
        } else if (updateMode === 'full') {
            // 处理全量更新响应（直接是 UI JSON 对象）
            if (response && typeof response === 'object' && response.type) {
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
                refreshPreviewInternal(response);
                // 这里应该调用 YUI.render 或其他函数来渲染完整的 UI
                // 例如：YUI.renderFromJson(response);
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
}
