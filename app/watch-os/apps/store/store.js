/**
 * 应用商店 - 从 store/packages 安装到 installed/
 */

var storeCatalog = [];

function onStoreLoad() {
    loadStoreCatalog();
    refreshStoreUI();
}

function onStoreShow() {
    applyWatchTheme();
    loadStoreCatalog();
    refreshStoreUI();
}

function loadStoreCatalog() {
    storeCatalog = [];
    var raw = YUI.readFile("app/watch-os/store/catalog.json");
    if (!raw) return;
    try {
        var data = JSON.parse(raw);
        storeCatalog = data.packages || [];
    } catch (e) {
        YUI.log("store: bad catalog");
    }
}

function refreshStoreUI() {
    YUI.setText("store_summary", storeCatalog.length + " 个可安装应用");

    for (var i = 0; i < 2; i++) {
        if (i < storeCatalog.length) {
            var pkg = storeCatalog[i];
            var installed = WatchAppRegistry.isInstalled(pkg.id);
            YUI.setText("store_pkg_" + i + "_icon", pkg.icon);
            YUI.setText("store_pkg_" + i + "_title", pkg.title);
            YUI.setText("store_pkg_" + i + "_desc", pkg.description || "");
            YUI.setText("btn_store_install_" + i, installed ? "已装" : "安装");
        }
    }
}

function installStorePackage(index) {
    var pkg = storeCatalog[index];
    if (!pkg) return;

    if (WatchAppRegistry.findById(pkg.id) && WatchAppRegistry.findById(pkg.id).installed) {
        YUI.setText("store_status", pkg.title + " 已安装");
        return;
    }

    var ok = WatchAppRegistry.installPackage(pkg.source, pkg.id);
    if (ok) {
        YUI.setText("store_status", pkg.title + " 安装成功");
        rescanWatchApps();
        refreshStoreUI();
    } else {
        YUI.setText("store_status", pkg.title + " 安装失败");
    }
}

function installStorePkg0() { installStorePackage(0); }
function installStorePkg1() { installStorePackage(1); }

function rescanWatchApps() {
    WatchAppRegistry.init();
    if (typeof Router !== "undefined") {
        Router.routes = WatchAppRegistry.getRoutes();
    }
    Watch.apps = WatchAppRegistry.getAll();
    if (typeof buildLauncherGrid === "function") {
        buildLauncherGrid();
    }
    if (typeof refreshFaceDock === "function") {
        refreshFaceDock();
    }
    YUI.setText("store_status", "已刷新，共 " + Watch.apps.length + " 个应用");
}
