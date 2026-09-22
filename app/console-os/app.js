/**
 * YUI Game OS - 壳层
 *
 * 职责：路由 / 主题 / 状态栏（时间、电量、音量、存储、WiFi）/ 共享系统数据
 * 桌面与子页面通过 Console 对象读取共享状态。
 */

var Console = {
    version: "0.1.0",
    themeMode: "dark",
    /* 模拟器运行方式：external(外部进程,默认) / fullscreen(全屏页面) / embedded(页面内嵌) */
    emulatorRunMode: "external",

    /* 系统状态（桌面常用信息） */
    battery: 85,
    charging: false,
    voltage: 3.92,
    temperature: 38,
    cycles: 213,
    volume: 70,
    brightness: 80,
    wifi: "HOME-5G",
    wifiLevel: 3,
    storage: { total: 64, used: 22 },

    /* 模拟器与运行状态 */
    emulators: [],
    selectedEmulator: "nes",
    running: null,

    /* 最近游玩 */
    recent: []
};

var CONSOLE_THEME_DIR = "themes";

/* 配置文件：优先用包内路径，避免写进程 CWD（老版本 watch-os 会误写 ./config.json） */
var CONSOLE_CONFIG_PATH = "app/console-os/config.json";
var CONSOLE_CONFIG_PATH_FALLBACK = "config.json";
var consoleConfigPathInUse = null;

function consoleConfigPath() {
    if (consoleConfigPathInUse) return consoleConfigPathInUse;
    if (typeof YUI.readFile === "function" && YUI.readFile(CONSOLE_CONFIG_PATH)) {
        consoleConfigPathInUse = CONSOLE_CONFIG_PATH;
    } else {
        consoleConfigPathInUse = CONSOLE_CONFIG_PATH_FALLBACK;
    }
    return consoleConfigPathInUse;
}

var consoleClockTimer = null;
var consoleBatteryTimer = null;
var consoleSwipeLock = false;
var consoleTileLockMs = 0;

var CONSOLE_HINTS = {
    "/": "Ⓐ 打开模拟器    Ⓑ 返回    Ⓧ 游戏库",
    "/library": "Ⓐ 启动游戏    Ⓑ 返回桌面",
    "/emulator": "Ⓐ 启动游戏    Ⓑ 返回桌面",
    "/player": "START 暂停    SELECT 菜单    Ⓑ 退出",
    "/battery": "Ⓑ 返回桌面",
    "/settings": "Ⓑ 返回桌面"
};

/* ==================== 配置读写 ==================== */

function loadConsoleConfig() {
    var cfg = {};
    if (typeof YUI.readFile !== "function") {
        return cfg;
    }
    var raw = YUI.readFile(consoleConfigPath());
    if (!raw) {
        return cfg;
    }
    try {
        cfg = JSON.parse(raw) || {};
    } catch (e) {
        YUI.log("[console] config parse error: " + e);
        cfg = {};
    }
    return cfg;
}

function saveConsoleConfig() {
    if (typeof YUI.writeFile !== "function") {
        return false;
    }
    var cfg = {
        themeMode: Console.themeMode,
        emulatorRunMode: Console.emulatorRunMode,
        gridColumns: 6,
        volume: Console.volume,
        brightness: Console.brightness,
        wifi: Console.wifi,
        recent: Console.recent
    };
    return YUI.writeFile(consoleConfigPath(), JSON.stringify(cfg, null, 2));
}

function applyConsoleConfig(cfg) {
    if (!cfg) return;
    if (cfg.themeMode === "light" || cfg.themeMode === "dark") {
        Console.themeMode = cfg.themeMode;
    }
    if (cfg.emulatorRunMode === "external" || cfg.emulatorRunMode === "fullscreen" ||
        cfg.emulatorRunMode === "embedded") {
        Console.emulatorRunMode = cfg.emulatorRunMode;
    }
    if (typeof cfg.volume === "number") Console.volume = cfg.volume;
    if (typeof cfg.brightness === "number") Console.brightness = cfg.brightness;
    if (cfg.wifi) Console.wifi = cfg.wifi;
    if (cfg.recent && cfg.recent.length !== undefined && cfg.recent.length > 0) {
        Console.recent = cfg.recent;
    }
}

