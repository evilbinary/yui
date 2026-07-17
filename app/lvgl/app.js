/**
 * LVGL demo — layer events + YUI.perf 性能检测
 */

function onDemoLabelClick() {
    YUI.log("LvLabel clicked: demo_label");
}

function onDemoSliderChange() {
    YUI.log("LvSlider value changed");
}

function onDemoSwitchChange() {
    YUI.log("LvSwitch toggled");
}

var g_overlayOn = true;
var g_stressOn = false;
var g_stressTimer = 0;
var g_pulseCount = 0;
var g_lastStatsText = "";

function hasPerfApi() {
    return typeof YUI.perf !== "undefined" &&
        typeof YUI.perf.enable === "function" &&
        typeof YUI.perf.getFrameStats === "function" &&
        typeof YUI.perf.getLayerStats === "function";
}

function setPerfStatus(text, color) {
    var change = { text: text };
    if (color) {
        change.color = color;
    }
    YUI.update({ target: "perf_status_label", change: change });
}

function setPerfStats(text) {
    if (text === g_lastStatsText) {
        return;
    }
    g_lastStatsText = text;
    YUI.update({
        target: "perf_stats_label",
        change: { text: text }
    });
}

function layerStatsLength(arr) {
    if (!arr) {
        return 0;
    }
    var i = 0;
    while (arr[String(i)] !== undefined && arr[String(i)] !== null) {
        i++;
    }
    return i;
}

function findLayerStat(arr, id) {
    var i;
    var n = layerStatsLength(arr);
    for (i = 0; i < n; i++) {
        var item = arr[String(i)];
        if (item && item.id === id) {
            return item;
        }
    }
    return null;
}

function layerStatsToArray(arr) {
    var out = [];
    var i;
    var n = layerStatsLength(arr);
    for (i = 0; i < n; i++) {
        out.push(arr[String(i)]);
    }
    return out;
}

function formatFrameStats(frame) {
    if (!frame) {
        return "frame: (null)";
    }
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
    if (n > limit) {
        n = limit;
    }
    for (i = 0; i < n; i++) {
        var item = arr[String(i)];
        if (!item) {
            continue;
        }
        var name = item.id ? item.id : ("type:" + item.type);
        lines.push((i + 1) + ". " + name + " " + item.renderMs + "ms x" + item.renderCount);
    }
    return lines.join("\n");
}

function formatCompactStats(frame, byTime) {
    var top = "";
    var item = byTime && byTime["0"];
    if (item && item.id) {
        top = " slowest=" + item.id + "(" + item.renderMs + "ms)";
    }
    return formatFrameStats(frame) + top;
}

function enablePerfDefaults() {
    YUI.perf.enable();
    YUI.perf.setTopN(8);
    YUI.perf.setOverlay(g_overlayOn);
    YUI.perf.watch("lvgl_demo");
    YUI.perf.watch("demo_label");
    YUI.perf.watch("demo_calendar");
    YUI.perf.setLogInterval(0);
}

function refreshPerfStats() {
    if (!hasPerfApi()) {
        setPerfStatus("YUI.perf API 不可用", "#e94560");
        return null;
    }

    var frame = YUI.perf.getFrameStats();
    var byTime = YUI.perf.getLayerStats("time");
    var byCount = YUI.perf.getLayerStats("count");

    setPerfStats(formatCompactStats(frame, byTime));

    var detail = formatFrameStats(frame) + "\n── by time ──\n" + formatTopLayers(byTime, 5);
    detail += "\n── by count ──\n" + formatTopLayers(byCount, 3);
    YUI.log("lvgl perf:\n" + detail);
    return { frame: frame, byTime: byTime, byCount: byCount };
}

function togglePerfOverlay() {
    if (!hasPerfApi()) {
        setPerfStatus("YUI.perf API 不可用", "#e94560");
        return;
    }
    g_overlayOn = !g_overlayOn;
    YUI.perf.setOverlay(g_overlayOn);
    YUI.update({
        target: "togglePerfOverlayBtn",
        change: { text: g_overlayOn ? "HUD 开" : "HUD 关" }
    });
    setPerfStatus("HUD: " + (g_overlayOn ? "开启" : "关闭"), "#4a90d9");
}

function pulseDemoLabel() {
    g_pulseCount++;
    YUI.update({
        target: "perf_pulse_label",
        change: { text: "脉冲：" + g_pulseCount }
    });
    YUI.update({
        target: "demo_label",
        change: { text: "LvLabel: pulse @" + g_pulseCount }
    });
}

