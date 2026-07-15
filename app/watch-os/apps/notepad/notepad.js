/**
 * Watch OS 记事本 - 列表浏览 + 多行编辑 + 文件持久化
 */

var NOTEPAD_DIR = "app/watch-os/notes";
var npCurrentFile = null;

function onNotepadLoad() {
    showNotepadList();
    refreshNotepadList();
}

function onNotepadShow() {
    applyWatchTheme();
    if (npCurrentFile) {
        showNotepadEdit();
    } else {
        showNotepadList();
        refreshNotepadList();
    }
}

function showNotepadList() {
    YUI.show("np_list_panel");
    YUI.hide("np_edit_panel");
}

function showNotepadEdit() {
    YUI.hide("np_list_panel");
    YUI.show("np_edit_panel");
}

function notepadIsTextFile(name) {
    if (!name) return 0;
    var len = name.length;
    return len > 4 && name.substring(len - 4) === ".txt";
}

function refreshNotepadList() {
    var entries = [];
    if (typeof YUI.listDir === "function") {
        var list = YUI.listDir(NOTEPAD_DIR);
        if (list) {
            for (var i = 0; i < list.length; i++) {
                var e = list[i];
                if (!e.isDir && notepadIsTextFile(e.name)) {
                    entries.push({
                        name: e.name.substring(0, e.name.length - 4),
                        file: e.name,
                        icon: "📝"
                    });
                }
            }
        }
    }
    entries.sort(function(a, b) {
        return a.name < b.name ? -1 : (a.name > b.name ? 1 : 0);
    });
    YUI.update({
        target: "np_list",
        change: { data: entries }
    });
    YUI.setText("np_status", entries.length + " 条备忘");
}

function onNotepadItemClick(layerId) {
    var raw = YUI.getText("np_list");
    if (!raw) return;

    var item;
    try {
        item = JSON.parse(raw);
    } catch (e) {
        return;
    }
    if (!item || !item.file) return;
    openNotepadNote(item.file);
}

function openNotepadNote(filename) {
    npCurrentFile = NOTEPAD_DIR + "/" + filename;
    var content = "";
    if (typeof YUI.readFile === "function") {
        var text = YUI.readFile(npCurrentFile);
        if (text) content = text;
    }
    YUI.setText("np_edit_title", filename.substring(0, filename.length - 4));
    YUI.setText("np_body", content);
    showNotepadEdit();
}

function newNotepadNote() {
    var now = new Date();
    var name = "note-" +
        now.getFullYear() +
        pad2(now.getMonth() + 1) +
        pad2(now.getDate()) + "-" +
        pad2(now.getHours()) +
        pad2(now.getMinutes()) +
        pad2(now.getSeconds()) + ".txt";
    npCurrentFile = NOTEPAD_DIR + "/" + name;
    YUI.setText("np_edit_title", "新备忘");
    YUI.setText("np_body", "");
    showNotepadEdit();
}

function pad2(n) {
    return n < 10 ? "0" + n : "" + n;
}

function saveNotepadNote() {
    if (!npCurrentFile) return;
    var text = YUI.getText("np_body") || "";
    if (typeof YUI.writeFile !== "function") {
        YUI.setText("np_status", "writeFile 不可用");
        return;
    }
    if (YUI.writeFile(npCurrentFile, text)) {
        npCurrentFile = null;
        showNotepadList();
        refreshNotepadList();
        YUI.setText("np_status", "已保存");
    } else {
        YUI.setText("np_status", "保存失败");
    }
}

function backNotepadList() {
    npCurrentFile = null;
    showNotepadList();
    refreshNotepadList();
}

function clearNotepadBody() {
    YUI.setText("np_body", "");
}