/* ==================== 主题 ==================== */

function ensureConsoleTheme(mode) {
    mode = mode || Console.themeMode;
    if (typeof Theme.isLoaded === "function" && Theme.isLoaded(mode)) {
        return true;
    }
    var old = ThemeManager && ThemeManager.currentTheme;
    Theme.load(CONSOLE_THEME_DIR + "/" + mode + ".json", mode);
    var loaded = typeof Theme.isLoaded === "function" && Theme.isLoaded(mode);
    if (loaded && old && old !== mode && typeof Theme.unload === "function") {
        Theme.unload(old);
    }
    return loaded;
}

function applyConsoleTheme() {
    ensureConsoleTheme(Console.themeMode);
    Theme.setCurrent(Console.themeMode);
    Theme.apply();
}

function switchConsoleTheme() {
    Console.themeMode = Console.themeMode === "dark" ? "light" : "dark";
    applyConsoleTheme();
    saveConsoleConfig();
    updateConsoleChrome();
    return Console.themeMode;
}

function getConsoleThemeLabel() {
    return Console.themeMode === "dark" ? "暗色" : "亮色";
}

/* ==================== 模拟器运行方式 ==================== */

function getConsoleRunModeLabel() {
    if (Console.emulatorRunMode === "embedded") return "页面内嵌";
    if (Console.emulatorRunMode === "fullscreen") return "全屏运行";
    return "外部进程";
}

function cycleConsoleRunMode() {
    if (Console.emulatorRunMode === "external") {
        Console.emulatorRunMode = "fullscreen";
    } else if (Console.emulatorRunMode === "fullscreen") {
        Console.emulatorRunMode = "embedded";
    } else {
        Console.emulatorRunMode = "external";
    }
    saveConsoleConfig();
    return Console.emulatorRunMode;
}

/* 按设置启动模拟器：外部进程成功返回 "external"；不可用/失败回退 "fullscreen" */
function consoleRunEmulator(emuId, romPath) {
    var mode = Console.emulatorRunMode || "external";
    if (mode !== "external") {
        return mode;
    }
    var emu = EmulatorRegistry.findById(emuId);
    var cmd = emu ? (emu.exec || emu.core || "") : "";
    if (!cmd || typeof YUI.spawn !== "function") {
        return "fullscreen";
    }
    if (romPath) {
        cmd = cmd + " " + consoleShellQuote(romPath);
    }
    YUI.log("[console] spawn external: " + cmd);
    var rc = YUI.spawn(cmd);
    if (rc === 0) {
        return "external";
    }
    YUI.log("[console] spawn failed rc=" + rc + ", fallback fullscreen");
    return "fullscreen";
}

/* 启动游戏：记录运行状态，按模式决定外部进程或进入播放页 */
function consoleLaunchAndRun(emuId, romTitle, romPath) {
    if (!consoleLaunchRom(emuId, romTitle, romPath)) return;

    var mode = consoleRunEmulator(emuId, romPath);
    if (mode === "external") {
        setConsoleHint("已启动外部模拟器进程 · " + romTitle);
        return;
    }

    var route = YUI.currentRoute ? YUI.currentRoute() : null;
    if (route && route.path === "/player") {
        if (typeof restartPlayerBoot === "function") restartPlayerBoot();
        return;
    }
    openConsolePage("/player");
}

/* ==================== 时间与日期 ==================== */

function consolePad2(n) {
    return (n < 10 ? "0" : "") + n;
}

/* mquickjs 无 new Date()，用 Date.now() 自行换算 */
function consoleNow() {
    var totalMin = Math.floor(Date.now() / 60000);
    return {
        h: Math.floor(totalMin / 60) % 24,
        m: totalMin % 60,
        totalMin: totalMin
    };
}

function formatConsoleTime(now) {
    now = now || consoleNow();
    return consolePad2(now.h) + ":" + consolePad2(now.m);
}

var CONSOLE_WEEKDAYS = ["周日", "周一", "周二", "周三", "周四", "周五", "周六"];
var CONSOLE_MONTHS = ["1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12"];

