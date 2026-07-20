/**
 * 设置 App
 */

function onSettingsLoad() {
    syncSettingsUI();
}

function onSettingsShow() {
    YUI.setText("settings_battery", Watch.battery + "%");
    syncSettingsUI();
    applyWatchTheme();
}

function syncSettingsUI() {
    var mode = getWatchLauncherMode();
    var isBubble = mode === "bubble";
    YUI.setText("settings_theme_val", "当前：" + getWatchThemeLabel());
    YUI.setText("settings_launcher_val", "当前：" + getWatchLauncherModeLabel(mode));
    YUI.update([
        { target: "btn_launcher_bubble", change: { variant: isBubble ? "dock-accent" : "pill" } },
        { target: "btn_launcher_grid", change: { variant: isBubble ? "pill" : "dock-accent" } }
    ]);
}

function toggleWatchTheme() {
    Watch.themeMode = Watch.themeMode === "dark" ? "light" : "dark";
    applyWatchTheme();
    syncSettingsUI();
}

function setWatchLauncherBubble() {
    setWatchLauncherMode("bubble");
    syncSettingsUI();
}

function setWatchLauncherGrid() {
    setWatchLauncherMode("grid");
    syncSettingsUI();
}
