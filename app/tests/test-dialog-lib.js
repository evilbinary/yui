/**
 * dialog.js 测试用例
 * 运行: ya -r qjs -- app/tests/test-dialog-lib.json
 */

var testLogs = [];
var testPass = 0;
var testFail = 0;
var AUTO_RUN = 1;

function appendLog(msg) {
    testLogs.push(msg);
    if (testLogs.length > 40) {
        testLogs = testLogs.slice(testLogs.length - 40);
    }
    YUI.setText("log", "log:\n" + testLogs.join("\n"));
    YUI.log("[dialog-test] " + msg);
}

function updateSummary() {
    var total = testPass + testFail;
    var color = testFail > 0 ? "#f38ba8" : "#a6e3a1";
    YUI.setText("summary", "通过 " + testPass + " / 失败 " + testFail + " / 共 " + total);
    YUI.update({ target: "summary", change: { style: { color: color } } });
}

function assert(name, cond) {
    if (cond) {
        testPass += 1;
        appendLog("PASS  " + name);
    } else {
        testFail += 1;
        appendLog("FAIL  " + name);
    }
    updateSummary();
}

function resetStats() {
    testPass = 0;
    testFail = 0;
    updateSummary();
}

function testApi() {
    assert("YUI.alert", typeof YUI.alert === "function");
    assert("YUI.confirm", typeof YUI.confirm === "function");
    assert("YUI.prompt", typeof YUI.prompt === "function");
    assert("YUI.dialog", typeof YUI.dialog === "function");
    assert("YUI.pickFile", typeof YUI.pickFile === "function");
    assert("YUI.__dialogTest__", typeof YUI.__dialogTest__ === "object");
}

function testNormalizePath() {
    var n = YUI.__dialogTest__.normalizePath;
    assert("normalize empty", n("") === "./");
    assert("normalize backslash", n("app\\assets") === "app/assets");
    assert("normalize keep", n("app/assets") === "app/assets");
}

function testMatchFileFilter() {
    var m = YUI.__dialogTest__.matchFileFilter;
    assert("filter null", m("any.txt", null) === true);
    assert("filter *", m("readme", "*") === true);
    assert("filter .*", m("readme", ".*") === true);
    assert("*.jpg hit", m("photo.JPG", "*.jpg") === true);
    assert("*.jpg miss", m("photo.png", "*.jpg") === false);
    assert(".png ext", m("a.png", ".png") === true);
    assert("png ext", m("a.png", "png") === true);
    assert("brace hit", m("x.webp", "*.{jpg,png,webp}") === true);
    assert("brace miss", m("x.gif", "*.{jpg,png,webp}") === false);
    assert("prefix", m("image_01.png", "image_*") === true);
    assert("single ?", m("a1.png", "a?.png") === true);
    assert("array filter", m("doc.pdf", ["*.pdf", "*.png"]) === true);
}

function testParseDialogPayload() {
    var p = YUI.__dialogTest__.parseDialogPayload;
    var single = JSON.stringify({
        id: "dlgA",
        type: "View",
        size: [100, 100]
    });
    var wrapped = JSON.stringify({
        root: "dialogLibTestApp",
        children: [{ id: "photoFileDialog", type: "View" }]
    });

    var r1 = p(single, "dialogLibTestApp");
    assert("single layer id", r1 && r1.layer.id === "dlgA");
    assert("single parent", r1 && r1.parentId === "dialogLibTestApp");

    var r2 = p(wrapped, "dialogLibTestApp");
    assert("wrapped layer id", r2 && r2.layer.id === "photoFileDialog");
    assert("wrapped root", r2 && r2.parentId === "dialogLibTestApp");

    assert("invalid json", p("{bad", "x") === null);
    assert("empty object", p("{}", "x") === null);
}

function testFileDialogMount() {
    var ensure = YUI.__dialogTest__.ensureFileDialogLoaded;
    var ok = ensure("dialogLibTestApp");
    assert("ensureFileDialogLoaded", ok === true);
    assert("photoFileDialog", typeof YUI.find === "function" && !!YUI.find("photoFileDialog"));
    assert("photoFileList", typeof YUI.find === "function" && !!YUI.find("photoFileList"));
}

function runUnitTests() {
    resetStats();
    appendLog("--- unit tests ---");
    testApi();
    testNormalizePath();
    testMatchFileFilter();
    testParseDialogPayload();
    testFileDialogMount();
    appendLog("--- done: " + testPass + " pass, " + testFail + " fail ---");
}

function onRunUnitTests() {
    runUnitTests();
}

function onRunAllTests() {
    runUnitTests();
    appendLog("--- manual UI tests: click Alert/Confirm/Prompt/PickFile ---");
}

function onClearLog() {
    testLogs = [];
    YUI.setText("log", "log:");
}

function onTestAlert() {
    YUI.alert("Alert test message", { root: "dialogLibTestApp" }, function () {
        appendLog("alert: closed");
    });
}

function onTestConfirm() {
    YUI.confirm("Confirm this action?", { root: "dialogLibTestApp" }, function (ok) {
        appendLog("confirm: " + (ok ? "ok" : "cancel"));
    });
}

function onTestPrompt() {
    YUI.prompt("Enter filename", "test.txt", { root: "dialogLibTestApp" }, function (name) {
        appendLog("prompt: " + (name === null ? "cancel" : name));
    });
}

function onTestPickFile() {
    YUI.pickFile({
        title: "测试选文件",
        root: "dialogLibTestApp",
        startPath: "app/assets",
        filter: "*.{jpg,png,gif,webp,svg}",
        width: 420,
        height: 420,
        overlayWidth: 520,
        overlayHeight: 560
    }, function (path) {
        appendLog("pickFile: " + (path || "(cancel)"));
    });
}

function onTestMount() {
    testFileDialogMount();
}

function onDialogTestLoad() {
    appendLog("dialog.js test page loaded");
    appendLog("run: ya -r qjs -- app/tests/test-dialog-lib.json");
    if (AUTO_RUN) {
        runUnitTests();
    }
}