/* epoch 天数 → 公历年月日（Howard Hinnant civil_from_days） */
function consoleCivilFromDays(days) {
    var z = days + 719468;
    var era = Math.floor(z / 146097);
    var doe = z - era * 146097;
    var yoe = Math.floor((doe - Math.floor(doe / 1460) + Math.floor(doe / 36524) - Math.floor(doe / 146096)) / 365);
    var y = yoe + era * 400;
    var doy = doe - (365 * yoe + Math.floor(yoe / 4) - Math.floor(yoe / 100));
    var mp = Math.floor((5 * doy + 2) / 153);
    var d = doy - Math.floor((153 * mp + 2) / 5) + 1;
    var m = mp + (mp < 10 ? 3 : -9);
    if (m <= 2) y = y + 1;
    return { y: y, m: m, d: d };
}

function consoleDate() {
    var days = Math.floor(Date.now() / 86400000);
    var civil = consoleCivilFromDays(days);
    var weekday = (days + 4) % 7;
    if (weekday < 0) weekday += 7;
    return {
        y: civil.y,
        m: civil.m,
        d: civil.d,
        weekday: weekday,
        weekdayText: CONSOLE_WEEKDAYS[weekday]
    };
}

function consoleFullDateText() {
    var dt = consoleDate();
    return dt.y + "年" + CONSOLE_MONTHS[dt.m - 1] + "月" + dt.d + "日 " + dt.weekdayText;
}

function consoleMonthDayText() {
    var dt = consoleDate();
    return CONSOLE_MONTHS[dt.m - 1] + "/" + dt.d + " " + dt.weekdayText;
}

function formatConsoleNumber(n) {
    return String(n).replace(/\B(?=(\d{3})+(?!\d))/g, ",");
}

/* Label 宽度超 rect.w-10 会被截断成 "…" 并在悬停时弹出 tooltip，这里先按字数收敛 */
function consoleShortText(text, max) {
    if (!text) return "";
    if (text.length <= max) return text;
    return text.substring(0, max) + "…";
}

/* ==================== 电量 / 存储 / 常用信息 ==================== */

function consoleBatteryIcon() {
    if (Console.charging) return "⚡";
    if (Console.battery <= 15) return "🪫";
    return "🔋";
}

function consoleBatteryText() {
    return consoleBatteryIcon() + " " + Console.battery + "%";
}

function consoleBatteryColor() {
    if (Console.charging) return "#38BDF8";
    if (Console.battery <= 15) return "#FF6B6B";
    if (Console.battery <= 35) return "#FACC15";
    return "#34D399";
}

function consoleStorageFree() {
    return Math.max(0, Console.storage.total - Console.storage.used);
}

function consoleStorageText() {
    return "💾 " + Console.storage.used + "/" + Console.storage.total + "G";
}

function consoleStoragePercent() {
    if (Console.storage.total <= 0) return 0;
    var pct = Math.round((Console.storage.used / Console.storage.total) * 100);
    if (pct > 100) pct = 100;
    if (pct < 0) pct = 0;
    return pct;
}

function consoleWifiText() {
    return "📶 " + Console.wifi;
}

function consoleVolumeText() {
    if (Console.volume <= 0) return "🔇 静音";
    return "🔊 " + Console.volume + "%";
}

function consolePlaytimeText(minutes) {
    if (minutes < 60) return minutes + " 分钟";
    var h = Math.floor(minutes / 60);
    var m = minutes % 60;
    return h + " 小时" + (m > 0 ? " " + m + " 分" : "");
}

/* 进度条：Progress 组件不支持运行时更新，用 View 填充层 + size 更新代替 */
function consoleBarWidth(percent, trackWidth) {
    var p = percent;
    if (p < 0) p = 0;
    if (p > 100) p = 100;
    var w = Math.round(trackWidth * p / 100);
    if (w < 0) w = 0;
    return w;
}

function consoleSetBar(fillId, width, height, color) {
    if (width < 0) width = 0;
    if (height < 0) height = 0;
    var change = { size: [width, height] };
    if (color) change.bgColor = color;
    YUI.update({ target: fillId, change: change });
}

/* ==================== 时钟 ==================== */

function startConsoleClock() {
    if (consoleClockTimer !== null) return;
    tickConsoleClock();
    consoleClockTimer = setTimeout(function loop() {
        tickConsoleClock();
        consoleClockTimer = setTimeout(loop, 20000);
    }, 20000);
}

