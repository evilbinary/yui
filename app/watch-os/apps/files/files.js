var filesBrowsePath = "app/watch-os";

function onFilesLoad() {
    refreshFilesList();
}

function onFilesShow() {
    applyWatchTheme();
    refreshFilesList();
}

function refreshFilesList() {
    YUI.setText("files_path", filesBrowsePath + "/");
    if (typeof YUI.listDir !== "function") {
        YUI.setText("files_status", "listDir 不可用");
        return;
    }
    var entries = YUI.listDir(filesBrowsePath);
    if (!entries) {
        YUI.setText("files_status", "无法读取目录");
        return;
    }
    for (var i = 0; i < 6; i++) {
        var label = "—";
        if (i < entries.length) {
            var e = entries[i];
            label = (e.isDir ? "📁 " : "📄 ") + e.name;
        }
        YUI.setText("files_row_" + i, label);
    }
    YUI.setText("files_status", entries.length + " 项");
}
