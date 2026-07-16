// YUI.perf 渲染性能测试
// 运行：ya -r playground -- app/tests/test-perf.json

var g_overlayOn = true;
var g_stressOn = false;
var g_stressTimer = 0;
var g_pulseCount = 0;
var g_autoTestDone = false;

function setStatus(text, color) {
    var change = { text: text };
    if (color) {
        change.color = color;
    }
    YUI.update({ target: "statusLabel", change: change });
}

function setStats(text) {
    YUI.update({
        target: "statsLabel",
        change: { text: text }
    });
}

function hasPerfApi() {
    return typeof YUI.perf !== "undefined" &&
        typeof YUI.perf.enable === "function" &&
        typeof YUI.perf.getFrameStats === "function" &&
        typeof YUI.perf.getLayerStats === "function";
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

function buildPerfExportPayload() {
    var payload = {
        frame: YUI.perf.getFrameStats(),
        layersByTime: layerStatsToArray(YUI.perf.getLayerStats("time")),
        layersByCount: layerStatsToArray(YUI.perf.getLayerStats("count")),
        layersByName: layerStatsToArray(YUI.perf.getLayerStats("name")),
        stressOn: g_stressOn,
        overlayOn: g_overlayOn,
        pulseCount: g_pulseCount
    };
    if (typeof Date !== "undefined") {
        payload.exportedAt = new Date().toISOString();
    }
    return payload;
}

function exportPerfToFile(filePath) {
    if (!hasPerfApi()) {
        setStatus("YUI.perf API 不可用", "#f38ba8");
        return false;
    }
    if (typeof YUI.writeFile !== "function") {
        setStatus("YUI.writeFile 不可用", "#f38ba8");
        return false;
    }

    var path = filePath || "perf-report.json";
    var payload = buildPerfExportPayload();
    var json = JSON.stringify(payload, null, 2);
    var ok = YUI.writeFile(path, json);

    if (ok) {
        setStatus("已导出：" + path, "#a6e3a1");
        YUI.log("perf export: " + path);
    } else {
        setStatus("导出失败：" + path, "#f38ba8");
        YUI.log("perf export failed: " + path);
    }
    return ok;
}

function exportPerfStats() {
    var frame = YUI.perf.getFrameStats();
    var suffix = frame && frame.frameIndex ? ("-f" + frame.frameIndex) : "";
    exportPerfToFile("perf-report" + suffix + ".json");
}

function formatFrameStats(frame) {
    if (!frame) {
        return "frame: (null)";
    }
    return "fps=" + frame.fps +
        " frameMs=" + frame.frameMs +
        " renderMs=" + frame.renderMs +
        " layers=" + frame.layerCount +
        " #=" + frame.frameIndex;
}

function formatTopLayers(arr, maxLines) {
    var lines = [];
    var i;
    var n = layerStatsLength(arr);
    var limit = maxLines || 5;
    if (n > limit) {
        n = limit;
    }
    for (i = 0; i < n; i++) {
        var item = arr[String(i)];
        if (!item) {
            continue;
        }
        var name = item.id ? item.id : ("type:" + item.type);
        lines.push(
            (i + 1) + ". " + name +
            " " + item.renderMs + "ms x" + item.renderCount
        );
    }
    return lines.join("\n");
}

function refreshPerfStats() {
    if (!hasPerfApi()) {
        setStatus("YUI.perf API 不可用", "#f38ba8");
        return;
    }

    var frame = YUI.perf.getFrameStats();
    var byTime = YUI.perf.getLayerStats("time");
    var byCount = YUI.perf.getLayerStats("count");

    var text = formatFrameStats(frame) + "\n── by time ──\n" + formatTopLayers(byTime, 6);
    text += "\n── by count ──\n" + formatTopLayers(byCount, 4);
    setStats(text);

    YUI.log("perf frame: " + formatFrameStats(frame));
    return { frame: frame, byTime: byTime, byCount: byCount };
}

function buildHeavySeed() {
    var lines = [];
    var i;
    lines.push("{");
    lines.push('  "id": "seed",');
    lines.push('  "type": "View",');
    lines.push('  "children": [');
    for (i = 0; i < 40; i++) {
        lines.push('    {"id": "item' + i + '", "type": "Label", "text": "line ' + i + '"},');
    }
    lines.push('    {"id": "tail", "type": "Label", "text": "end"}');
    lines.push("  ]");
    lines.push("}");
    return lines.join("\n");
}

function enablePerfDefaults() {
    YUI.perf.enable();
    YUI.perf.setTopN(8);
    YUI.perf.setOverlay(g_overlayOn);
    YUI.perf.watch("heavyEditor");
    YUI.perf.watch("lightLabel");
    YUI.perf.setLogInterval(0);
}

function togglePerfOverlay() {
    if (!hasPerfApi()) {
        setStatus("YUI.perf API 不可用", "#f38ba8");
        return;
    }
    g_overlayOn = !g_overlayOn;
    YUI.perf.setOverlay(g_overlayOn);
    YUI.update({
        target: "toggleOverlayBtn",
        change: { text: g_overlayOn ? "HUD 开" : "HUD 关" }
    });
    setStatus("HUD: " + (g_overlayOn ? "开启" : "关闭"), "#89b4fa");
}

function pulseLightLabel() {
    g_pulseCount++;
    YUI.update({
        target: "counterLabel",
        change: { text: "计数：" + g_pulseCount }
    });
    YUI.update({
        target: "lightLabel",
        change: { text: "脉冲 @" + g_pulseCount }
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
        change: { text: "开始压测", bgColor: "#f38ba8" }
    });
    setStatus("压测已停止", "#a6adc8");
}

function stressTick() {
    if (!g_stressOn) {
        return;
    }
    var text = YUI.getText("heavyEditor");
    text = text + "\n// stress " + g_pulseCount;
    if (text.length > 12000) {
        text = buildHeavySeed();
    }
    YUI.setText("heavyEditor", text);
    g_pulseCount++;
    g_stressTimer = setTimeout(stressTick, 80);
}

function togglePerfStress() {
    if (!hasPerfApi()) {
        setStatus("YUI.perf API 不可用", "#f38ba8");
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
        change: { text: "停止压测", bgColor: "#eba0ac" }
    });
    setStatus("压测中：每 80ms 追加 heavyEditor 文本", "#f9e2af");
    stressTick();
}

