/**
 * Component Gallery — 组件平铺 + 主题切换 + YUI.perf
 * 运行: make gallery
 *   或: ya -r playground -- app/gallery/app.json
 */

var GALLERY_THEMES = {
    mocha: { label: "Mocha", path: "app/lib/themes/mocha.json", buttonId: "btnThemeMocha" },
    dark: { label: "Dark", path: "app/lib/themes/dark.json", buttonId: "btnThemeDark" },
    latte: { label: "Latte", path: "app/lib/themes/latte.json", buttonId: "btnThemeLatte" },
    "element-plus": { label: "Element Plus", path: "app/lib/themes/element-plus.json", buttonId: "btnThemeEp" },
    "element-plus-dark": { label: "EP Dark", path: "app/lib/themes/element-plus-dark.json", buttonId: "btnThemeEpDark" },
    "soft-ui": { label: "Soft UI", path: "app/lib/themes/soft-ui.json", buttonId: "btnThemeSoft" },
    "soft-ui-dark": { label: "Soft UI Dark", path: "app/lib/themes/soft-ui-dark.json", buttonId: "btnThemeSoftDark" },
    "developer-terminal": { label: "Terminal", path: "app/lib/themes/developer-terminal.json", buttonId: "btnThemeTerminal" }
};

var currentThemeId = "developer-terminal";

var g_perf = {
    overlayOn: false,
    pollTimer: null,
    lastStatsText: ""
};

function setStatus(msg) {
    var el = yui.find("galleryStatus");
    if (el) el.text = msg;
}

function setPerfStatus(msg) {
    var el = yui.find("galleryPerfStatus");
    if (el) el.text = "状态：" + msg;
}

function setPerfStats(msg) {
    if (msg === g_perf.lastStatsText) return;
    g_perf.lastStatsText = msg;
    var el = yui.find("galleryPerfStats");
    if (el) el.text = msg;
}

function setBtnText(id, text) {
    var el = yui.find(id);
    if (el) el.text = text;
}

function hasPerfApi() {
    return typeof YUI.perf !== "undefined" &&
        typeof YUI.perf.enable === "function" &&
        typeof YUI.perf.getFrameStats === "function" &&
        typeof YUI.perf.getLayerStats === "function";
}

function layerStatsLength(arr) {
    if (!arr) return 0;
    var i = 0;
    while (arr[String(i)] !== undefined && arr[String(i)] !== null) i++;
    return i;
}

function layerStatsToArray(arr) {
    var out = [];
    var i;
    var n = layerStatsLength(arr);
    for (i = 0; i < n; i++) out.push(arr[String(i)]);
    return out;
}

function formatFrameStats(frame) {
    if (!frame) return "frame: (null)";
    return "fps=" + frame.fps +
        " frameMs=" + frame.frameMs +
        " renderMs=" + frame.renderMs +
        " layers=" + frame.layerCount +
        " #" + frame.frameIndex;
}

function formatTopLayers(arr, maxLines) {
    var lines = [];
    var i;
    var n = layerStatsLength(arr);
    var limit = maxLines || 4;
    if (n > limit) n = limit;
    for (i = 0; i < n; i++) {
        var item = arr[String(i)];
        if (!item) continue;
        var name = item.id ? item.id : ("type:" + item.type);
        lines.push((i + 1) + ". " + name + " " + item.renderMs + "ms x" + item.renderCount);
    }
    return lines.length ? lines.join(" | ") : "(empty)";
}

function enablePerfDefaults() {
    YUI.perf.enable();
    YUI.perf.setTopN(8);
    YUI.perf.setOverlay(g_perf.overlayOn);
    YUI.perf.clearWatch();
    YUI.perf.watch("galleryRoot");
    YUI.perf.watch("galleryContent");
    YUI.perf.watch("secPerf");
    YUI.perf.watch("secButtons");
    YUI.perf.watch("secTable");
    YUI.perf.setLogInterval(0);
}

function refreshPerfStats() {
    if (!hasPerfApi()) {
        setPerfStatus("YUI.perf API 不可用");
        return null;
    }
    var frame = YUI.perf.getFrameStats();
    var byTime = YUI.perf.getLayerStats("time");
    var text = formatFrameStats(frame) + "\nTop: " + formatTopLayers(byTime, 4);
    setPerfStats(text);
    return { frame: frame, byTime: byTime };
}

function onPerfToggleHud() {
    if (!hasPerfApi()) {
        setPerfStatus("YUI.perf API 不可用");
        return;
    }
    g_perf.overlayOn = !g_perf.overlayOn;
    YUI.perf.setOverlay(g_perf.overlayOn);
    setBtnText("btnPerfHud", g_perf.overlayOn ? "HUD 开" : "HUD 关");
    setPerfStatus("HUD " + (g_perf.overlayOn ? "开启" : "关闭"));
    setStatus("perf HUD: " + (g_perf.overlayOn ? "开" : "关"));
}

function onPerfRefresh() {
    var result = refreshPerfStats();
    if (result) {
        setPerfStatus("已刷新 #" + (result.frame && result.frame.frameIndex));
        setStatus("perf 已刷新");
    }
}

