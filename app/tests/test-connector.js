// Connector / Draggable 组件测试

var g_nodeAOffset = 0;
var g_edgeACAdded = false;

function findLayer(layerId) {
    if (typeof yui !== "undefined" && typeof yui.find === "function") {
        return yui.find(layerId);
    }
    if (typeof YUI !== "undefined" && typeof YUI.find === "function") {
        return YUI.find(layerId);
    }
    return null;
}

function setStatus(text, color) {
    YUI.update({
        target: "statusLabel",
        change: {
            text: "状态：" + text,
            color: color || "#a6adc8"
        }
    });
}

function assertLayerExists(layerId) {
    var layer = findLayer(layerId);
    if (!layer) {
        throw new Error("缺少图层: " + layerId);
    }
    return layer;
}

function runConnectorTest() {
    YUI.log("runConnectorTest: start");
    try {
        assertLayerExists("nodeA");
        assertLayerExists("nodeB");
        assertLayerExists("nodeC");
        assertLayerExists("edgeAB");
        assertLayerExists("edgeBC");
        assertLayerExists("graphCanvas");

        var nodeA = findLayer("nodeA");
        if (!nodeA.rect || nodeA.rect.w <= 0 || nodeA.rect.h <= 0) {
            throw new Error("nodeA 尺寸异常");
        }

        setStatus("检查通过：3 节点 + 2 连线均存在", "#a6e3a1");
        YUI.log("runConnectorTest: passed");
    } catch (err) {
        var message = err && err.message ? err.message : String(err);
        setStatus("检查失败：" + message, "#f38ba8");
        YUI.log("runConnectorTest: failed - " + message);
    }
}

function moveNodeA() {
    g_nodeAOffset += 40;
    if (g_nodeAOffset > 120) {
        g_nodeAOffset = 0;
    }

    YUI.log("moveNodeA: offset=" + g_nodeAOffset);
    YUI.update({
        target: "nodeA",
        change: {
            position: [40 + g_nodeAOffset, 120 + g_nodeAOffset]
        }
    });
    setStatus("已移动 nodeA，连线应自动跟随", "#89b4fa");
}

function addEdgeAC() {
    if (g_edgeACAdded) {
        setStatus("A→C 连线已存在", "#f9e2af");
        YUI.log("addEdgeAC: already added");
        return;
    }

    YUI.log("addEdgeAC");
    YUI.update({
        target: "graphCanvas",
        change: {
            children: [{
                id: "edgeAC",
                type: "Connector",
                position: [0, 0],
                size: [0, 0],
                from: { id: "nodeA", anchor: "bottom" },
                to: { id: "nodeC", anchor: "bottom" },
                curve: "auto-vertical",
                style: {
                    color: "#f9e2af",
                    strokeWidth: 2,
                    arrow: true
                }
            }]
        }
    });

    g_edgeACAdded = true;
    setStatus("已添加 A→C 垂直连线", "#f9e2af");
}

function resetNodes() {
    g_nodeAOffset = 0;
    g_edgeACAdded = false;

    YUI.log("resetNodes");
    YUI.update({
        target: "nodeA",
        change: { position: [40, 120] }
    });
    YUI.update({
        target: "nodeB",
        change: { position: [280, 200] }
    });
    YUI.update({
        target: "nodeC",
        change: { position: [520, 120] }
    });
    YUI.update({
        target: "graphCanvas",
        change: { "children.edgeAC": null }
    });

    setStatus("节点位置已重置，动态连线已移除", "#cdd6f4");
}

function onLoad() {
    YUI.log("Connector 测试页加载完成");
    YUI.log("手动：拖拽 A/B/C 节点观察贝塞尔连线");
    YUI.log("自动：运行检查 / 移动 nodeA / 添加 A→C / 重置");
    setTimeout(runConnectorTest, 200);
}
