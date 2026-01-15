// JSON 增量更新测试

// 测试单个属性更新
function testSingleUpdate() {
    YUI.log("测试单个属性更新");
    
    // 方式1：更新文本
    var update1 = '{"target":"statusLabel","change":{"text":"状态：单个属性已更新","color":"#4caf50"}}';
    YUI.update(update1);
    
    // 方式2：更新背景色
    var update2 = '{"target":"item1","change":{"text":"项目 1 - 已更新！","bgColor":"#4caf50"}}';
    YUI.update(update2);
}

// 测试批量更新
function testBatchUpdate() {
    YUI.log("测试批量更新");
    
    var batchUpdate = '[' +
        '{"target":"statusLabel","change":{"text":"状态：批量更新完成","color":"#2196f3"}},' +
        '{"target":"item1","change":{"text":"批量更新 - 项目 1","bgColor":"#2196f3"}},' +
        '{"target":"item2","change":{"text":"批量更新 - 项目 2","bgColor":"#ff9800"}},' +
        '{"target":"item3","change":{"text":"批量更新 - 项目 3","bgColor":"#9c27b0"}}' +
        ']';
    
    YUI.update(batchUpdate);
}

// 测试删除操作
function testDelete() {
    YUI.log("测试删除操作（隐藏元素）");
    
    // 使用 null 删除/隐藏元素
    var deleteUpdate = '{"target":"updateBtn3","change":{"visible":null}}';
    YUI.update(deleteUpdate);
    
    // 更新状态标签
    var statusUpdate = '{"target":"statusLabel","change":{"text":"状态：删除按钮已隐藏","color":"#ff5722"}}';
    YUI.update(statusUpdate);
}

// 页面加载时的初始化
function onPageLoad() {
    YUI.log("JSON 增量更新测试页面加载完成");
    YUI.log("支持的功能：");
    YUI.log("1. 单个属性更新");
    YUI.log("2. 批量更新");
    YUI.log("3. 删除/隐藏元素");
    
    // 列出 YUI 的所有属性
    YUI.log("YUI 对象属性:");
    YUI.log("- YUI.log: " + typeof YUI.log);
    YUI.log("- YUI.setText: " + typeof YUI.setText);
    YUI.log("- YUI.getText: " + typeof YUI.getText);
    YUI.log("- YUI.setBgColor: " + typeof YUI.setBgColor);
    YUI.log("- YUI.hide: " + typeof YUI.hide);
    YUI.log("- YUI.show: " + typeof YUI.show);
    YUI.log("- YUI.renderFromJson: " + typeof YUI.renderFromJson);
    YUI.log("- YUI.call: " + typeof YUI.call);
    YUI.log("- YUI.update: " + typeof YUI.update);
    
    if (typeof YUI.update !== 'undefined') {
        // 初始化状态（直接使用 JSON 字符串）
        var initUpdate = '{"target":"statusLabel","change":{"text":"状态：就绪 - 点击按钮测试功能","color":"#61dafb"}}';
        YUI.update(initUpdate);
    } else {
        YUI.log("错误: YUI.update 不存在！");
        YUI.log("尝试使用 YUI.setText 替代...");
        YUI.setText("statusLabel", "状态：就绪（update API 不可用）");
    }
}
