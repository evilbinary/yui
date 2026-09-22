/**
 * 运行界面
 *
 * 掌机运行模拟器时的 HUD：加载核心进度 → 运行中帧率 / 时间 / 电量 / 时长。
 * 真实接入 libretro 前端时，画面区域替换为前端输出即可。
 */

var PLAYER_BOOT_STAGES = [
    "正在加载 ROM…",
    "初始化模拟核心…",
    "校验存档数据…",
    "启动图形输出…"
];

var playerBootPct = 0;
var playerBootTimer = null;
var playerFpsTimer = null;
var playerPaused = false;
var playerFps = 60;
var playerRecentPushed = false;

function onPlayerLoad() {
    YUI.log("[player] onLoad");
    refreshPlayerUI();
}

function onPlayerShow() {
    applyConsoleTheme();
    applyPlayerDisplayMode();
    refreshPlayerUI();
    restartPlayerBoot();
}

function onPlayerHide() {
    playerStopTimers();
    restorePlayerChrome();
}

/* 全屏模式：隐藏页面信息栏/HUD/按键栏与状态栏，让画面区域占满 */
function applyPlayerDisplayMode() {
    var fullscreen = Console.emulatorRunMode === "fullscreen";
    var ids = ["player_header", "player_hud", "player_keys", "status_bar", "hint_bar"];
    for (var i = 0; i < ids.length; i++) {
        if (fullscreen) {
            YUI.hide(ids[i]);
        } else {
            YUI.show(ids[i]);
        }
    }
}

function restorePlayerChrome() {
    var ids = ["player_header", "player_hud", "player_keys", "status_bar", "hint_bar"];
    for (var i = 0; i < ids.length; i++) {
        YUI.show(ids[i]);
    }
}

function playerStopTimers() {
    if (playerBootTimer !== null) {
        clearTimeout(playerBootTimer);
        playerBootTimer = null;
    }
    if (playerFpsTimer !== null) {
        clearTimeout(playerFpsTimer);
        playerFpsTimer = null;
    }
}

/* ==================== 启动流程 ==================== */

function restartPlayerBoot() {
    playerStopTimers();
    playerBootPct = 0;
    playerPaused = false;
    playerRecentPushed = false;

    if (!Console.running) {
        YUI.setText("player_icon", "🎮");
        YUI.setText("player_title", "无运行中的游戏");
        YUI.setText("player_sub", "请从桌面或游戏库选择一个模拟器");
        YUI.setText("player_boot_icon", "💤");
        YUI.setText("player_boot_text", "等待启动…");
        YUI.setText("player_boot_pct", "0%");
        YUI.setText("player_screen_tip", "选择一个 ROM 后回到此页面");
        YUI.show("player_boot_text");
        YUI.show("player_boot_track");
        YUI.show("player_boot_pct");
        YUI.hide("player_running");
        return;
    }

    var run = Console.running;
    YUI.setText("player_icon", run.emuIcon);
    YUI.setText("player_title", run.rom);
    YUI.setText("player_sub", run.emuTitle + "  ·  核心 " + run.core);
    YUI.setText("player_boot_icon", run.emuIcon);
    YUI.setText("player_boot_text", PLAYER_BOOT_STAGES[0]);
    YUI.setText("player_boot_pct", "0%");
    YUI.setText("player_screen_tip", "正在把 ROM 交给 " + run.core + " 前端…");

    YUI.show("player_boot_icon");
    YUI.show("player_boot_text");
    YUI.show("player_boot_track");
    YUI.show("player_boot_pct");
    YUI.hide("player_running");

    consoleSetBar("player_boot_fill", 0, 10, "#38BDF8");

    playerBootTimer = setTimeout(playerBootStep, 140);
}

function playerBootStep() {
    playerBootTimer = null;
    playerBootPct += 6 + Math.floor(Math.random() * 10);
    if (playerBootPct >= 100) {
        playerBootPct = 100;
    }
    playerApplyBootProgress();
    if (playerBootPct >= 100) {
        playerFinishBoot();
        return;
    }
    playerBootTimer = setTimeout(playerBootStep, 130);
}

