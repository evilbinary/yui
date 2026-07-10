var lifecycleLog = [];

function appendLog(msg) {
    lifecycleLog.push(msg);
    YUI.setText("log", "log: " + lifecycleLog.join(" -> "));
    YUI.log("[lifecycle] " + msg);
}

function onAppLoad() {
    appendLog("app.onLoad");
}

function onHomeLoad() { appendLog("home.onLoad"); }
function onHomeShow() { appendLog("home.onShow"); }
function onHomeHide() { appendLog("home.onHide"); }
function onHomeUnload() { appendLog("home.onUnload"); }

function onListLoad() { appendLog("list.onLoad"); }
function onListShow() { appendLog("list.onShow"); }
function onListHide() { appendLog("list.onHide"); }
function onListUnload() { appendLog("list.onUnload"); }

function onDetailPageLoad() { appendLog("detail.onLoad"); }
function onDetailPageShow() { appendLog("detail.onShow"); }
function onDetailPageHide() { appendLog("detail.onHide"); }
function onDetailPageUnload() { appendLog("detail.onUnload"); }

function findLayer(layerId) {
    if (typeof YUI.find === "function") return YUI.find(layerId);
    if (typeof yui !== "undefined" && typeof yui.find === "function") return yui.find(layerId);
    return null;
}

// 从 pages/xxx.json 动态挂载到 outlet；已存在则 append=true 复用 keepAlive
function loadPageJson(jsonPath, layerId) {
    if (typeof YUI.readFile !== "function") {
        appendLog("readFile missing");
        return false;
    }

    var jsonStr = YUI.readFile(jsonPath);
    if (!jsonStr) {
        appendLog("read fail:" + jsonPath);
        return false;
    }

    var append = !!findLayer(layerId);
    var code = YUI.renderFromJson("page_outlet", jsonStr, append);
    if (code !== 0) {
        appendLog("render fail:" + layerId);
        return false;
    }
    return true;
}

function showPage(layerId) {
    YUI.hide("page_home");
    YUI.hide("page_list");
    YUI.hide("dynamic_detail");
    YUI.show(layerId);
}

function goHome() {
    YUI.hide("page_list");
    YUI.hide("dynamic_detail");
    YUI.show("page_home");
}

function goList() {
    if (!loadPageJson("app/tests/pages/list.json", "page_list")) return;
    showPage("page_list");
}

function goDetail() {
    if (!loadPageJson("app/tests/pages/detail.json", "dynamic_detail")) return;
    showPage("dynamic_detail");
}

function destroyListPage() {
    YUI.update({
        "page_outlet": {
            "remove": "page_list"
        }
    });
}
