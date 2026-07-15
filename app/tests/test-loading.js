// Loading 组件测试

var g_loadingVisible = false;

function showLoader() {
    YUI.log("showLoader");
    g_loadingVisible = true;

    YUI.update({
        target: "loadPanel",
        change: {
            children: [{
                id: "overlayLoader",
                type: "Loading",
                visible: true,
                size: [56, 56],
                variant: "spinner",
                text: "加载中...",
                color: "#89b4fa",
                trackColor: "#45475a",
                strokeWidth: 4,
                speed: 1.0
            }]
        }
    });

    YUI.update({
        target: "panelHint",
        change: { visible: false }
    });

    YUI.update({
        target: "statusLabel",
        change: { text: "状态：Loading 已显示", color: "#89b4fa" }
    });
}

function hideLoader() {
    YUI.log("hideLoader");
    g_loadingVisible = false;

    YUI.update({
        target: "loadPanel",
        change: { "children.overlayLoader": null }
    });

    YUI.update({
        target: "panelHint",
        change: { visible: true }
    });

    YUI.update({
        target: "statusLabel",
        change: { text: "状态：Loading 已隐藏", color: "#a6adc8" }
    });
}

function simulateRequest() {
    if (g_loadingVisible) {
        YUI.log("simulateRequest: already loading");
        return;
    }

    YUI.log("simulateRequest: start");
    showLoader();

    YUI.update({
        target: "statusLabel",
        change: { text: "状态：请求中（2 秒）...", color: "#f9e2af" }
    });

    setTimeout(function() {
        hideLoader();
        YUI.update({
            target: "statusLabel",
            change: { text: "状态：请求完成", color: "#a6e3a1" }
        });
        YUI.log("simulateRequest: done");
    }, 2000);
}

function onLoad() {
    YUI.log("Loading 测试页加载完成");
    YUI.log("静态区：spinner / dots / spinner+text");
    YUI.log("交互：显示、隐藏、模拟 2 秒请求");
}
