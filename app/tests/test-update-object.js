// 测试 YUI.update 同时支持字符串和对象参数

function testStringParam() {
    YUI.log("测试字符串参数");
    
    // 方式1：传递 JSON 字符串
    var jsonStr = '{"target":"statusLabel","change":{"text":"字符串参数更新","color":"#4caf50"}}';
    YUI.update(jsonStr);
}

function testObjectParam() {
    YUI.log("测试对象参数");
    
    // 方式2：传递 JavaScript 对象
    // 对于 mquickjs，需要手动调用 JSON.stringify
    // 对于 Mario 和 QuickJS，可以直接传对象
    var updateObj = {
        target: "statusLabel",
        change: {
            text: "对象参数更新",
            color: "#2196f3"
        }
    };
    
 
    YUI.update(updateObj);  // Mario/QuickJS 支持直接传对象
    
}

function testArrayParam() {
    YUI.log("测试数组参数（批量更新）");
    
    // 方式3：传递数组对象（批量更新）
    var updates = [
        {
            target: "statusLabel",
            change: {
                text: "批量更新 - 对象数组",
                color: "#ff9800"
            }
        },
        {
            target: "item1",
            change: {
                text: "项目1已更新",
                bgColor: "#ff9800"
            }
        }
    ];
    
 
        YUI.update(updates);  // Mario/QuickJS 支持直接传数组
    
}

function testMixedUsage() {
    YUI.log("测试混合使用");
    
    // 可以根据需要选择使用字符串或对象
    var useString = false;
    
    if (useString) {
        YUI.update('{"target":"statusLabel","change":{"text":"字符串方式"}}');
    } else {
        YUI.update({target: "statusLabel", change: {text: "对象方式"}});
    }
}

function onLoad() {
    YUI.log("测试页面加载完成");
    YUI.log("YUI.update 现在支持字符串和对象两种参数格式");
    
    // 初始化显示
    var initUpdate = {
        target: "statusLabel",
        change: {
            text: "就绪 - 点击按钮测试不同的参数格式",
            color: "#61dafb"
        }
    };
    
    
    YUI.update(initUpdate);
    
}
