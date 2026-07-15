// Grid 组件 + layout.type=grid 测试

var g_extraCellIndex = 0;
var g_lastExtraCellId = "";

function setStatus(text, color) {
    YUI.update({
        target: "statusLabel",
        change: {
            text: "状态：" + text,
            color: color || "#a6adc8"
        }
    });
}

function onCellClick(layerId) {
    var layer = yui.find(layerId);
    var label = layer && layer.text ? layer.text : layerId;
    YUI.log("onCellClick: " + layerId + " (" + label + ")");
    setStatus("点击格子 " + label + " [" + layerId + "]", "#a6e3a1");
}

function setGridColumns(columns) {
    YUI.log("setGridColumns: " + columns);
    YUI.update({
        target: "gridComponent",
        change: {
            layout: {
                type: "grid",
                columns: columns,
                spacing: 6
            }
        }
    });
    setStatus("Grid 组件改为 " + columns + " 列", "#89b4fa");
}

function setGridColumns2() { setGridColumns(2); }
function setGridColumns3() { setGridColumns(3); }
function setGridColumns4() { setGridColumns(4); }

function setLayoutColumns(columns) {
    YUI.log("setLayoutColumns: " + columns);
    YUI.update({
        target: "layoutGrid",
        change: {
            layout: {
                type: "grid",
                columns: columns,
                spacing: 8
            }
        }
    });
    setStatus("layoutGrid 改为 " + columns + " 列", "#74c7ec");
}

function setLayoutColumns2() { setLayoutColumns(2); }
function setLayoutColumns4() { setLayoutColumns(4); }
function setLayoutColumns6() { setLayoutColumns(6); }

function addGridCell() {
    g_extraCellIndex++;
    g_lastExtraCellId = "g_extra_" + g_extraCellIndex;
    YUI.log("addGridCell: " + g_lastExtraCellId);

    YUI.update({
        target: "gridComponent",
        change: {
            children: [{
                id: g_lastExtraCellId,
                type: "Button",
                text: "+",
                style: {
                    bgColor: "#f9e2af",
                    color: "#1e1e2e",
                    fontSize: 14,
                    borderRadius: 4
                },
                events: { onClick: "@onCellClick" }
            }]
        }
    });
    setStatus("已添加格子 " + g_lastExtraCellId, "#f9e2af");
}

function removeGridCell() {
    if (!g_lastExtraCellId) {
        setStatus("没有可移除的动态格子", "#f38ba8");
        return;
    }

    var removeId = g_lastExtraCellId;
    YUI.log("removeGridCell: " + removeId);

    var change = {};
    change["children." + removeId] = null;

    YUI.update({
        target: "gridComponent",
        change: change
    });

    g_lastExtraCellId = "";
    setStatus("已移除格子 " + removeId, "#f38ba8");
}

function onLoad() {
    YUI.log("Grid 测试页加载完成");
    YUI.log("A区: type=Grid + columns");
    YUI.log("B区: View + layout.type=grid");
    YUI.log("C区: 增量更新列数 / 增删子项");
}
