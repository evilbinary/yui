/**
 * 应用启动器 - 从 WatchAppRegistry 动态生成图标
 */

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
    YUI.setText("launcher_count", apps.length + " 个应用");

    YUI.update(JSON.stringify({
        target: "launcher_grid",
        change: { children: null }
    }));

    var perRow = 4;
    for (var i = 0; i < apps.length; i += perRow) {
        var rowChildren = [];
        for (var j = i; j < i + perRow && j < apps.length; j++) {
            var app = apps[j];
            rowChildren.push({
                id: "launcher_app_" + app.id,
                type: "Button",
                variant: "dock-flat",
                text: app.icon,
                size: [58, 58],
                events: { onClick: "@onLauncherAppClick" }
            });
        }
        var rowJson = JSON.stringify({
            id: "launcher_row_" + (i / perRow),
            type: "View",
            size: [342, 60],
            layout: { type: "horizontal", spacing: 10, align: "center" },
            children: rowChildren
        });
        YUI.renderFromJson("launcher_grid", rowJson);
    }
}

function onLauncherAppClick(layerId) {
    if (!layerId) return;
    var prefix = "launcher_app_";
    if (layerId.indexOf(prefix) === 0) {
        var appId = layerId.substring(prefix.length);
        if (appId === "face") {
            goWatchBack();
            return;
        }
        WatchAppRegistry.openById(appId);
    }
}