function playerApplyBootProgress() {
    var stage = Math.min(PLAYER_BOOT_STAGES.length - 1,
        Math.floor(playerBootPct / (100 / PLAYER_BOOT_STAGES.length)));
    YUI.setText("player_boot_text", PLAYER_BOOT_STAGES[stage]);
    YUI.setText("player_boot_pct", playerBootPct + "%");
    consoleSetBar("player_boot_fill", consoleBarWidth(playerBootPct, 420), 10, "#38BDF8");
}

function playerFinishBoot() {
    YUI.hide("player_boot_icon");
    YUI.hide("player_boot_text");
    YUI.hide("player_boot_track");
    YUI.hide("player_boot_pct");
    YUI.show("player_running");
    YUI.setText("player_screen_tip", "模拟器已接管画面输出 · 快捷键见底部提示");

    if (Console.running && !playerRecentPushed) {
        consolePushRecent(Console.running.emuId, Console.running.rom);
        playerRecentPushed = true;
    }

    setConsoleHint("START 暂停    SELECT 菜单    Ⓑ 退出");
    playerStartFpsLoop();
}

/* ==================== 运行中 HUD ==================== */

function playerStartFpsLoop() {
    if (playerFpsTimer !== null) return;
    playerFpsTimer = setTimeout(function loop() {
        playerTick();
        playerFpsTimer = setTimeout(loop, 1000);
    }, 1000);
}

function playerTick() {
    if (playerPaused) {
        YUI.setText("player_running", "⏸ 已暂停");
        consoleSetBar("player_fps_fill", 0, 6, "#FACC15");
    } else {
        playerFps = 58 + Math.floor(Math.random() * 4);
        YUI.setText("player_running", "▶ 运行中 · " + playerFps + " FPS");
        YUI.setText("player_fps", playerFps + " FPS");
        consoleSetBar("player_fps_fill", consoleBarWidth(playerFps, 124), 6, "#34D399");
    }
    refreshPlayerHUD();
}

function refreshPlayerHUD() {
    YUI.setText("player_hud_time", formatConsoleTime());
    consoleSetBar("player_hud_time_fill", 62, 6, "#38BDF8");

    YUI.setText("player_hud_battery", Console.battery + "%");
    consoleSetBar("player_hud_battery_fill",
        consoleBarWidth(Console.battery, 124), 6, consoleBatteryColor());

    var minutes = 0;
    if (Console.running) {
        minutes = Math.floor((Date.now() - Console.running.startedAt) / 60000);
    }
    YUI.setText("player_hud_play", consolePlaytimeText(minutes));
    consoleSetBar("player_hud_play_fill",
        consoleBarWidth(Math.min(100, minutes * 5), 124), 6, "#A78BFA");
}

function refreshPlayerClock() {
    refreshPlayerHUD();
}

function refreshPlayerUI() {
    if (!Console.running) {
        YUI.setText("player_title", "无运行中的游戏");
        YUI.setText("player_sub", "请选择一个 ROM");
    }
    refreshPlayerHUD();
}

/* ==================== 操作 ==================== */

function onPlayerPause() {
    playerPaused = !playerPaused;
    YUI.setText("btn_player_pause", playerPaused ? "继续" : "暂停");
    playerTick();
}

function onPlayerSave() {
    setConsoleHint("已写入即时存档 slot 1（" + formatConsoleTime() + "）");
}

function onPlayerQuickLoad() {
    setConsoleHint("已从即时存档 slot 1 读取");
}

function onPlayerKeyA() { setConsoleHint("Ⓐ 确认 · 模拟器按键已转发"); }
function onPlayerKeyB() { setConsoleHint("Ⓑ 取消 · 模拟器按键已转发"); }
function onPlayerKeyX() { setConsoleHint("Ⓧ 功能键已转发"); }
function onPlayerKeyY() { setConsoleHint("Ⓨ 功能键已转发"); }

function onPlayerExit() {
    playerStopTimers();
    YUI.setText("btn_player_pause", "暂停");
    openConsolePage("/emulator");
}
