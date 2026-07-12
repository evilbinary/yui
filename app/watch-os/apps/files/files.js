/**
 * 文件管理器 - 动态 List（list_component）
 */

var filesBrowsePath = "app/watch-os";
var filesEntries = [];

function onFilesLoad() {
    refreshFilesList();
}

function onFilesShow() {
    applyWatchTheme();
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

    var dirs = [];
    var files = [];
    for (var i = 0; i < entries.length; i++) {
        var e = entries[i];
        var item = {
            name: e.name,
            icon: e.isDir ? "📁" : "📄",
            isDir: !!e.isDir
        };
        if (item.isDir) dirs.push(item);
        else files.push(item);
    }
    filesEntries = dirs.concat(files);

    YUI.update({
        target: "files_list",
        change: { data: filesEntries }
    });

    YUI.setText("files_status", filesEntries.length + " 项");
}

function onFilesItemClick(layerId) {
    var raw = YUI.getText("files_list");
    if (!raw) return;

    var item;
    try {
        item = JSON.parse(raw);
    } catch (e) {
        return;
    }
    if (!item || !item.name) return;

    if (item.isDir) {
        filesBrowsePath = filesBrowsePath + "/" + item.name;
        refreshFilesList();
    }
}

function onFilesUp() {
    var path = filesBrowsePath;
    if (path.endsWith("/")) path = path.substring(0, path.length - 1);
    var idx = path.lastIndexOf("/");
    if (idx > 0) {
        filesBrowsePath = path.substring(0, idx);
    } else {
        filesBrowsePath = ".";
    }
    refreshFilesList();
}
