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
    YUI.setText("settings_theme_val", "当前：" + getWatchThemeLabel());
}

function toggleWatchTheme() {
    Watch.themeMode = Watch.themeMode === "dark" ? "light" : "dark";
    applyWatchTheme();
    syncSettingsUI();
}
