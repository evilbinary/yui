/**
 * 桌面
 *
 * - 顶部：时间 / 日期 / 电量 / 存储 / 音量（常用信息）
 * - 中部：模拟器图标网格（来自 EmulatorRegistry 扫描结果，可动态加载）
 * - 底部：最近游玩
 */

var DESKTOP_TILE_W = 92;
var DESKTOP_TILE_H = 78;
var DESKTOP_TILE_GAP = 8;
var DESKTOP_TILE_ICON = 46;
var DESKTOP_COLUMNS = 6;
var DESKTOP_GRID_WIDTH = 592;

var desktopBuilt = false;
var desktopTileLockMs = 0;

function onDesktopLoad() {
    YUI.log("[desktop] onLoad");
    rebuildDesktopGrid();
    refreshDesktopHero();
    refreshDesktopRecent();
}

function onDesktopShow() {
    applyConsoleTheme();
    if (!desktopBuilt || Console.emulators.length === 0) {
        rebuildDesktopGrid();
    }
    refreshDesktopHero();
    refreshDesktopRecent();
}

/* ==================== 图标网格 ==================== */

function rebuildDesktopGrid() {
    var list = Console.emulators;
    if (!list || list.length === 0) {
        list = EmulatorRegistry.getAll();
    }

    var rows = Math.max(1, Math.ceil(list.length / DESKTOP_COLUMNS));
    var gridHeight = rows * DESKTOP_TILE_H + (rows - 1) * DESKTOP_TILE_GAP;

    YUI.update({
        target: "emu_grid",
        change: { width: DESKTOP_GRID_WIDTH, height: gridHeight, children: null }
    });

    for (var i = 0; i < list.length; i++) {
        var emu = list[i];
        var tileId = "emu_tile_" + emu.id;
        var romCount = EmulatorRegistry.romCount(emu.id);
        var active = (emu.id === Console.selectedEmulator);

        YUI.renderFromJson("emu_grid", JSON.stringify({
            id: tileId,
            type: "View",
            variant: "tile",
            focusable: true,
            size: [DESKTOP_TILE_W, DESKTOP_TILE_H],
            layout: {
                type: "vertical",
                spacing: 2,
                padding: [6, 4, 4, 4],
                align: "center",
                justifyContent: "center"
            },
            events: { onClick: "@onDesktopTileClick" },
            children: [
                {
                    id: "emu_icon_" + emu.id,
                    type: "Button",
                    variant: active ? "tile-icon-active" : "tile-icon",
                    text: emu.icon,
                    size: [DESKTOP_TILE_ICON, DESKTOP_TILE_ICON],
                    events: { onClick: "@onDesktopTileClick" }
                },
                {
                    id: "emu_name_" + emu.id,
                    type: "Label",
                    variant: "tile-title",
                    /* 磁贴只有 84 宽（Label 溢出判据 rect.w-10），长名改用简称避免触发 tooltip */
                    text: emu.title.length <= 6 ? emu.title : emu.short,
                    size: [84, 13],
                    textAlign: "center"
                },
                {
                    id: "emu_roms_" + emu.id,
                    type: "Label",
                    variant: "stat-label",
                    text: romCount + " 个游戏",
                    size: [84, 10],
                    textAlign: "center"
                }
            ]
        }), true);

        YUI.show(tileId);
    }

    desktopBuilt = true;
    applyConsoleTheme();

    YUI.setText("desktop_emu_count", list.length + " 个已加载 · "
        + EmulatorRegistry.totalRoms() + " 个游戏 · 扫描自 "
        + EmulatorRegistry.root + "/");
}

/* 点击模拟器图标 → 选中并进入详情（图标与磁贴可能同时命中，做去重） */
function onDesktopTileClick(layerId) {
    if (!layerId) return;
    var id = desktopEmuIdFromLayer(layerId);
    if (!id) return;

    var now = Date.now();
    if (now - desktopTileLockMs < 220) return;
    desktopTileLockMs = now;

    YUI.log("[desktop] tile click " + id);
    consoleOpenEmulator(id);
    refreshDesktopGridSelection();
}

