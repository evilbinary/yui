// JSON 增量更新测试 - 简化版（for Mario JS）

// 测试单个属性更新
function testSingleUpdate() {
    YUI.log("测试单个属性更新");
    
    var update1 = '{"target":"statusLabel","change":{"text":"状态：单个更新","color":"#4caf50"}}';
    YUI.update(update1);
}

// 测试批量更新
function testBatchUpdate() {
    YUI.log("测试批量更新");
    
    var updates = '[{"target":"statusLabel","change":{"text":"状态：批量更新","color":"#2196f3"}},{"target":"item1","change":{"text":"项目1","bgColor":"#2196f3"}}]';
    YUI.update(updates);
}

// 测试删除操作
function testDelete() {
    YUI.log("测试删除操作");
    
    var hideBtn = '{"target":"updateBtn3","change":{"visible":null}}';
    YUI.update(hideBtn);
}

// 页面加载
function onPageLoad() {
    YUI.log("页面加载完成");
}
