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

function goHome() {
    YUI.hide("page_list");
    YUI.show("page_home");
}

function goList() {
    YUI.hide("page_home");
    YUI.show("page_list");
}

function destroyListPage() {
    YUI.update({
        "page_outlet": {
            "remove": "page_list"
        }
    });
}
