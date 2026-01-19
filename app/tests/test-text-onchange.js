// Text 组件 onChange 事件测试

function onTextChange() {
    var text = YUI.getText("inputText");
    YUI.log("onTextChange: text changed to: " + text);
    YUI.setText("outputLabel", "文本已更改:" + text);
}

function onLoad() {
    YUI.log("Test onChange event for Text component");
}
