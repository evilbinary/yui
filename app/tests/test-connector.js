// Connector / Draggable 组件测试

var g_nodeAOffset = 0;
var g_edgeACAdded = false;
var g_dynamicNodeSeq = 0;
var g_dynamicNodeIds = [];

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
        assertLayerExists("edgeInputToB");
        assertLayerExists("edgeInputToPort");
        assertLayerExists("graphCanvas");

        var nodeA = findLayer("nodeA");
        if (!nodeA || !nodeA.size || nodeA.size[0] <= 0 || nodeA.size[1] <= 0) {
            throw new Error("nodeA 尺寸异常");
        }

        setStatus("检查通过：节点/子端口/连线均存在", "#a6e3a1");
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
            position: [40 + g_nodeAOffset, 50 + g_nodeAOffset]
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
                    strokeWidth: 2
                }
            }]
        }
    });

    g_edgeACAdded = true;
    setStatus("已添加 A→C 垂直连线", "#f9e2af");
}

function addDynamicNode() {
    g_dynamicNodeSeq += 1;
    var nodeId = "dynNode" + g_dynamicNodeSeq;
    var inputId = nodeId + "Input";
    var selectId = nodeId + "Select";
    var offset = g_dynamicNodeSeq * 28;

    g_dynamicNodeIds.push(nodeId);
    YUI.log("addDynamicNode: " + nodeId);

    YUI.update({
        target: "graphCanvas",
        change: {
            children: [{
                id: nodeId,
                type: "Draggable",
                position: [80 + offset, 320 + offset],
                size: [220, 148],
                dragHandleHeight: 36,
                style: {
                    bgColor: "#313244",
                    borderRadius: 10
                },
                children: [{
                    id: nodeId + "Body",
                    type: "View",
                    position: [0, 0],
                    size: [220, 148],
                    layout: {
                        type: "vertical",
                        spacing: 8,
                        padding: [12, 12, 12, 12]
                    },
                    children: [
                        {
                            id: nodeId + "Title",
                            type: "Label",
                            text: "动态节点 " + g_dynamicNodeSeq + " (拖动标题栏)",
                            size: [0, 20],
                            style: {
                                color: "#94e2d5",
                                fontSize: 13
                            }
                        },
                        {
                            id: inputId,
                            type: "Input",
                            connectable: true,
                            size: [0, 32],
                            placeholder: "输入参数",
                            style: {
                                bgColor: "#1e1e2e",
                                textColor: "#f8f8f2",
                                borderColor: "#45475a",
                                borderRadius: 4,
                                fontSize: 13
                            }
                        },
                        {
                            id: selectId,
                            type: "Select",
                            text: "选项 A",
                            size: [0, 32],
                            style: {
                                bgColor: "#1e1e2e",
                                color: "#f8f8f2",
                                fontSize: 13,
                                borderWidth: 1,
                                borderColor: "#45475a",
                                dividerColor: "#45475a",
                                borderRadius: 4
                            },
                            items: [
                                { label: "选项 A", value: "a" },
                                { label: "选项 B", value: "b" },
                                { label: "选项 C", value: "c" }
                            ]
                        }
                    ]
                }]
            }]
        }
    });

    setStatus("已添加 " + nodeId + "（含 Input / Select）", "#94e2d5");
}

function resetNodes() {
    g_nodeAOffset = 0;
    g_edgeACAdded = false;

    YUI.log("resetNodes");
    YUI.update({
        target: "nodeA",
        change: { position: [40, 50] }
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

    var i;
    var removeChange = {};
    for (i = 0; i < g_dynamicNodeIds.length; i++) {
        removeChange["children." + g_dynamicNodeIds[i]] = null;
    }
    if (g_dynamicNodeIds.length > 0) {
        YUI.update({
            target: "graphCanvas",
            change: removeChange
        });
    }
    g_dynamicNodeSeq = 0;
    g_dynamicNodeIds = [];

    setStatus("节点位置已重置，动态连线/节点已移除", "#cdd6f4");
}

function onLoad() {
    YUI.log("Connector 测试页加载完成");
    YUI.log("手动：拖拽 A/B/C 节点观察贝塞尔连线");
    YUI.log("手动：在节点圆点上按下并拖到另一节点圆点创建连线");
    YUI.log("手动：左键拖圆点新建连线（可多条）；左键拖线条可移动该线端点");
    YUI.log("手动：右键圆点/连线可删除");
    YUI.log("自动：运行检查 / 移动 nodeA / 添加 A→C / 添加节点 / 重置");
    setTimeout(runConnectorTest, 200);
}