function tickConsoleClock() {
    YUI.setText("status_time", formatConsoleTime());

    var route = YUI.currentRoute ? YUI.currentRoute() : null;
    var path = route ? route.path : "/";

    if (path === "/") {
        YUI.setText("hero_time", formatConsoleTime());
        YUI.setText("hero_date", consoleFullDateText());
    } else if (path === "/player") {
        if (typeof refreshPlayerClock === "function") refreshPlayerClock();
    } else if (path === "/battery") {
        if (typeof refreshBatteryUI === "function") refreshBatteryUI();
    }
}

/* 电量缓慢变化，模拟掌机耗电/充电 */
function startConsoleBatteryLoop() {
    if (consoleBatteryTimer !== null) return;
    consoleBatteryTimer = setTimeout(function loop() {
        if (Console.charging) {
            if (Console.battery < 100) Console.battery += 1;
            Console.voltage = 4.12;
        } else if (Console.battery > 4) {
            Console.battery -= 1;
            Console.voltage = 3.6 + Console.battery * 0.005;
        }
        Console.temperature = 34 + (Console.battery % 7);
        updateConsoleChrome();
        var route = YUI.currentRoute ? YUI.currentRoute() : null;
        if (route && route.path === "/battery" && typeof refreshBatteryUI === "function") {
            refreshBatteryUI();
        }
        consoleBatteryTimer = setTimeout(loop, 45000);
    }, 45000);
}

/* ==================== 状态栏 / 提示栏 ==================== */

function consoleHintForPath(path) {
    var hint = CONSOLE_HINTS[path];
    return hint ? hint : "Ⓑ 返回桌面";
}

function updateConsoleChrome() {
    YUI.setText("status_time", formatConsoleTime());
    YUI.setText("status_battery", consoleBatteryText());
    YUI.setText("status_volume", consoleVolumeText());
    YUI.setText("status_storage", consoleStorageText());
    YUI.setText("status_wifi", consoleWifiText());

    var route = YUI.currentRoute ? YUI.currentRoute() : null;
    var path = route ? route.path : "/";
    YUI.setText("hint_text", consoleHintForPath(path));
    YUI.setText("hint_count", Console.emulators.length + " 个模拟器 · "
        + EmulatorRegistry.totalRoms() + " 个游戏");
}

function setConsoleHint(text) {
    if (text) YUI.setText("hint_text", text);
}

/* ==================== 路由与导航 ==================== */

function openConsolePage(path) {
    if (typeof Router === "undefined") return false;
    YUI.navigate(path);
    updateConsoleChrome();
    return true;
}

function openDesktopPage() { openConsolePage("/"); }
function openLibraryPage() { openConsolePage("/library"); }
function openEmulatorPage() { openConsolePage("/emulator"); }
function openPlayerPage() { openConsolePage("/player"); }
function openBatteryPage() { openConsolePage("/battery"); }
function openSettingsPage() { openConsolePage("/settings"); }

function consoleIsShellPath(path) {
    return path === "/";
}

function goConsoleBack() {
    var route = YUI.currentRoute ? YUI.currentRoute() : null;
    var path = route ? route.path : "/";
    YUI.log("[console] back from " + path);

    if (path === "/") {
        updateConsoleChrome();
        return;
    }
    if (typeof Router !== "undefined" && Router.canBack()) {
        YUI.back();
    } else {
        YUI.navigate("/");
    }
    updateConsoleChrome();
    applyConsoleTheme();
}

/* 应用级按键：Esc/手柄 B 返回 */
function onConsoleKey(layerId) {
    var code = (typeof YUI.keyCode === "function") ? YUI.keyCode() : 0;
    if (code === 27) {
        goConsoleBack();
    }
}

function onConsoleTouch(layerId, event) {
    var type = event ? event.type : null;
    if (type !== "swipe") return;
    if (consoleSwipeLock) return;

    var route = YUI.currentRoute ? YUI.currentRoute() : null;
    var path = route ? route.path : "/";
    var direction = event.deltaX < 0 ? "left" : "right";
    YUI.log("[console] swipe " + direction + " at " + path);

    if (path !== "/" && direction === "right") {
        consoleSwipeLock = true;
        goConsoleBack();
        setTimeout(function() { consoleSwipeLock = false; }, 380);
        return;
    }
    if (path === "/" && direction === "left") {
        consoleSwipeLock = true;
        openLibraryPage();
        setTimeout(function() { consoleSwipeLock = false; }, 380);
    }
}

