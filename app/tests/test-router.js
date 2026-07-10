var routes = {
    "/": { layerId: "home", keepAlive: true },
    "/list": { layerId: "list", keepAlive: true },
    "/detail/:id": { layerId: "detail", keepAlive: true },
    "/dynamic/:id": {
        json: "app/tests/pages/detail.json",
        keepAlive: true
    },
    "/admin": { layerId: "list", keepAlive: true }
};

var logs = [];
var isLoggedIn = false;
var ROUTER_SELF_TEST = 0;

function appendLog(msg) {
    logs.push(msg);
    YUI.setText("log", "log: " + logs.join(" -> "));
    YUI.log("[router] " + msg);
}

function updateRouteInfo() {
    if (typeof YUI.currentRoute !== "function") return;
    var route = YUI.currentRoute();
    if (!route) {
        YUI.setText("route_info", "route: (none)");
        return;
    }
    YUI.setText("route_info", "route: " + route.path + "  canBack=" + YUI.canBack());
}

function onAppLoad() {
    Router.init({
        outlet: "page_outlet",
        routes: routes
    });

    YUI.beforeRoute(function(from, to, next) {
        if (to.path === "/admin" && !isLoggedIn) {
            appendLog("guard:block->/list");
            next("/list");
            return;
        }
        next();
    });

    appendLog("app.onLoad");
    updateRouteInfo();

    if (ROUTER_SELF_TEST) {
        runRouterSelfTest();
    }
}

function runRouterSelfTest() {
    appendLog("selfTest:start");
    goList();
    goDetail();
    goBack();
    goAdmin();
    goDynamic();
    goBack();
    goHome();
    appendLog("selfTest:end");
}

function onHomeLoad() { appendLog("home.onLoad"); }
function onHomeShow() { appendLog("home.onShow"); updateRouteInfo(); }
function onHomeHide() { appendLog("home.onHide"); }

function onListLoad() { appendLog("list.onLoad"); }
function onListShow() { appendLog("list.onShow"); updateRouteInfo(); }
function onListHide() { appendLog("list.onHide"); }

function onDetailLoad() { appendLog("detail.onLoad"); }
function onDetailShow() { appendLog("detail.onShow"); updateRouteInfo(); }
function onDetailHide() { appendLog("detail.onHide"); }

function onDetailPageLoad() { appendLog("dynamic.onLoad"); }
function onDetailPageShow() { appendLog("dynamic.onShow"); updateRouteInfo(); }
function onDetailPageHide() { appendLog("dynamic.onHide"); }
function onDetailPageUnload() { appendLog("dynamic.onUnload"); }

function goHome() { YUI.navigate("/"); }
function goList() { YUI.navigate("/list"); }
function goDetail() { YUI.navigate("/detail/42"); }
function goDynamic() { YUI.navigate("/dynamic/99"); }
function goAdmin() { YUI.navigate("/admin"); }
function goBack() { YUI.back(); updateRouteInfo(); }
