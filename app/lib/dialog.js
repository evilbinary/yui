/**
 * YUI Dialog API — 命令式对话框
 *
 * 用法:
 *   YUI.alert("保存成功")
 *   YUI.confirm("确定删除？", function(ok) { if (ok) ... })
 *   YUI.prompt("文件名", "默认.json", function(name) { if (name) ... })
 *   YUI.dialog({ title: "...", content: [...], buttons: [...] })
 *   YUI.openFile({ title: "选择图片", filter: "*.jpg,*.png" }, function(path) { ... })
 *   // filter 支持通配符: *.jpg  .png  image_*  *  *.{jpg,png}
 */
(function () {

var seq = 0;

function uid() {
    return "yui_dlg_" + (++seq);
}

function regGlobal(name, fn) {
    globalThis[name] = fn;
}

function unregGlobal(name) {
    delete globalThis[name];
}

function buildButtons(dialogId, buttons, callback) {
    var cols = [];
    for (var i = 0; i < buttons.length; i++) {
        (function (btn, idx) {
            var btnId = dialogId + "_btn_" + idx;
            var cbName = dialogId + "_cb_" + idx;
            var result = btn.value !== undefined ? btn.value : btn.text;

            regGlobal(cbName, function () {
                if (btn.onClick) btn.onClick();
                if (callback) callback(result);
                if (typeof YUI.hide === "function") YUI.hide(dialogId);
                unregGlobal(cbName);
                for (var j = 0; j < buttons.length; j++) {
                    var c = dialogId + "_cb_" + j;
                    if (c !== cbName) unregGlobal(c);
                }
            });

            cols.push({
                id: btnId,
                type: "Button",
                text: btn.text,
                size: [0, 36],
                flex: 1,
                style: {
                    color: btn.primary ? "#ffffff" : "#e0e0e0",
                    bgColor: btn.primary ? "#7c3aed" : "#2a2a4a",
                    borderRadius: 8,
                    fontSize: 13
                },
                events: { onClick: "@" + cbName }
            });
        })(buttons[i], i);
    }
    return cols;
}

function showDialog(opts, callback) {
    var dialogId = uid();
    var rootId = opts.root || "photoApp";
    var contentList = opts.content || [];
    if (!(contentList instanceof Array)) contentList = [contentList];

    var buttons = opts.buttons || [{ text: "确定", value: true }];
    var btnCols = buildButtons(dialogId, buttons, callback);

    var dialogW = opts.width || 320;
    var boxH = opts.height || 180;
    var appW = 390;
    var boxX = (appW - dialogW) / 2;
    var bodyH = boxH - 44 - 52;

    var json = {
        id: dialogId,
        type: "View",
        size: [appW, 844],
        position: [0, 0],
        style: { bgColor: "transparent" },
        children: [
            {
                id: dialogId + "_overlay",
                type: "View",
                size: [appW, 844], position: [0, 0],
                style: { bgColor: "rgba(0,0,0,0.5)" }
            },
            {
                id: dialogId + "_box",
                type: "View", size: [dialogW, boxH], position: [boxX, 220],
                style: { bgColor: "#1a1a2e", borderRadius: 12 },
                layout: { type: "vertical", spacing: 0, padding: [0, 0, 0, 0] },
                children: [
                    {
                        id: dialogId + "_title",
                        type: "Label", text: opts.title || "提示",
                        size: [dialogW, 44],
                        style: { color: "#e0e0e0", fontSize: 15, fontWeight: "bold", textAlign: "center", bgColor: "#2a2a4a" }
                    },
                    {
                        id: dialogId + "_body",
                        type: "View", size: [dialogW, bodyH],
                        style: { bgColor: "transparent" },
                        layout: { type: "vertical", spacing: 8, padding: [16, 20, 16, 20], align: "center", justify: "center" },
                        children: contentList
                    },
                    {
                        id: dialogId + "_btns",
                        type: "View", size: [dialogW, 52],
                        style: { bgColor: "#12121f" },
                        layout: { type: "horizontal", spacing: 8, padding: [8, 12, 8, 12], align: "center" },
                        children: btnCols
                    }
                ]
            }
        ]
    };

    var ret = YUI.renderFromJson(rootId, JSON.stringify(json), true);
    if (ret < 0) {
        if (callback) callback(null);
        return;
    }
    if (typeof YUI.show === "function") YUI.show(dialogId);
}

// --- 公开 API ---

function alert(msg, opts, callback) {
    if (typeof opts === "function") { callback = opts; opts = {}; }
    opts = opts || {};
    callback = callback || function () {};
    showDialog({
        root: opts.root,
        title: opts.title || "提示",
        width: opts.width || 280, height: 140,
        content: [{
            id: uid() + "_text", type: "Label",
            text: String(msg), size: [240, 40],
            style: { color: "#e0e0e0", fontSize: 14, textAlign: "center" }
        }],
        buttons: [{ text: "确定", value: true }]
    }, function () { if (callback) callback(); });
}

function confirm(msg, opts, callback) {
    if (typeof opts === "function") { callback = opts; opts = {}; }
    opts = opts || {};
    callback = callback || function () {};
    showDialog({
        root: opts.root,
        title: opts.title || "确认操作",
        width: opts.width || 300, height: 160,
        content: [{
            id: uid() + "_text", type: "Label",
            text: String(msg), size: [260, 40],
            style: { color: "#e0e0e0", fontSize: 14, textAlign: "center" }
        }],
        buttons: [
            { text: "取消", value: false },
            { text: "确定", value: true, primary: true }
        ]
    }, callback);
}

function prompt(msg, defaultVal, opts, callback) {
    if (typeof opts === "function") { callback = opts; opts = {}; }
    opts = opts || {};
    callback = callback || function () {};
    var inputId = uid() + "_input";
    showDialog({
        root: opts.root,
        title: opts.title || "输入",
        width: opts.width || 320, height: 200,
        content: [
            {
                id: uid() + "_label", type: "Label",
                text: String(msg), size: [280, 24],
                style: { color: "#94a3b8", fontSize: 13, textAlign: "left" }
            },
            {
                id: inputId, type: "Input",
                text: String(defaultVal || ""),
                placeholder: opts.placeholder || "",
                size: [280, 38],
                style: { color: "#e0e0e0", bgColor: "#0f0f1a", fontSize: 13, padding: [6, 10, 6, 10], borderRadius: 8 }
            }
        ],
        buttons: [
            { text: "取消", value: null },
            { text: "确定", value: true, primary: true }
        ]
    }, function (res) {
        if (res === true) {
            if (callback) callback(YUI.getText(inputId) || "");
        } else {
            if (callback) callback(null);
        }
    });
}

// 注册到 YUI
if (typeof YUI !== "undefined") {
    YUI.alert = alert;
    YUI.confirm = confirm;
    YUI.prompt = prompt;
    YUI.dialog = showDialog;
}

// ====================== File Browser ======================

var fileDialogResolve = null;
var fileDialogPath = "./";
var fileDialogFilter = null;
var fileDialogLoaded = false;
var fileDialogCallback = null;

function findLayer(id) {
    if (typeof YUI !== "undefined" && typeof YUI.find === "function") {
        return YUI.find(id);
    }
    if (typeof yui !== "undefined" && typeof yui.find === "function") {
        return yui.find(id);
    }
    return null;
}

function readDialogJson() {
    var paths = [
        "app/lib/dialog.json",
        "../lib/dialog.json",
        "lib/dialog.json"
    ];
    var i;
    for (i = 0; i < paths.length; i++) {
        try {
            var raw = YUI.readFile(paths[i]);
            if (raw) {
                return { raw: raw, path: paths[i] };
            }
        } catch (e) {
            YUI.log("FileDialog: read failed " + paths[i] + " -> " + e);
        }
    }
    return null;
}

/** 支持单层 { id, type, ... } 或片段 { root, children: [...] } */
function parseDialogPayload(raw, defaultRootId) {
    var doc;
    try {
        doc = JSON.parse(raw);
    } catch (e) {
        YUI.log("FileDialog: invalid dialog.json: " + e);
        return null;
    }
    if (!doc || typeof doc !== "object") {
        return null;
    }
    if (doc.id && doc.type) {
        return { parentId: defaultRootId, layer: doc };
    }
    if (doc.children && doc.children.length > 0) {
        return {
            parentId: doc.root || defaultRootId,
            layer: doc.children[0]
        };
    }
    return null;
}

function ensureFileDialogLoaded(rootId) {
    if (fileDialogLoaded && findLayer("photoFileDialog")) {
        return true;
    }
    var file = readDialogJson();
    if (!file) {
        YUI.log("FileDialog: cannot read dialog.json");
        return false;
    }
    var payload = parseDialogPayload(file.raw, rootId);
    if (!payload) {
        YUI.log("FileDialog: dialog.json format invalid");
        return false;
    }
    var ret = YUI.renderFromJson(
        payload.parentId,
        JSON.stringify(payload.layer),
        true,
        file.path
    );
    if (ret !== 0) {
        YUI.log("FileDialog: renderFromJson failed: " + ret);
        return false;
    }
    if (!findLayer("photoFileDialog") || !findLayer("photoFileList")) {
        YUI.log("FileDialog: layer tree mount failed");
        return false;
    }
    fileDialogLoaded = true;
    return true;
}

function normalizePath(path) {
    if (!path || path === "") return "./";
    return path.replace(/\\/g, "/");
}

/** 将单个通配符模式转为 RegExp；返回 null 表示匹配所有文件 */
function globPatternToRegExp(pattern) {
    var s = String(pattern || "").trim().toLowerCase();
    if (!s || s === "*" || s === "*.*" || s === ".*") {
        return null;
    }
    // 纯扩展名简写: .jpg / jpg
    if (s.charAt(0) === "." && s.indexOf("*") < 0 && s.indexOf("?") < 0 && s.indexOf("{") < 0) {
        return new RegExp(s.replace(/[.+^${}()|[\]\\]/g, "\\$&") + "$");
    }
    if (s.charAt(0) !== "*" && s.charAt(0) !== "?" && s.indexOf("/") < 0 &&
        s.indexOf("*") < 0 && s.indexOf("?") < 0 && s.indexOf("{") < 0) {
        return new RegExp("\\." + s.replace(/[.+^${}()|[\]\\]/g, "\\$&") + "$");
    }
    var re = "^";
    for (var i = 0; i < s.length; i++) {
        var c = s.charAt(i);
        if (c === "*") {
            re += ".*";
        } else if (c === "?") {
            re += ".";
        } else if (c === "{") {
            var end = s.indexOf("}", i);
            if (end > i) {
                var alts = s.substring(i + 1, end).split(",");
                var j;
                re += "(?:";
                for (j = 0; j < alts.length; j++) {
                    if (j > 0) re += "|";
                    re += alts[j].trim().replace(/[.+^${}()|[\]\\]/g, "\\$&");
                }
                re += ")";
                i = end;
            } else {
                re += "\\{";
            }
        } else {
            re += c.replace(/[.+^${}()|[\]\\]/g, "\\$&");
        }
    }
    re += "$";
    return new RegExp(re);
}

/** filter: 字符串(逗号分隔)或字符串数组，支持 *.jpg / .png / * / image_* */
function matchFileFilter(filename, filterSpec) {
    if (!filterSpec) return true;
    var patterns = filterSpec instanceof Array ? filterSpec : String(filterSpec).split(",");
    var name = String(filename || "").toLowerCase();
    var i;
    for (i = 0; i < patterns.length; i++) {
        var re = globPatternToRegExp(patterns[i]);
        if (re === null) return true;
        if (re.test(name)) return true;
    }
    return false;
}

function onPhotoFileUp() {
    var path = fileDialogPath || "./";
    path = path.replace(/\\/g, "/");
    if (path.endsWith("/")) path = path.substring(0, path.length - 1);
    if (path === "" || path === ".") {
        fileDialogPath = "./";
    } else {
        var idx = path.lastIndexOf("/");
        fileDialogPath = idx >= 0 ? path.substring(0, idx + 1) : "./";
    }
    refreshPhotoFileList();
}

function onPhotoFileSelect(layerId) {
    if (!layerId) return;
    try {
        var txt = YUI.getText(layerId);
        if (!txt) return;
        var node = JSON.parse(txt);
        if (node._isDir) {
            var newPath = fileDialogPath;
            if (!newPath.endsWith("/")) newPath += "/";
            newPath += node.text + "/";
            fileDialogPath = newPath;
            refreshPhotoFileList();
        } else {
            YUI.setText("photoFileInput", node.text);
        }
    } catch (e) {}
}

function refreshPhotoFileList() {
    var path = fileDialogPath || "./";
    YUI.setText("photoFilePath", path);

    var entries = YUI.listDir(path);
    YUI.log("FileDialog: listDir(" + path + ") count=" + (entries ? entries.length : "null"));
    var treeData = [];
    if (entries) {
        var dirs = [];
        var files = [];
        for (var i = 0; i < entries.length; i++) {
            var e = entries[i];
            if (e.name === "." || e.name === "..") continue;
            if (e.isDir) {
                dirs.push({ text: e.name, icon: "folder", icon_text: "📁", expandable: false, _isDir: true });
            } else {
                if (fileDialogFilter && !matchFileFilter(e.name, fileDialogFilter)) {
                    continue;
                }
                files.push({ text: e.name, icon: "file", icon_text: "🖼", _isDir: false });
            }
        }
        treeData = dirs.concat(files);
    }
    var tree = findLayer("photoFileList");
    if (tree) {
        tree.data = treeData;
        YUI.log("FileDialog: tree items=" + treeData.length);
    } else {
        YUI.log("FileDialog: photoFileList layer not found");
    }
}

function onPhotoFileOk() {
    var input = YUI.getText("photoFileInput") || "";
    var filename = input.trim();
    if (!filename) return;

    var path = fileDialogPath;
    if (!path.endsWith("/")) path += "/";
    var fullPath = path + filename;

    YUI.hide("photoFileDialog");
    if (fileDialogCallback) {
        fileDialogCallback(fullPath);
        fileDialogCallback = null;
    }
}

function onPhotoFileCancel() {
    YUI.hide("photoFileDialog");
    if (fileDialogCallback) {
        fileDialogCallback(null);
        fileDialogCallback = null;
    }
}

globalThis.onPhotoFileUp = onPhotoFileUp;
globalThis.onPhotoFileSelect = onPhotoFileSelect;
globalThis.onPhotoFileOk = onPhotoFileOk;
globalThis.onPhotoFileCancel = onPhotoFileCancel;

function openFile(opts, callback) {
    if (typeof opts === "function") { callback = opts; opts = {}; }
    opts = opts || {};
    callback = callback || function () {};
    var rootId = opts.root || "photoApp";

    if (!ensureFileDialogLoaded(rootId)) {
        callback(null);
        return;
    }

    fileDialogPath = normalizePath(opts.startPath || "app/assets");
    fileDialogFilter = opts.filter || null;
    YUI.setText("photoFileTitle", opts.title || "选择文件");
    YUI.setText("photoFileInput", "");
    refreshPhotoFileList();
    YUI.show("photoFileDialog");
    fileDialogCallback = callback;
}

YUI.openFile = openFile;

// 供测试用例访问（app/tests/test-dialog-lib.js）
if (typeof YUI !== "undefined") {
    YUI.__dialogTest__ = {
        matchFileFilter: matchFileFilter,
        parseDialogPayload: parseDialogPayload,
        normalizePath: normalizePath,
        ensureFileDialogLoaded: ensureFileDialogLoaded
    };
}

})();
