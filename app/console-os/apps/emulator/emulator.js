/**
 * 模拟器详情
 *
 * 展示当前选中模拟器的元数据（核心 / 格式 / ROM 目录）与游戏列表；
 * 点击列表项即“加载模拟器并启动该 ROM”，进入运行界面。
 */

function onEmulatorLoad() {
    YUI.log("[emulator] onLoad");
    refreshEmulatorUI();
}

function onEmulatorShow() {
    applyConsoleTheme();
    refreshEmulatorUI();
}

function refreshEmulatorUI() {
    var emu = consoleSelectedEmulator();
    if (!emu) {
        YUI.setText("emu_hero_title", "未发现模拟器");
        YUI.setText("emu_hero_sub", "请在 " + EmulatorRegistry.root + "/ 下放入模拟器目录");
        YUI.update({ target: "emu_rom_list", change: { data: [] } });
        return;
    }

    var roms = EmulatorRegistry.listRoms(emu.id);

    YUI.setText("emu_hero_icon", emu.icon);
    YUI.setText("emu_hero_title", emu.title + "  ·  " + emu.short);
    YUI.setText("emu_hero_sub", (emu.subtitle || emu.id) + "  ·  核心 " + EmulatorRegistry.coreText(emu));
    YUI.setText("emu_list_bio", emu.bio || "");

    YUI.setText("emu_stat_games", String(roms.length));
    YUI.setText("emu_stat_core", EmulatorRegistry.coreText(emu));
    YUI.setText("emu_stat_exts", EmulatorRegistry.extsText(emu));
    YUI.setText("emu_stat_romdir", emu.romDir);

    consoleSetBar("emu_stat_games_fill",
        consoleBarWidth(Math.min(100, roms.length * 8), 124), 6, "#34D399");

    YUI.setText("emu_list_count", EmulatorRegistry.systemsText(emu) + " · " + roms.length + " 个游戏");
    YUI.update({ target: "emu_rom_list", change: { data: roms } });

    setConsoleHint("Ⓐ 启动 " + emu.title + "    Ⓑ 返回桌面    右滑返回");
}

/* 列表选中：item JSON 由 list 组件写入图层 text */
function onEmulatorRomSelect(layerId) {
    var raw = YUI.getText("emu_rom_list");
    if (!raw) return;
    var item;
    try {
        item = JSON.parse(raw);
    } catch (e) {
        YUI.log("[emulator] bad item json");
        return;
    }
    if (!item || !item.title) return;

    var emu = consoleSelectedEmulator();
    if (!emu) return;

    YUI.log("[emulator] select " + emu.id + " / " + item.title);
    launchEmulatorRom(emu.id, item.title);
}

function onEmulatorLaunchFirst() {
    var emu = consoleSelectedEmulator();
    if (!emu) return;
    var roms = EmulatorRegistry.listRoms(emu.id);
    if (roms.length === 0) {
        YUI.setText("emu_list_count", "目录 " + emu.romDir + " 为空，请放入 ROM 文件");
        return;
    }
    launchEmulatorRom(emu.id, roms[0].title);
}

/* 加载模拟器：记录运行状态 → 写入最近记录 → 进入运行界面 */
function launchEmulatorRom(emuId, romTitle) {
    if (!consoleLaunchRom(emuId, romTitle)) return;

    var route = YUI.currentRoute ? YUI.currentRoute() : null;
    if (route && route.path === "/player") {
        if (typeof restartPlayerBoot === "function") restartPlayerBoot();
        return;
    }
    openConsolePage("/player");
}