function disablePerf() {
    if (!hasPerfApi()) {
        return;
    }
    stopPerfStress();
    YUI.perf.disable();
    g_overlayOn = false;
    YUI.update({
        target: "toggleOverlayBtn",
        change: { text: "HUD 关" }
    });
    setStatus("perf 已关闭", "#6c7086");
    setStats("perf 已禁用");
}

function assertPerfAuto(frame, byTime) {
    var errors = [];

    if (!frame) {
        errors.push("getFrameStats 返回空");
    } else {
        if (!(frame.layerCount > 0)) {
            errors.push("layerCount 应 > 0，实际=" + frame.layerCount);
        }
        if (!(frame.frameIndex > 0)) {
            errors.push("frameIndex 应 > 0");
        }
        if (!(frame.renderMs >= 0)) {
            errors.push("renderMs 无效");
        }
    }

    if (layerStatsLength(byTime) < 1) {
        errors.push("getLayerStats 应至少 1 条");
    } else {
        var heavy = findLayerStat(byTime, "heavyEditor");
        if (!heavy) {
            errors.push("未统计到 heavyEditor");
        }
    }

    return errors;
}

function runPerfAutoTest() {
    if (!hasPerfApi()) {
        setStatus("自动检测失败：无 YUI.perf API", "#f38ba8");
        return;
    }

    enablePerfDefaults();
    setStatus("自动检测中…", "#f9e2af");

    setTimeout(function() {
        pulseLightLabel();
        setTimeout(function() {
            var result = refreshPerfStats();
            var errors = assertPerfAuto(result.frame, result.byTime);

            if (errors.length === 0) {
                g_autoTestDone = true;
                setStatus("自动检测通过", "#a6e3a1");
                YUI.log("perf auto test: PASS");
            } else {
                setStatus("自动检测失败：" + errors.join("; "), "#f38ba8");
                YUI.log("perf auto test: FAIL " + errors.join("; "));
            }
        }, 300);
    }, 300);
}

function onLoadPerf() {
    YUI.log("perf 测试页加载");

    YUI.setText("heavyEditor", buildHeavySeed());

    if (!hasPerfApi()) {
        setStatus("失败：YUI.perf 未注册", "#f38ba8");
        return;
    }

    enablePerfDefaults();
    setStatus("perf 已启用，watch: heavyEditor / lightLabel", "#a6e3a1");

    setTimeout(function() {
        refreshPerfStats();
        runPerfAutoTest();
    }, 400);
}
