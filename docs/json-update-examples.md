# JSON 增量更新 - 实战示例

## 快速开始

### 1. 基本用法

```javascript
//  单个更新
var update = JSON.stringify({
    target: "myButton",
    change: {
        text: "已点击",
        bgColor: "#ff0000"
    }
});
YUI.update(update);
```

### 2. 批量更新

```javascript
var updates = JSON.stringify([
    { target: "label1", change: { text: "更新1", color: "#00ff00" } },
    { target: "label2", change: { text: "更新2", color: "#ff00ff" } },
    { target: "btn1", change: { bgColor: "#4caf50", enabled: true } }
]);
YUI.update(updates);
```

### 3. 路径访问

```javascript
// 通过索引访问子元素
var update = JSON.stringify({
    target: "container",
    change: {
        "children.0": { text: "更新第一个子元素" }
    }
});

// 通过 ID 访问子元素
var update2 = JSON.stringify({
    target: "container",
    change: {
        "children.myChild": { bgColor: "#ff0000" }
    }
});
```

### 4. 删除操作

```javascript
// 隐藏元素
var hideUpdate = JSON.stringify({
    target: "oldButton",
    change: {
        visible: null
    }
});
YUI.update(hideUpdate);

// 删除子元素
var removeChild = JSON.stringify({
    target: "listContainer",
    change: {
        "children.0": null  // 删除第一个子元素
    }
});
YUI.update(removeChild);

// 清空所有子元素
var clearAll = JSON.stringify({
    target: "listContainer",
    change: {
        children: null
    }
});
YUI.update(clearAll);
```

## 完整应用示例

### 表单验证

```javascript
function validateForm() {
    var email = YUI.getText("emailInput");
    var isValid = email.indexOf("@") > 0;
    
    if (isValid) {
        // 验证通过 - 批量更新 UI
        var successUpdate = JSON.stringify([
            {
                target: "emailInput",
                change: {
                    borderColor: "#4caf50",
                    borderWidth: 2
                }
            },
            {
                target: "validIcon",
                change: {
                    visible: true,
                    text: "✓",
                    color: "#4caf50"
                }
            },
            {
                target: "submitBtn",
                change: {
                    enabled: true,
                    bgColor: "#4caf50",
                    text: "提交表单"
                }
            },
            {
                target: "errorMsg",
                change: {
                    visible: null
                }
            }
        ]);
        YUI.update(successUpdate);
    } else {
        // 验证失败
        var errorUpdate = JSON.stringify([
            {
                target: "emailInput",
                change: {
                    borderColor: "#ff0000",
                    borderWidth: 2
                }
            },
            {
                target: "errorMsg",
                change: {
                    visible: true,
                    text: "请输入有效的邮箱地址",
                    color: "#ff0000"
                }
            }
        ]);
        YUI.update(errorUpdate);
    }
}
```

### 动态列表更新

```javascript
function updateList(newItems) {
    // 清空旧列表
    var clearUpdate = JSON.stringify({
        target: "listContainer",
        change: {
            children: null
        }
    });
    YUI.update(clearUpdate);
    
    // 逐个添加新项目（注意：这需要扩展 appendChild 功能）
    // 目前可以通过 renderFromJson 实现
    for (var i = 0; i < newItems.length; i++) {
        var itemJson = JSON.stringify({
            id: "item" + i,
            type: "Label",
            text: newItems[i],
            size: [300, 40],
            style: {
                bgColor: "#333333",
                color: "#ffffff"
            }
        });
        YUI.renderFromJson("listContainer", itemJson);
    }
}
```

### 状态机 UI

```javascript
var currentState = "idle";

function setState(newState) {
    currentState = newState;
    updateUIByState();
}

function updateUIByState() {
    var stateConfigs = {
        idle: [
            { target: "status", change: { text: "就绪", color: "#888888" } },
            { target: "startBtn", change: { enabled: true, text: "开始" } },
            { target: "stopBtn", change: { enabled: false } },
            { target: "progress", change: { visible: null } }
        ],
        running: [
            { target: "status", change: { text: "运行中...", color: "#4caf50" } },
            { target: "startBtn", change: { enabled: false } },
            { target: "stopBtn", change: { enabled: true, text: "停止" } },
            { target: "progress", change: { visible: true } }
        ],
        error: [
            { target: "status", change: { text: "错误", color: "#ff0000" } },
            { target: "startBtn", change: { enabled: true, text: "重试" } },
            { target: "stopBtn", change: { enabled: false } },
            { target: "errorMsg", change: { visible: true } }
        ]
    };
    
    var config = stateConfigs[currentState];
    if (config) {
        YUI.update(JSON.stringify(config));
    }
}
```

