/**
 * 游戏库 - 汇总所有模拟器的游戏，可全局 / 最近 / 按模拟器筛选后启动
 */

var LIBRARY_FILTERS = ["all", "recent", "emu"];

var libraryFilter = "all";
var libraryEmuIndex = -1;

function onLibraryLoad() {
    YUI.log("[library] onLoad");
    refreshLibraryUI();
}

function onLibraryShow() {
    applyConsoleTheme();
    refreshLibraryUI();
}

function libraryCurrentEmuId() {
    if (libraryFilter !== "emu") return null;
    var list = Console.emulators;
    if (list.length === 0 || libraryEmuIndex < 0) return null;
    return list[libraryEmuIndex % list.length].id;
}

function libraryData() {
    var all = EmulatorRegistry.allRoms();
    var out = [];
    var emuId = libraryCurrentEmuId();

    if (libraryFilter === "recent") {
        for (var i = 0; i < Console.recent.length; i++) {
            var entry = Console.recent[i];
            for (var j = 0; j < all.length; j++) {
                if (all[j].title === entry.title && all[j].emu === entry.emu) {
                    out.push(all[j]);
                    break;
                }
            }
        }
        return out;
    }

    for (var k = 0; k < all.length; k++) {
        if (emuId && all[k].emu !== emuId) continue;
        out.push(all[k]);
    }
    return out;
}

function refreshLibraryUI() {
    var emuId = libraryCurrentEmuId();
    var eemu = emuId ? EmulatorRegistry.findById(emuId) : null;

    YUI.setText("library_sub", "共 " + EmulatorRegistry.totalRoms() + " 个游戏 · "
        + EmulatorRegistry.count() + " 个模拟器 · 扫描 " + EmulatorRegistry.root + "/");
    YUI.setText("btn_lib_filter_emu", "筛选：" + (eemu ? eemu.title : "全部"));

    YUI.update([
        { target: "btn_lib_filter_all", change: { variant: libraryFilter === "all" ? "pill" : "secondary" } },
        { target: "btn_lib_filter_recent", change: { variant: libraryFilter === "recent" ? "pill" : "secondary" } },
        { target: "btn_lib_filter_emu", change: { variant: libraryFilter === "emu" ? "pill" : "secondary" } }
    ]);

    var data = libraryData();
    YUI.update({ target: "library_list", change: { data: data } });
    YUI.setText("library_stats", data.length + " 个游戏");

    if (data.length === 0) {
        YUI.setText("library_filter_hint", "无匹配游戏，请放入 ROM");
    } else if (libraryFilter === "recent") {
        YUI.setText("library_filter_hint", "仅显示最近玩过的游戏");
    } else {
        YUI.setText("library_filter_hint", "点击条目即加载并启动");
    }

    setConsoleHint("Ⓐ 启动游戏    Ⓑ 返回桌面    右滑返回");
}

/* ==================== 筛选 ==================== */

function onLibraryFilterAll() {
    libraryFilter = "all";
    refreshLibraryUI();
}

function onLibraryFilterRecent() {
    libraryFilter = "recent";
    refreshLibraryUI();
}

/* 轮换模拟器筛选：全部核心 → 各模拟器 → 回到全部核心 */
function onLibraryCycleEmulator() {
    var total = Console.emulators.length;
    if (total === 0) {
        YUI.setText("library_filter_hint", "尚未加载任何模拟器");
        return;
    }

    if (libraryFilter !== "emu") {
        libraryFilter = "emu";
        libraryEmuIndex = 0;
    } else {
        libraryEmuIndex += 1;
        if (libraryEmuIndex >= total) {
            libraryFilter = "all";
            libraryEmuIndex = -1;
        }
    }
    refreshLibraryUI();
}

function onLibraryRescan() {
    rescanConsoleEmulators();
    libraryEmuIndex = -1;
    if (libraryFilter === "emu") libraryFilter = "all";
    refreshLibraryUI();
}

/* ==================== 启动 ==================== */

function onLibraryItemSelect(layerId) {
    var raw = YUI.getText("library_list");
    if (!raw) return;
    var item;
    try {
        item = JSON.parse(raw);
    } catch (e) {
        YUI.log("[library] bad item json");
        return;
    }
    if (!item || !item.emu || !item.title) return;

    YUI.log("[library] launch " + item.emu + " / " + item.title);
    consoleSelectEmulator(item.emu);
    if (!consoleLaunchRom(item.emu, item.title)) return;
    openConsolePage("/player");
}