/* ==================== 模拟器 ==================== */

function rescanConsoleEmulators() {
    EmulatorRegistry.init();
    Console.emulators = EmulatorRegistry.getAll();
    updateConsoleChrome();
    if (typeof rebuildDesktopGrid === "function") rebuildDesktopGrid();
    return Console.emulators.length;
}

function consoleSelectEmulator(id) {
    if (!id) return false;
    if (!EmulatorRegistry.findById(id)) {
        YUI.log("[console] unknown emulator " + id);
        return false;
    }
    Console.selectedEmulator = id;
    return true;
}

function consoleOpenEmulator(id) {
    if (!consoleSelectEmulator(id)) return false;
    openConsolePage("/emulator");
    return true;
}

function consoleSelectedEmulator() {
    var emu = EmulatorRegistry.findById(Console.selectedEmulator);
    if (emu) return emu;
    var all = EmulatorRegistry.getAll();
    if (all.length > 0) {
        Console.selectedEmulator = all[0].id;
        return all[0];
    }
    return null;
}

function consoleLaunchRom(emuId, romTitle, romPath) {
    var emu = EmulatorRegistry.findById(emuId);
    if (!emu) return false;
    Console.running = {
        emuId: emu.id,
        emuTitle: emu.title,
        emuIcon: emu.icon,
        core: emu.core,
        rom: romTitle,
        romPath: romPath || "",
        startedAt: Date.now()
    };
    YUI.log("[console] launch " + emu.title + " / " + romTitle
        + (romPath ? (" <" + romPath + ">") : ""));
    return true;
}

/* shell 参数引用：空格/特殊字符安全 */
function consoleShellQuote(s) {
    return "\"" + String(s).replace(/"/g, "\\\"") + "\"";
}

function consolePushRecent(emuId, romTitle) {
    var emu = EmulatorRegistry.findById(emuId);
    if (!emu) return;
    var entry = {
        emu: emu.id,
        icon: emu.icon,
        title: romTitle,
        meta: emu.short + " · 本次新增",
        when: "刚刚"
    };
    var list = [];
    list.push(entry);
    for (var i = 0; i < Console.recent.length; i++) {
        if (Console.recent[i].title === romTitle) continue;
        if (list.length >= 3) break;
        list.push(Console.recent[i]);
    }
    Console.recent = list;
    saveConsoleConfig();
}

function resetConsoleRecent() {
    Console.recent = [];
    saveConsoleConfig();
    if (typeof refreshDesktopRecent === "function") refreshDesktopRecent();
}

/* ==================== 生命周期 ==================== */

function initConsoleApps() {
    EmulatorRegistry.init();
    Console.emulators = EmulatorRegistry.getAll();
    YUI.log("[console] emulators: " + Console.emulators.length
        + ", roms: " + EmulatorRegistry.totalRoms());
}

function onConsoleLoad() {
    YUI.log("[console] onConsoleLoad");
    applyConsoleConfig(loadConsoleConfig());
    YUI.log("[console] config: " + consoleConfigPath()
        + " (recent " + Console.recent.length + ", theme " + Console.themeMode + ")");
    applyConsoleTheme();
    initConsoleApps();

    Router.init({
        outlet: "page_outlet",
        routes: consoleRoutes()
    });

    YUI.navigate("/");
    updateConsoleChrome();
    startConsoleClock();
    startConsoleBatteryLoop();
}

function consoleRoutes() {
    return {
        "/": { json: "app/console-os/apps/desktop/desktop.json", keepAlive: true },
        "/library": { json: "app/console-os/apps/library/library.json", keepAlive: true },
        "/emulator": { json: "app/console-os/apps/emulator/emulator.json", keepAlive: true },
        "/player": { json: "app/console-os/apps/player/player.json", keepAlive: true },
        "/battery": { json: "app/console-os/apps/battery/battery.json", keepAlive: true },
        "/settings": { json: "app/console-os/apps/settings/settings.json", keepAlive: true }
    };
}

function onConsoleShow() {
    updateConsoleChrome();
    tickConsoleClock();
}
