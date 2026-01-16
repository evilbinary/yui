// 测试 Mario 对象转字符串

function onLoad() {
    YUI.log("测试 Mario 对象序列化");
    
    // 测试简单对象
    var obj = {target: "statusLabel", change: {text: "测试"}};
    YUI.log("对象创建完成");
    
    // 尝试调用 update
    try {
        YUI.update(obj);
        YUI.log("update 调用成功");
    } catch (e) {
        YUI.log("update 调用失败");
    }
}
