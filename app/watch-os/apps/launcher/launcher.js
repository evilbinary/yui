/**
 * 应用启动器 - 从 WatchAppRegistry 动态生成图标
 */

var launcherSlotApps = [];

function onLauncherLoad() {
    buildLauncherGrid();
    applyWatchTheme();
}

function onLauncherShow() {
    buildLauncherGrid();
    applyWatchTheme();
}

function buildLauncherGrid() {
    if (typeof WatchAppRegistry === "undefined") return;

    var apps = WatchAppRegistry.getLauncherApps();
    launcherSlotApps = apps;
    YUI.setText("launcher_count", apps.length + " 个应用");

    // 使用静态槽位，让页面滚动由引擎布局接管。
    for (var i = 0; i < 24; i++) {
        var slotId = "launcher_app_slot_" + i;
        if (i >= apps.length) {
            YUI.hide(slotId);
            continue;
        }
        var app = apps[i];
        YUI.setText(slotId, app.icon);
        YUI.show(slotId);
    }
}

function onLauncherAppClick(layerId) {
    if (!layerId) return;
    var prefix = "launcher_app_slot_";
    if (layerId.indexOf(prefix) === 0) {
        var slotIndex = parseInt(layerId.substring(prefix.length), 10);
        var app = launcherSlotApps[slotIndex];
        if (!app) return;
        var appId = app.id;
        if (appId === "face") {
            goWatchBack();
            return;
        }
        WatchAppRegistry.openById(appId);
    }
}
