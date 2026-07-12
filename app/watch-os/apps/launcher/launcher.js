/**
 * 应用启动器 - 从 WatchAppRegistry 动态生成图标
 *
 * launcher_grid 使用 Grid 图层，动态 append 子按钮后由引擎自动排布，
 * 并跟随 page_launcher 滚动。
 *
 * 只在首次加载时构建一次网格；返回启动器时不再重建，
 * 避免销毁/重建导致按钮点击事件失效。
 * 安装/卸载应用后调用 rebuildLauncherGrid() 强制重建。
 */

var LAUNCHER_COLS = 4;
var LAUNCHER_BTN = 58;
var LAUNCHER_SPACING = 10;
var launcherGridBuilt = false;

function onLauncherLoad() {
    buildLauncherGrid();
    applyWatchTheme();
}

function onLauncherShow() {
    if (!launcherGridBuilt) {
        buildLauncherGrid();
    }
    applyWatchTheme();
}

function buildLauncherGrid() {
    if (typeof WatchAppRegistry === "undefined") return;

    var apps = WatchAppRegistry.getLauncherApps();
    YUI.setText("launcher_count", apps.length + " 个应用");

    var rows = Math.max(1, Math.ceil(apps.length / LAUNCHER_COLS));
    var gridHeight = rows * LAUNCHER_BTN + (rows - 1) * LAUNCHER_SPACING;

    YUI.update({
        target: "launcher_grid",
        change: { width: 342, height: gridHeight, children: null }
    });

    for (var i = 0; i < apps.length; i++) {
        var app = apps[i];
        var layerId = "launcher_app_" + app.id;
        YUI.renderFromJson("launcher_grid", JSON.stringify({
            id: layerId,
            type: "Button",
            variant: "dock-flat",
            text: app.icon,
            size: [LAUNCHER_BTN, LAUNCHER_BTN],
            events: { onClick: "@onLauncherAppClick" }
        }), true);
        YUI.show(layerId);
    }

    launcherGridBuilt = true;
}

function rebuildLauncherGrid() {
    launcherGridBuilt = false;
    buildLauncherGrid();
}

function onLauncherAppClick(layerId) {
    if (!layerId) return;
    var prefix = "launcher_app_";
    if (layerId.indexOf(prefix) === 0) {
        WatchAppRegistry.openById(layerId.substring(prefix.length));
    }
}
