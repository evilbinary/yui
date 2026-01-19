/**
 * 第一个脚本文件 - 定义全局变量
 */

// 定义一个测试变量
var globalTestVar = "来自第一个文件的变量";
var testObject = {
    message: "这是第一个文件中的对象",
    value: 123
};

// 尝试多种方式定义全局变量
if (typeof globalThis !== 'undefined') {
    globalThis.fromFirstFile = "通过globalThis设置";
}

if (typeof global !== 'undefined') {
    global.fromFirstFile = "通过global设置";
}

if (typeof window !== 'undefined') {
    window.fromFirstFile = "通过window设置";
}

// 在QuickJS环境中尝试特殊方式
if (typeof std !== 'undefined' && typeof os !== 'undefined') {
    try {
        // 尝试通过this设置
        this.fromFirstFileThis = "通过this设置";
        
        // 尝试使用eval
        // eval('var fromFirstFileEval = "通过eval设置"');
        
        // 尝试使用Function构造函数
        new Function('var fromFirstFileFunc = "通过Function设置"')();
    } catch (e) {
        console.log("设置全局变量时出错:", e.message);
    }
}

// 打印日志
YUI.log('[First Script] 变量已定义');
YUI.setText('infoLabel', '第一个脚本已执行');