### 主题切换

```javascript
var isDarkMode = false;

function toggleTheme() {
    isDarkMode = !isDarkMode;
    
    var theme = isDarkMode ? {
        bg: "#1e1e1e",
        text: "#ffffff",
        cardBg: "#2d2d2d",
        buttonBg: "#3a3a3a"
    } : {
        bg: "#ffffff",
        text: "#000000",
        cardBg: "#f5f5f5",
        buttonBg: "#e0e0e0"
    };
    
    var themeUpdate = JSON.stringify([
        { target: "root", change: { bgColor: theme.bg } },
        { target: "header", change: { color: theme.text } },
        { target: "card1", change: { bgColor: theme.cardBg, color: theme.text } },
        { target: "card2", change: { bgColor: theme.cardBg, color: theme.text } },
        { target: "actionBtn", change: { bgColor: theme.buttonBg, color: theme.text } }
    ]);
    
    YUI.update(themeUpdate);
}
```

## 性能优化技巧

### 1. 批量更新而非多次单个更新

❌ 不好：
```javascript
YUI.update(JSON.stringify({ target: "btn1", change: { text: "更新1" } }));
YUI.update(JSON.stringify({ target: "btn2", change: { text: "更新2" } }));
YUI.update(JSON.stringify({ target: "btn3", change: { text: "更新3" } }));
```

✅ 好：
```javascript
YUI.update(JSON.stringify([
    { target: "btn1", change: { text: "更新1" } },
    { target: "btn2", change: { text: "更新2" } },
    { target: "btn3", change: { text: "更新3" } }
]));
```

### 2. 只更新变化的属性

❌ 不好：
```javascript
YUI.update(JSON.stringify({
    target: "btn",
    change: {
        text: "点击",
        bgColor: "#ff0000",  // 即使没变也设置
        size: [100, 40],     // 即使没变也设置
        visible: true        // 即使没变也设置
    }
}));
```

✅ 好：
```javascript
YUI.update(JSON.stringify({
    target: "btn",
    change: {
        text: "点击"  // 只更新改变的属性
    }
}));
```

### 3. 使用 null 删除而非频繁 renderFromJson

❌ 不好（性能差）：
```javascript
// 清空后重新渲染所有子元素
YUI.renderFromJson("container", "...");
```

✅ 好：
```javascript
// 只更新必要的部分
YUI.update(JSON.stringify({
    target: "container",
    change: {
        "children.0": null  // 只删除特定元素
    }
}));
```

## 错误处理

```javascript
function safeUpdate(updateData) {
    try {
        var json = JSON.stringify(updateData);
        var result = YUI.update(json);
        
        if (result != 0) {
            YUI.log("更新失败: " + result);
            // 回退到默认状态
            fallbackUpdate();
        }
    } catch (e) {
        YUI.log("JSON 序列化失败");
    }
}

function fallbackUpdate() {
    YUI.update(JSON.stringify({
        target: "errorLabel",
        change: {
            visible: true,
            text: "操作失败，请重试",
            color: "#ff0000"
        }
    }));
}
```

## 调试技巧

```javascript
// 开启详细日志
function debugUpdate(updateData) {
    YUI.log("=== 开始更新 ===");
    YUI.log(JSON.stringify(updateData));
    
    var result = YUI.update(JSON.stringify(updateData));
    
    YUI.log("更新结果: " + result);
    YUI.log("=== 更新结束 ===");
    
    return result;
}

// 使用
debugUpdate({
    target: "myButton",
    change: { text: "测试" }
});
```

## 限制和注意事项

1. **必须使用 JSON.stringify**：mquickjs 要求传入 JSON 字符串
2. **路径语法有限**：目前支持 `children.0` 和 `children.id`，不支持更复杂的嵌套路径
3. **null 的语义**：null 表示删除/重置，而非设置为 null 值
4. **异步问题**：更新是同步的，不支持动画过渡
5. **字体大小更新**：需要重新加载字体，可能有性能影响

## 未来扩展

计划支持的功能：
- [ ] 动画过渡（`transition` 字段）
- [ ] 条件更新（`if` 字段）
- [ ] 计算属性（表达式支持）
- [ ] 更复杂的路径语法
- [ ] appendChild/removeChild 直接支持
- [ ] 更新队列和批处理优化
