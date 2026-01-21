// YUI Inspect 功能测试脚本

// 切换全局 inspect 模式的标志
var inspectEnabled = false;
var showBounds = false;
var showInfo = false;

// 为按钮绑定事件
function bindEvents() {
    // 切换 Inspect 按钮
    YUI.setEvent('toggle_inspect', 'click', 'onToggleInspect');
    
    // 切换边界按钮
    YUI.setEvent('toggle_bounds', 'click', 'onToggleBounds');
    
    // 切换信息按钮
    YUI.setEvent('toggle_info', 'click', 'onToggleInfo');
    
    // 为 header 中的按钮绑定事件
    for (var i = 1; i <= 3; i++) {
        YUI.setEvent('btn' + i, 'click', 'onHeaderButtonClick');
    }
}

// 切换 Inspect 按钮的回调
function onToggleInspect(layer) {
    toggleInspect();
}

// 切换边界按钮的回调
function onToggleBounds(layer) {
    toggleBounds();
}

// 切换信息按钮的回调
function onToggleInfo(layer) {
    toggleInfo();
}

// Header 按钮的回调
function onHeaderButtonClick(layer) {
    YUI.log('Clicked: ' + layer.id);
    // 为这个按钮启用 inspect
    YUI.inspect.setLayer(layer.id, true);
}

// 切换全局 inspect 模式
function toggleInspect() {
    inspectEnabled = !inspectEnabled;
    if (inspectEnabled) {
        YUI.inspect.enable();
        YUI.log('Inspect 模式已启用');
    } else {
        YUI.inspect.disable();
        YUI.log('Inspect 模式已禁用');
    }
}

// 切换边界显示
function toggleBounds() {
    showBounds = !showBounds;
    YUI.inspect.setShowBounds(showBounds);
    YUI.log('边界显示: ' + (showBounds ? '开启' : '关闭'));
}

// 切换信息显示
function toggleInfo() {
    showInfo = !showInfo;
    YUI.inspect.setShowInfo(showInfo);
    YUI.log('信息显示: ' + (showInfo ? '开启' : '关闭'));
}

// 初始化
function init() {
    YUI.log('YUI Inspect 测试脚本加载完成');
    YUI.log('可用的 Inspect API:');
    YUI.log('- YUI.inspect.enable() - 启用全局 inspect 模式');
    YUI.log('- YUI.inspect.disable() - 禁用全局 inspect 模式');
    YUI.log('- YUI.inspect.setLayer(layerId, enabled) - 为特定图层设置 inspect');
    YUI.log('- YUI.inspect.setShowBounds(show) - 设置是否显示边界框');
    YUI.log('- YUI.inspect.setShowInfo(show) - 设置是否显示详细信息');
    
    // 绑定事件
    bindEvents();
    
    YUI.log('点击 "切换Inspect" 按钮来启用/禁用 inspect 模式');
    YUI.log('在 inspect 模式下，所有图层会显示红色边界框和详细信息');
}

// 执行初始化
init();