function desktopEmuIdFromLayer(layerId) {
    var prefixes = ["emu_tile_", "emu_icon_", "emu_name_", "emu_roms_"];
    for (var i = 0; i < prefixes.length; i++) {
        var p = prefixes[i];
        if (layerId.indexOf(p) === 0) {
            return layerId.substring(p.length);
        }
    }
    return null;
}

/* 切换高亮（不重建图层，避免闪烁） */
function refreshDesktopGridSelection() {
    if (!desktopBuilt) return;
    var updates = [];
    for (var i = 0; i < Console.emulators.length; i++) {
        var emu = Console.emulators[i];
        updates.push({
            target: "emu_icon_" + emu.id,
            change: { variant: emu.id === Console.selectedEmulator ? "tile-icon-active" : "tile-icon" }
        });
    }
    if (updates.length > 0) {
        YUI.update(updates);
    }
}

/* ==================== 常用信息 ==================== */

function refreshDesktopHero() {
    YUI.setText("hero_time", formatConsoleTime());
    YUI.setText("hero_date", consoleFullDateText());

    if (Console.running) {
        YUI.setText("hero_sub", "运行中 · " + consoleShortText(Console.running.emuTitle, 10));
    } else {
        YUI.setText("hero_sub", "YUI GAME OS v" + Console.version
            + " · " + Console.emulators.length + " 个模拟器");
    }

    YUI.setText("hero_battery_icon", consoleBatteryIcon());
    YUI.setText("hero_battery_value", Console.battery + "%");
    YUI.setText("hero_battery_label", Console.charging
        ? "充电中 · 约 25 分钟"
        : "剩余约 " + (Math.round(Console.battery * 0.07 * 10) / 10) + " 小时");
    consoleSetBar("hero_battery_fill", consoleBarWidth(Console.battery, 102), 6, consoleBatteryColor());

    YUI.setText("hero_storage_value", consoleStorageFree() + "G");
    YUI.setText("hero_storage_label", "可用 / 共 " + Console.storage.total + "G");
    consoleSetBar("hero_storage_fill", consoleBarWidth(consoleStoragePercent(), 102), 6, "#38BDF8");

    YUI.setText("hero_system_icon", Console.volume <= 0 ? "🔇" : "🔊");
    YUI.setText("hero_system_value", Console.volume + "%");
    YUI.setText("hero_system_label", "亮度 " + Console.brightness + "% · " + Console.temperature + "°C");
    consoleSetBar("hero_system_fill", consoleBarWidth(Console.volume, 102), 6, "#FACC15");
}

/* ==================== 最近游玩 ==================== */

function refreshDesktopRecent() {
    for (var i = 0; i < 3; i++) {
        var card = "recent_card_" + i;
        var entry = Console.recent[i];
        if (!entry) {
            YUI.setText("recent_" + i + "_tag", "—");
            YUI.setText("recent_" + i + "_title", "暂无记录");
            YUI.setText("recent_" + i + "_meta", "启动任意游戏后会出现在这里");
            YUI.hide(card);
            continue;
        }
        YUI.show(card);
        YUI.setText("recent_" + i + "_tag", entry.icon + " " + entry.when);
        YUI.setText("recent_" + i + "_title", consoleShortText(entry.title, 8));
        YUI.setText("recent_" + i + "_meta", consoleShortText(entry.meta, 12));
    }
    YUI.show("desktop_recent");
}

function onRecentCardClick(layerId) {
    if (!layerId) return;
    var idx = -1;
    var m = layerId.match(/recent_(\d+)/);
    if (m) idx = parseInt(m[1], 10);

    var entry = Console.recent[idx >= 0 && idx < 3 ? idx : 0];
    if (!entry) return;

    if (!consoleSelectEmulator(entry.emu)) return;
    consoleLaunchRom(entry.emu, entry.title);
    openConsolePage("/player");
}

function onDesktopClearRecent() {
    resetConsoleRecent();
    refreshDesktopRecent();
    setConsoleHint("已清空最近游玩记录");
}

/* ==================== 扫描 ==================== */

function onDesktopRescan() {
    var n = rescanConsoleEmulators();
    refreshDesktopHero();
    setConsoleHint("已重新扫描 " + EmulatorRegistry.root + "/ · 加载 " + n + " 个模拟器");
}
