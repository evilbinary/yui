/**
 * YUI Demo Launcher
 * Dynamically scans app/ and app/tests/ directories for demo JSON pages.
 */

var DemoMap = {};

var GRID_COLS = 5;
var GRID_SPACING = 8;
var CELL_SIZE = 140;
var GRID_WIDTH = GRID_COLS * CELL_SIZE + (GRID_COLS - 1) * GRID_SPACING;

function onLauncherLoad() {
    var apps = scanApps();
    var tests = scanTests();
    buildGrid("grid_apps", apps);
    buildGrid("grid_tests", tests);
    var total = apps.length + tests.length;
    YUI.setText("launcher_title", "YUI Demo Launcher (" + total + ")");
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
    try { json = JSON.parse(raw); } catch (e) { return null; }
    if (!json.type && !json.children && !json.js) return null;
    var title = json.title || json.text || json.id || fallback;
    var id = json.id || title;
    var icon = json.icon || null;
    return { id: String(id), title: String(title), icon: icon, jsonPath: path, _source: json.source || null };
}

// ---------- Grid UI ----------

function buildGrid(gridId, demos) {
    if (demos.length === 0) return;
    var rows = Math.ceil(demos.length / GRID_COLS);
    var gridH = rows * CELL_SIZE + (rows - 1) * GRID_SPACING;
    YUI.update({ target: gridId, change: { width: GRID_WIDTH, height: gridH, children: null } });
    for (var i = 0; i < demos.length; i++) {
        var d = demos[i];
        var key = "dm_" + i + "_" + d._cat;
        DemoMap[key] = d;
        YUI.renderFromJson(gridId, JSON.stringify({
            id: key,
            type: "Button",
            size: [CELL_SIZE, CELL_SIZE],
            style: { bgColor: "#2a2a3a", borderRadius: 8 },
            icon: d.icon || d.title.charAt(0).toUpperCase(),
            text: d.title,
            iconAlign: "center",
            iconSize: 42,
            iconGap:18,
            fontSize: 18,
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
    YUI.hide("launcher_content");
    YUI.show("btn_back");

    // renderFromJson creates layers hidden; extract JSON root id to show it
    var pageId = null;
    try { pageId = JSON.parse(raw).id; } catch (e) {}
    YUI.renderFromJson("page_outlet", raw, false, demo.jsonPath);
    if (pageId) YUI.show(pageId);
    YUI.show("page_outlet");
    YUI.setText("launcher_title", demo.title);
}

function onBackClick() {
    YUI.update({ target: "page_outlet", change: { children: null } });
    YUI.hide("page_outlet");
    YUI.hide("btn_back");
    YUI.show("launcher_content");
    var n = 0;
    for (var k in DemoMap) { if (DemoMap.hasOwnProperty(k)) n++; }
    YUI.setText("launcher_title", "YUI Demo Launcher (" + n + ")");
}