function stopPerfStress() {
    g_stressOn = false;
    if (g_stressTimer) {
        clearTimeout(g_stressTimer);
        g_stressTimer = 0;
    }
    YUI.update({
        target: "stressBtn",
        change: { text: "开始压测", bgColor: "#e94560" }
    });
    setPerfStatus("压测已停止", "#a0a0b0");
}

function stressTick() {
    if (!g_stressOn) {
        return;
    }
    pulseDemoLabel();
    g_stressTimer = setTimeout(stressTick, 80);
}

function togglePerfStress() {
    if (!hasPerfApi()) {
        setPerfStatus("YUI.perf API 不可用", "#e94560");
        return;
    }
    if (g_stressOn) {
        stopPerfStress();
        refreshPerfStats();
        return;
    }
    g_stressOn = true;
    YUI.update({
        target: "stressBtn",
        change: { text: "停止压测", bgColor: "#c73650" }
    });
    setPerfStatus("压测中：每 80ms 更新 demo_label", "#f5a623");
    stressTick();
}

function buildPerfExportPayload() {
    return {
        app: "lvgl_demo",
        frame: YUI.perf.getFrameStats(),
        layersByTime: layerStatsToArray(YUI.perf.getLayerStats("time")),
        layersByCount: layerStatsToArray(YUI.perf.getLayerStats("count")),
        layersByName: layerStatsToArray(YUI.perf.getLayerStats("name")),
        stressOn: g_stressOn,
        overlayOn: g_overlayOn,
        pulseCount: g_pulseCount,
        exportedAt: typeof Date !== "undefined" ? new Date().toISOString() : ""
    };
}

function exportPerfStats() {
    if (!hasPerfApi()) {
        setPerfStatus("YUI.perf API 不可用", "#e94560");
        return;
    }
    if (typeof YUI.writeFile !== "function") {
        setPerfStatus("YUI.writeFile 不可用", "#e94560");
        return;
    }

    var frame = YUI.perf.getFrameStats();
    var suffix = frame && frame.frameIndex ? ("-f" + frame.frameIndex) : "";
    var path = "lvgl-perf" + suffix + ".json";
    var ok = YUI.writeFile(path, JSON.stringify(buildPerfExportPayload(), null, 2));

    if (ok) {
        setPerfStatus("已导出：" + path, "#0f9b8e");
        YUI.log("lvgl perf export: " + path);
    } else {
        setPerfStatus("导出失败：" + path, "#e94560");
    }
}

function disablePerf() {
    if (!hasPerfApi()) {
        return;
    }
    stopPerfStress();
    YUI.perf.disable();
    g_overlayOn = false;
    YUI.update({
        target: "togglePerfOverlayBtn",
        change: { text: "HUD 关" }
    });
    setPerfStatus("perf 已关闭", "#666680");
    setPerfStats("perf 已禁用");
}

function assertPerfAuto(frame, byTime) {
    var errors = [];

    if (!frame) {
        errors.push("getFrameStats 返回空");
    } else {
        if (!(frame.layerCount > 0)) {
            errors.push("layerCount 应 > 0");
        }
        if (!(frame.frameIndex > 0)) {
            errors.push("frameIndex 应 > 0");
        }
    }

    if (layerStatsLength(byTime) < 1) {
        errors.push("getLayerStats 应至少 1 条");
    } else if (!findLayerStat(byTime, "demo_label") && !findLayerStat(byTime, "lvgl_demo")) {
        errors.push("未统计到 demo_label / lvgl_demo");
    }

    return errors;
}

function runPerfAutoTest() {
    if (!hasPerfApi()) {
        setPerfStatus("自动检测失败：无 YUI.perf API", "#e94560");
        return;
    }

    enablePerfDefaults();
    setPerfStatus("自动检测中…", "#f5a623");

    setTimeout(function() {
        pulseDemoLabel();
        setTimeout(function() {
            var result = refreshPerfStats();
            var errors = assertPerfAuto(result && result.frame, result && result.byTime);

            if (errors.length === 0) {
                setPerfStatus("自动检测通过", "#0f9b8e");
                YUI.log("lvgl perf auto test: PASS");
            } else {
                setPerfStatus("失败：" + errors.join("; "), "#e94560");
                YUI.log("lvgl perf auto test: FAIL " + errors.join("; "));
            }
        }, 300);
    }, 300);
}

function onAppLoad() {
    YUI.log("lvgl app.js loaded");

    if (!hasPerfApi()) {
        setPerfStatus("失败：YUI.perf 未注册", "#e94560");
        return;
    }

    enablePerfDefaults();
    setPerfStatus("perf 已启用，watch: lvgl_demo / demo_label / demo_calendar", "#0f9b8e");

    setTimeout(function() {
        refreshPerfStats();
        runPerfAutoTest();
    }, 400);
}
