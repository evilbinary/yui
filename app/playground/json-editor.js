// JSON Editor JavaScript Logic
// JSON 编辑器 JavaScript 逻辑

// 编辑器状态
var editorState = {
    jsonContent: null,
    isValid: false
};

// 初始化JSON编辑器 - onLoad 事件触发
function initJsonEditor() {
    YUI.log("initJsonEditor: Initializing JSON editor...");

    // 设置默认的示例JSON
    var defaultJson = {
        "id": "test",
        "type": "Button",
        "text": "Hello World",
        "size": [200, 50],
        "style": {
            "bgColor": "#4CAF50",
            "color": "#ffffff",
            "fontSize": 16,
            "borderRadius": 8
        }
    };

    

    // 格式化并设置到编辑器
    var formattedJson = JSON.stringify(defaultJson, null, 4);
    YUI.setText("jsonEditor", formattedJson);

    // 初始验证
    validateJsonInternal(formattedJson);

    // 初始预览
    refreshPreviewInternal(defaultJson);

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

// 验证JSON - 验证按钮 onClick 事件
function validateJson() {
    YUI.log("validateJson: Validating JSON...");
    
    var jsonText = YUI.getText("jsonEditor");
    var result = validateJsonInternal(jsonText);
    
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

    YUI.log("refreshPreviewInternal: JSON = " + JSON.stringify(json));

    // 将 JSON 转换为字符串
    var jsonString = JSON.stringify(json, null, 4);

    YUI.log("refreshPreviewInternal: JSON Text: " + jsonString);

    // 使用新的 renderFromJson 函数动态渲染 UI
    var result = YUI.renderFromJson("previewContainer", jsonString);

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
