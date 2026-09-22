/**
 * 系统设置：外观 / 音量 / 亮度 / 模拟器加载 / 最近记录
 */

function onSettingsLoad() {
    YUI.log("[settings] onLoad");
    refreshSettingsUI();
}

function onSettingsShow() {
    applyConsoleTheme();
    refreshSettingsUI();
}

function refreshSettingsUI() {
    YUI.setText("settings_theme_value", "当前：" + getConsoleThemeLabel()
        + "（" + Console.themeMode + ".json）");

    YUI.setText("settings_runmode_value", "当前：" + getConsoleRunModeLabel()
        + "（" + Console.emulatorRunMode + "）");

    YUI.setText("settings_volume_value", Console.volume + "%");
    consoleSetBar("settings_volume_fill", consoleBarWidth(Console.volume, 120), 6, "#38BDF8");

    YUI.setText("settings_brightness_value", Console.brightness + "%");
    consoleSetBar("settings_brightness_fill", consoleBarWidth(Console.brightness, 120), 6, "#FACC15");

    YUI.setText("settings_emulator_value",
        EmulatorRegistry.root + "/ · " + EmulatorRegistry.count() + " 个模拟器 / "
        + EmulatorRegistry.totalRoms() + " 个游戏（来源：" + EmulatorRegistry.source + "）");

    YUI.setText("settings_recent_value", Console.recent.length + " 条记录");

    YUI.setText("settings_about_status",
        "已加载 " + EmulatorRegistry.count() + " 个模拟器 · 电量 " + Console.battery + "% · "
        + Console.wifi + " · 主题 " + getConsoleThemeLabel());
}

/* ==================== 外观 ==================== */

function onSettingsToggleTheme() {
    var mode = switchConsoleTheme();
    refreshSettingsUI();
    setConsoleHint("主题已切换为" + (mode === "dark" ? "暗色" : "亮色"));
}

/* 模拟器运行方式：外部进程 → 全屏运行 → 页面内嵌 循环 */
function onSettingsCycleRunMode() {
    var mode = cycleConsoleRunMode();
    refreshSettingsUI();
    setConsoleHint("模拟器运行方式：" + getConsoleRunModeLabel() + "（" + mode + "）");
}

/* ==================== 音量 / 亮度 ==================== */

function onSettingsVolumeUp() {
    Console.volume = Math.min(100, Console.volume + 10);
    saveConsoleConfig();
    updateConsoleChrome();
    refreshSettingsUI();
    if (typeof refreshDesktopHero === "function") refreshDesktopHero();
}

function onSettingsVolumeDown() {
    Console.volume = Math.max(0, Console.volume - 10);
    saveConsoleConfig();
    updateConsoleChrome();
    refreshSettingsUI();
    if (typeof refreshDesktopHero === "function") refreshDesktopHero();
}

function onSettingsBrightnessUp() {
    Console.brightness = Math.min(100, Console.brightness + 10);
    saveConsoleConfig();
    refreshSettingsUI();
    if (typeof refreshDesktopHero === "function") refreshDesktopHero();
}

function onSettingsBrightnessDown() {
    Console.brightness = Math.max(10, Console.brightness - 10);
    saveConsoleConfig();
    refreshSettingsUI();
    if (typeof refreshDesktopHero === "function") refreshDesktopHero();
}

/* ==================== 模拟器 / 最近记录 ==================== */

function onSettingsRescan() {
    var n = rescanConsoleEmulators();
    refreshSettingsUI();
    setConsoleHint("已重新扫描 " + EmulatorRegistry.root + "/ · " + n + " 个模拟器");
}

function onSettingsClearRecent() {
    resetConsoleRecent();
    refreshSettingsUI();
    setConsoleHint("最近游玩记录已清空");
}
