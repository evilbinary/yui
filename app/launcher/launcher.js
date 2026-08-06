/**
 * YUI Demo Launcher
 * Dynamically scans app/ and app/tests/ directories for demo JSON pages.
 */

var DemoMap = {};
var currentAppId = null;

var GRID_SPACING = 8;
var CELL_MIN = 80;
var CELL_MAX = 140;

var THEMES = [
    { id: "developer-terminal", path: "app/lib/themes/developer-terminal.json" },
    { id: "dark",               path: "app/lib/themes/dark.json" },
    { id: "light",              path: "app/lib/themes/light.json" },
    { id: "mocha",              path: "app/lib/themes/mocha.json" },
];

var perfVisible = false;

function onLauncherLoad() {
    initThemes();
    var apps = scanApps();
    var tests = scanTests();
    buildGrid("grid_apps", apps);
    buildGrid("grid_tests", tests);
    var total = apps.length + tests.length;
    YUI.setText("launcher_title", "YUI Demo Launcher (" + total + ")");
}

// ---------- Theming ----------

function initThemes() {
    for (var i = 0; i < THEMES.length; i++) {
        Theme.load(THEMES[i].path);
    }
    Theme.setCurrent("developer-terminal");
    Theme.apply();
}

function onThemeSelect() {
    var val = YUI.getProperty("theme_select", "value");
    if (!val) return;
    Theme.setCurrent(val);
    Theme.apply();
}

function onPerfToggle() {
    perfVisible = !perfVisible;
    YUI.perf.enable();
    YUI.perf.setOverlay(perfVisible);
    YUI.setText("btn_perf", perfVisible ? "Perf: ON" : "Perf: OFF");
}

// ---------- Scanning ----------

function scanApps() {
    var list = [];
    var entries = YUI.listDir("app");
    if (!entries) return list;
    for (var i = 0; i < entries.length; i++) {
        var e = entries[i];
        if (!e.isDir) continue;
        var name = e.name;
        if (name.charAt(0) === ".") continue;
        if (name === "launcher" || name === "assets" || name === "lib" ||
            name === "js" || name === "__pycache__" || name === "lvgl") continue;
        var demo = resolveApp(name);
        if (demo) { demo._cat = "apps"; list.push(demo); }
    }
    return list;
}

function resolveApp(dir) {
    var base = "app/" + dir;
    var meta = readMeta(base + "/app.json", dir);
    if (meta) {
        if (meta._source) {
            var m2 = readMeta(meta._source, dir);
            if (m2) {
                if (!m2.icon) m2.icon = meta.icon;
                return m2;
            }
        }
        return meta;
    }
    meta = readMeta(base + "/" + dir + ".json", dir);
    if (meta) return meta;
    var sub = YUI.listDir(base);
    if (sub) {
        for (var i = 0; i < sub.length; i++) {
            if (sub[i].isDir) continue;
            var fn = sub[i].name;
            if (fn.indexOf(".json") < 0) continue;
            meta = readMeta(base + "/" + fn, dir);
            if (meta) return meta;
        }
    }
    return null;
}

function scanTests() {
    var list = [];
    var entries = YUI.listDir("app/tests");
    if (!entries) return list;
    for (var i = 0; i < entries.length; i++) {
        var e = entries[i];
        if (e.isDir) continue;
        var fn = e.name;
        if (fn.indexOf(".json") < 0) continue;
        var meta = readMeta("app/tests/" + fn, fn);
        if (meta) { meta._cat = "tests"; list.push(meta); }
    }
    return list;
}

function readMeta(path, fallback) {
    var raw;
    try { raw = YUI.readFile(path); } catch (e) { return null; }
    if (!raw) return null;
    var json;
    try { json = JSON.parse(raw); } catch (err) { return null; }
    if (!json.type && !json.children && !json.js) return null;
    var title = json.title || json.text || json.id || fallback;
    var id = json.id || title;
    var icon = json.icon || null;
    return { id: String(id), title: String(title), icon: icon, jsonPath: path, _source: json.source || null };
}
// ---------- Grid UI ----------

function buildGrid(gridId, demos) {
    if (demos.length === 0) return;
    var win = YUI.getWindowSize();
    var contentW = YUI.getProperty("launcher_content", "width") || win.width || 600;
    YUI.log('contentW = ' + contentW);
    var pad = 24;
    var availW = Math.max(200, contentW - pad);
    var cols = Math.max(2, Math.min(8, Math.floor(availW / CELL_MIN)));
    var cell = Math.floor((availW - GRID_SPACING * (cols - 1)) / cols);
    cell = Math.min(cell, CELL_MAX);
    var gridW = cols * cell + (cols - 1) * GRID_SPACING;
    var rows = Math.ceil(demos.length / cols);
    var gridH = rows * cell + (rows - 1) * GRID_SPACING;
    YUI.update({ target: gridId, change: { width: gridW, height: gridH, children: null } });
    for (var i = 0; i < demos.length; i++) {
        var d = demos[i];
        var key = "dm_" + i + "_" + d._cat;
        DemoMap[key] = d;
        YUI.renderFromJson(gridId, JSON.stringify({
            id: key,
            type: "Button",
            size: [cell, cell],
            style: { bgColor: "#2a2a3a", borderRadius: 8 },
            icon: d.icon || d.title.charAt(0).toUpperCase(),
            text: d.title,
            iconAlign: "center",
            iconSize: Math.floor(cell * 0.3),
            iconGap: Math.floor(cell * 0.12),
            fontSize: Math.floor(cell * 0.13),
            events: { onClick: "@onDemoClick" }
        }), true);
        YUI.show(key);
    }
}

// ---------- Event Handlers ----------

function onDemoClick(layerId) {
    var demo = DemoMap[layerId];
    if (!demo) return;
    var raw;
    try { raw = YUI.readFile(demo.jsonPath); } catch (e) { return; }
    if (!raw) return;
    var json;
    try { json = JSON.parse(raw); } catch (err) { return; }
    var appId = json.id || "page_outlet";
    YUI.hide("launcher_content");
    YUI.show("btn_back");
    YUI.renderFromJson("page_outlet", raw, false, demo.jsonPath);
    YUI.show("page_outlet");
    YUI.show(appId);
    currentAppId = appId;
    YUI.setText("launcher_title", demo.title);
}

function onBackClick() {
    if (typeof _gameIsRunning !== "undefined") _gameIsRunning = false;
    // 恢复之前的主题
    if (typeof _prevThemeName !== "undefined" && _prevThemeName && typeof Theme !== "undefined") {
        Theme.setCurrent(_prevThemeName);
        Theme.apply();
    }
    if (currentAppId) YUI.hide(currentAppId);
    YUI.update({ target: "page_outlet", change: { children: null } });
    YUI.hide("page_outlet");
    YUI.hide("btn_back");
    YUI.show("launcher_content");
    var n = 0;
    for (var k in DemoMap) { if (DemoMap.hasOwnProperty(k)) n++; }
    YUI.setText("launcher_title", "YUI Demo Launcher (" + n + ")");
}