function stopPerfPoll() {
    if (g_perf.pollTimer) {
        clearInterval(g_perf.pollTimer);
        g_perf.pollTimer = null;
    }
    setBtnText("btnPerfPoll", "自动刷新");
}

function onPerfTogglePoll() {
    if (!hasPerfApi()) {
        setPerfStatus("YUI.perf API 不可用");
        return;
    }
    if (g_perf.pollTimer) {
        stopPerfPoll();
        setPerfStatus("自动刷新已停止");
        setStatus("perf 自动刷新关");
        return;
    }
    enablePerfDefaults();
    refreshPerfStats();
    g_perf.pollTimer = setInterval(function () {
        refreshPerfStats();
    }, 500);
    setBtnText("btnPerfPoll", "停止刷新");
    setPerfStatus("每 500ms 自动刷新");
    setStatus("perf 自动刷新开");
}

function onPerfExport() {
    if (!hasPerfApi()) {
        setPerfStatus("YUI.perf API 不可用");
        return;
    }
    if (typeof YUI.writeFile !== "function") {
        setPerfStatus("YUI.writeFile 不可用");
        return;
    }
    var frame = YUI.perf.getFrameStats();
    var suffix = frame && frame.frameIndex ? ("-f" + frame.frameIndex) : "";
    var path = "gallery-perf" + suffix + ".json";
    var payload = {
        app: "gallery",
        theme: currentThemeId,
        frame: frame,
        layersByTime: layerStatsToArray(YUI.perf.getLayerStats("time")),
        layersByCount: layerStatsToArray(YUI.perf.getLayerStats("count")),
        overlayOn: g_perf.overlayOn,
        exportedAt: typeof Date !== "undefined" ? new Date().toISOString() : ""
    };
    var ok = YUI.writeFile(path, JSON.stringify(payload, null, 2));
    if (ok) {
        setPerfStatus("已导出 " + path);
        setStatus("perf 导出: " + path);
        YUI.log("gallery perf export: " + path);
    } else {
        setPerfStatus("导出失败 " + path);
    }
}

function onPerfReset() {
    if (!hasPerfApi()) return;
    YUI.perf.reset();
    refreshPerfStats();
    setPerfStatus("统计已重置");
    setStatus("perf reset");
}

function onPerfDisable() {
    if (!hasPerfApi()) return;
    stopPerfPoll();
    YUI.perf.disable();
    g_perf.overlayOn = false;
    setBtnText("btnPerfHud", "HUD 关");
    setPerfStatus("perf 已关闭");
    setPerfStats("统计：已禁用");
    setStatus("perf 已关闭");
}

function updateThemeButtonSelection(themeId) {
    var id;
    for (id in GALLERY_THEMES) {
        if (!GALLERY_THEMES.hasOwnProperty(id)) continue;
        var btn = yui.find(GALLERY_THEMES[id].buttonId);
        if (!btn) continue;
        btn.variant = (id === themeId) ? "primary" : "compact";
    }
}

function applyGalleryTheme(themeId) {
    var theme = GALLERY_THEMES[themeId];
    if (!theme) {
        setStatus("未知主题: " + themeId);
        return;
    }
    if (typeof YUI.themeLoad !== "function") {
        setStatus("主题 API 不可用");
        return;
    }
    var loaded = YUI.themeLoad(theme.path);
    if (!loaded || loaded.success === false) {
        setStatus("加载失败: " + theme.path);
        return;
    }
    var name = loaded.name || themeId;
    currentThemeId = themeId;
    YUI.themeSetCurrent(name);
    /* 先改选中态 variant，再 apply，primary 才会落在当前主题按钮上 */
    updateThemeButtonSelection(themeId);
    YUI.themeApplyToTree();
    setStatus("当前主题：" + theme.label + " (" + name + ")");
}

function onThemeMocha() { applyGalleryTheme("mocha"); }
function onThemeDark() { applyGalleryTheme("dark"); }
function onThemeLatte() { applyGalleryTheme("latte"); }
function onThemeElementPlus() { applyGalleryTheme("element-plus"); }
function onThemeElementPlusDark() { applyGalleryTheme("element-plus-dark"); }
function onThemeSoftUi() { applyGalleryTheme("soft-ui"); }
function onThemeSoftUiDark() { applyGalleryTheme("soft-ui-dark"); }
function onThemeTerminal() { applyGalleryTheme("developer-terminal"); }

function onGalleryLog(msg) {
    setStatus(String(msg || "clicked"));
}

function onShowInfoDialog() {
    YUI.show("galleryInfoDialog");
}

function onGalleryPagerChange() {
    setStatus("分页已切换");
}

function onGallerySelect() {
    setStatus("Table / Tree 选中");
}

function onGalleryReady() {
    applyGalleryTheme(currentThemeId);
    if (!hasPerfApi()) {
        setPerfStatus("YUI.perf 未注册");
        setPerfStats("统计：API 不可用");
        return;
    }
    enablePerfDefaults();
    setBtnText("btnPerfHud", g_perf.overlayOn ? "HUD 开" : "HUD 关");
    setPerfStatus("已启用，watch: galleryRoot / galleryContent / secButtons…");
    refreshPerfStats();
}

onGalleryReady();
