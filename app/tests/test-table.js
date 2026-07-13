function onTableSelect(layerId) {
    var layer = yui.find(layerId);
    if (!layer || !layer.text) return;
    try {
        var row = JSON.parse(layer.text);
        var label = yui.find("statusLabel");
        if (label) {
            label.text = "选中: #" + row.id + " " + row.name + " (" + row.role + ")";
        }
    } catch (e) {}
}

function onLoad() {
    YUI.log("Table test loaded");
}
