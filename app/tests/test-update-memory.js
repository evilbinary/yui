// YUI.update 内存压力测试
// 用法：playground app/tests/test-update-memory.json
// 打开任务管理器观察 playground.exe 专用内存，对比各按钮前后变化

var g_stats = {
    rounds: 0,
    updates: 0,
    lastMs: 0
};

var g_busy = false;
var g_typewriter = {
    running: false,
    timer: null,
    mode: "",
    startMs: 0,
    lastRenderMs: 0,
    lastStatsMs: 0,
    visible: "",
    pending: "",
    chars: 0,
    uiUpdates: 0,
    sequence: 0,
    seedOffset: 0,
    currentSeed: "",
    maxChars: 12000
};

var g_perf = {
    overlayOn: true,
    pollTimer: null
};

function hasPerfApi() {
    return typeof YUI.perf !== "undefined" &&
        typeof YUI.perf.enable === "function" &&
        typeof YUI.perf.getFrameStats === "function" &&
        typeof YUI.perf.getLayerStats === "function";
}

function layerStatsLength(arr) {
    var i = 0;
    if (!arr) {
        return 0;
    }
    while (arr[String(i)] !== undefined && arr[String(i)] !== null) {
        i++;
    }
    return i;
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
    return lines.join(" | ");
}

function setPerfStats(text) {
    YUI.setText("perfStatsLabel", text);
}

function enablePerfDefaults() {
    if (!hasPerfApi()) {
        return false;
    }
    YUI.perf.enable();
    YUI.perf.setTopN(8);
    YUI.perf.setOverlay(g_perf.overlayOn);
    YUI.perf.watch("typewriterText");
    YUI.perf.setLogInterval(0);
    return true;
}

function refreshPerfStats() {
    if (!hasPerfApi()) {
        setPerfStats("YUI.perf 不可用");
        return null;
    }
    var frame = YUI.perf.getFrameStats();
    var byTime = YUI.perf.getLayerStats("time");
    var byCount = YUI.perf.getLayerStats("count");
    var text = formatFrameStats(frame);
    var topTime = formatTopLayers(byTime, 3);
    var topCount = formatTopLayers(byCount, 2);
    if (topTime) {
        text += "\n慢层: " + topTime;
    }
    if (topCount) {
        text += "\n高频: " + topCount;
    }
    setPerfStats(text);
    return { frame: frame, byTime: byTime, byCount: byCount };
}

function togglePerfOverlay() {
    if (!hasPerfApi()) {
        setStatus("YUI.perf API 不可用", "#f38ba8");
        return;
    }
    g_perf.overlayOn = !g_perf.overlayOn;
    YUI.perf.setOverlay(g_perf.overlayOn);
    YUI.setText("toggleOverlayBtn", g_perf.overlayOn ? "HUD 开" : "HUD 关");
    setStatus("perf HUD: " + (g_perf.overlayOn ? "开启" : "关闭"), "#89b4fa");
}

function stopPerfPoll() {
    if (g_perf.pollTimer) {
        clearInterval(g_perf.pollTimer);
        g_perf.pollTimer = null;
    }
}

function startPerfPoll() {
    stopPerfPoll();
    if (!hasPerfApi()) {
        return;
    }
    g_perf.pollTimer = setInterval(refreshPerfStats, 500);
}

function togglePerfPoll() {
    if (g_perf.pollTimer) {
        stopPerfPoll();
        YUI.setText("perfPollBtn", "自动刷新");
        setStatus("perf 自动刷新已停止", "#a6adc8");
        return;
    }
    startPerfPoll();
    YUI.setText("perfPollBtn", "停止刷新");
    setStatus("perf 每 500ms 自动刷新", "#a6e3a1");
}

function exportPerfStats() {
    if (!hasPerfApi()) {
        setStatus("YUI.perf API 不可用", "#f38ba8");
        return;
    }
    if (typeof YUI.writeFile !== "function") {
        setStatus("YUI.writeFile 不可用", "#f38ba8");
        return;
    }
    var frame = YUI.perf.getFrameStats();
    var payload = {
        frame: frame,
        typewriter: {
            running: g_typewriter.running,
            mode: g_typewriter.mode,
            chars: g_typewriter.chars,
            uiUpdates: g_typewriter.uiUpdates
        },
        stats: g_stats,
        layersByTime: layerStatsToArray(YUI.perf.getLayerStats("time")),
        layersByCount: layerStatsToArray(YUI.perf.getLayerStats("count"))
    };
    var path = "update-memory-perf.json";
    if (YUI.writeFile(path, JSON.stringify(payload, null, 2))) {
        setStatus("perf 已导出: " + path, "#a6e3a1");
    } else {
        setStatus("perf 导出失败", "#f38ba8");
    }
}

function setStatus(text, color) {
    YUI.update({
        target: "statusLabel",
        change: {
            text: text,
            color: color || "#a6adc8"
        }
    });
}

function refreshStats(extra) {
    var line = "统计：轮次 " + g_stats.rounds +
        " | update " + g_stats.updates +
        " | 上次耗时 " + g_stats.lastMs + "ms";
    if (extra) {
        line += " | " + extra;
    }
    YUI.update({
        target: "statsLabel",
        change: { text: line }
    });
}

function emptyRootJson() {
    return {
        id: "root",
        type: "View",
        size: [400, 400],
        layout: { type: "vertical", spacing: 4 },
        children: [
            {
                id: "grid",
                type: "View",
                size: [380, 360],
                layout: { type: "grid", columns: 10, spacing: 2 },
                children: []
            }
        ]
    };
}

function resetPreviewOnly() {
    if (g_typewriter.running) {
        stopTypewriter();
    }
    var json = emptyRootJson();
    YUI.renderFromJson("previewLabel", JSON.stringify(json));
    YUI.show("root");
    g_stats.rounds++;
    setStatus("已重置预览区（空 root + grid）", "#a6e3a1");
    refreshStats();
    YUI.log("resetPreviewOnly: done");
}

function makeButtonChild(index, round) {
    var id = round !== undefined && round !== null
        ? ("r" + round + "_btn" + index)
        : ("btn" + index);
    return {
        id: id,
        type: "Button",
        size: [30, 30],
        text: String(index % 10),
        style: {
            bgColor: "#313244",
            color: "#cdd6f4",
            fontSize: 11,
            borderRadius: 2
        }
    };
}

function makeButtonUpdate(index, round) {
    return {
        target: "grid",
        change: {
            children: [makeButtonChild(index, round)]
        }
    };
}

function buildMinesweeperRoot() {
    var cells = [];
    var i;
    for (i = 0; i < 9; i++) {
        cells.push({
            id: "c" + i,
            type: "Button",
            size: [30, 30],
            text: "",
            style: {
                bgColor: "#45475a",
                color: "#cdd6f4",
                fontSize: 12,
                borderRadius: 2
            }
        });
    }
    return {
        id: "root",
        type: "View",
        size: [200, 300],
        layout: { type: "vertical", spacing: 4 },
        children: [
            { id: "title", type: "Label", text: "扫雷", size: [0, 30] },
            {
                id: "status",
                type: "View",
                size: [0, 30],
                layout: { type: "horizontal", spacing: 20 },
                children: [
                    { id: "remain", type: "Label", text: "雷数: 10" },
                    { id: "timer", type: "Label", text: "时间: 0" }
                ]
            },
            {
                id: "grid",
                type: "View",
                size: [180, 180],
                layout: { type: "grid", columns: 3, spacing: 2 },
                children: cells
            }
        ]
    };
}

function runSequentialUpdates(count, resetFirst, round) {
    var i;
    var t0 = Date.now();

    if (resetFirst) {
        resetPreviewOnly();
        g_stats.rounds--;
    }

    for (i = 0; i < count; i++) {
        YUI.update(JSON.stringify(makeButtonUpdate(i, round)));
        g_stats.updates++;
    }

    g_stats.lastMs = Date.now() - t0;
    refreshStats("顺序 " + count + " 条");
    YUI.log("runSequentialUpdates: count=" + count + " reset=" + resetFirst + " ms=" + g_stats.lastMs);
}

function runSequentialUpdatesChunked(count, resetFirst, round) {
    var i = 0;
    var t0 = Date.now();
    var chunk = 500;

    if (resetFirst) {
        resetPreviewOnly();
        g_stats.rounds--;
    }

    function step() {
        var end = Math.min(i + chunk, count);
        for (; i < end; i++) {
            YUI.update(JSON.stringify(makeButtonUpdate(i, round)));
            g_stats.updates++;
        }
        if (i < count) {
            refreshStats("顺序 " + count + " … " + i + "/" + count);
            setTimeout(step, 0);
        } else {
            g_stats.lastMs = Date.now() - t0;
            refreshStats("顺序 " + count + " 条");
            g_busy = false;
            setStatus("顺序 ×" + count + " 完成", "#a6e3a1");
            YUI.log("runSequentialUpdatesChunked: count=" + count + " ms=" + g_stats.lastMs);
        }
    }

    step();
}

function runBatchUpdates(count, resetFirst, round, onDone) {
    var batch = [];
    var i = 0;
    var t0 = Date.now();
    var chunk = 2000;

    if (resetFirst) {
        resetPreviewOnly();
        g_stats.rounds--;
    }

    function buildStep() {
        var end = Math.min(i + chunk, count);
        for (; i < end; i++) {
            batch.push(makeButtonUpdate(i, round));
        }
        if (i < count) {
            refreshStats("构建批量 " + i + "/" + count);
            setTimeout(buildStep, 0);
            return;
        }
        setStatus("批量 update ×" + count + " …", "#a6e3a1");
        var t1 = Date.now();
        var jsonStr = JSON.stringify(batch);
        var t2 = Date.now();
        YUI.update(jsonStr);
        var t3 = Date.now();
        g_stats.updates += count;
        g_stats.lastMs = t3 - t0;
        refreshStats("批量 " + count + " 条");
        YUI.log("runBatchUpdates: count=" + count +
            " build=" + (t1 - t0) + "ms" +
            " stringify=" + (t2 - t1) + "ms" +
            " update=" + (t3 - t2) + "ms" +
            " total=" + g_stats.lastMs + "ms");
        if (onDone) {
            onDone();
        }
    }

    buildStep();
}

function runRounds(rounds, perRound, resetEachRound) {
    var r;
    var t0 = Date.now();

    if (resetEachRound) {
        resetPreviewOnly();
        g_stats.rounds--;
    }

    for (r = 0; r < rounds; r++) {
        if (resetEachRound && r > 0) {
            resetPreviewOnly();
            g_stats.rounds--;
        }
        runSequentialUpdates(perRound, false, resetEachRound ? null : r);
        g_stats.rounds++;
    }

    g_stats.lastMs = Date.now() - t0;
    setStatus("完成 " + rounds + " 轮 x " + perRound + " 条 update" +
        (resetEachRound ? "（每轮重置）" : "（不重置，模拟累积）"), "#f9e2af");
    refreshStats();
}

function guardBusy(fn) {
    if (g_busy) {
        setStatus("测试进行中，请稍候…", "#fab387");
        return;
    }
    g_busy = true;
    try {
        fn();
    } catch (e) {
        setStatus("错误: " + e, "#f38ba8");
        YUI.log("stress error: " + e);
    }
    g_busy = false;
}

function stressSequentialLarge(count) {
    if (g_busy) {
        setStatus("测试进行中，请稍候…", "#fab387");
        return;
    }
    g_busy = true;
    setStatus("顺序 update ×" + count + " …", "#89b4fa");
    runSequentialUpdatesChunked(count, true);
}

function stressBatch(count) {
    if (g_busy) {
        setStatus("测试进行中，请稍候…", "#fab387");
        return;
    }
    g_busy = true;
    setStatus("批量 update ×" + count + " …", "#a6e3a1");
    runBatchUpdates(count, true, null, function() {
        g_busy = false;
        setStatus("批量 ×" + count + " 完成", "#a6e3a1");
    });
}

function stressSequential50() {
    guardBusy(function() {
        setStatus("顺序 update ×50 …", "#89b4fa");
        runSequentialUpdates(50, true);
        setStatus("顺序 ×50 完成", "#a6e3a1");
    });
}

function stressSequential200() {
    guardBusy(function() {
        setStatus("顺序 update ×200 …", "#89b4fa");
        runSequentialUpdates(200, true);
        setStatus("顺序 ×200 完成", "#a6e3a1");
    });
}

function stressSequential5000() {
    stressSequentialLarge(5000);
}

function stressSequential10000() {
    stressSequentialLarge(10000);
}

function stressSequential50000() {
    stressSequentialLarge(50000);
}

function stressBatch50() {
    stressBatch(50);
}

function stressBatch200() {
    stressBatch(200);
}

function stressBatch5000() {
    stressBatch(5000);
}

function stressBatch10000() {
    stressBatch(10000);
}

function stressBatch50000() {
    stressBatch(50000);
}

function stressRounds5() {
    guardBusy(function() {
        setStatus("5 轮 ×50（每轮重置）…", "#f9e2af");
        runRounds(5, 50, true);
    });
}

function stressRounds10() {
    guardBusy(function() {
        setStatus("10 轮 ×50（每轮重置）…", "#f9e2af");
        runRounds(10, 50, true);
    });
}

function stressNoReset10() {
    guardBusy(function() {
        setStatus("10 轮 ×50（不重置，模拟旧版累积）…", "#fab387");
        runRounds(10, 50, false);
        setStatus("不重置累积测试完成 — 内存若持续上涨说明有泄漏", "#fab387");
    });
}

function stressMinesweeper() {
    guardBusy(function() {
        var t0 = Date.now();
        setStatus("renderFromJson 扫雷 UI …", "#cba6f7");

        YUI.renderFromJson("previewLabel", JSON.stringify(buildMinesweeperRoot()));
        YUI.show("root");

        g_stats.rounds++;
        g_stats.updates++;
        g_stats.lastMs = Date.now() - t0;
        setStatus("扫雷 UI 已渲染（单次全量）", "#cba6f7");
        refreshStats("全量 renderFromJson");
    });
}

function buildTypewriterRoot() {
    return {
        id: "typewriterRoot",
        type: "View",
        size: [820, 360],
        layout: {
            type: "vertical",
            spacing: 8,
            padding: [12, 12, 12, 12]
        },
        style: {
            bgColor: "#181825",
            borderRadius: 8
        },
        children: [
            {
                id: "typewriterTitle",
                type: "Label",
                text: "流式打字机：Text.setText 基准",
                size: [0, 24],
                style: { color: "#cdd6f4", fontSize: 14 }
            },
            {
                id: "typewriterText",
                type: "Text",
                text: "",
                multiline: true,
                editable: false,
                scrollable: 0,
                size: [780, 290],
                style: {
                    color: "#cdd6f4",
                    bgColor: "#11111b",
                    fontSize: 14,
                    padding: [8, 8, 8, 8]
                }
            }
        ]
    };
}

function typewriterSeed(sequence) {
    var phrases = [
        "正在生成摄影点评：构图层次清晰，建议保留主体周围的留白。",
        "正在分析光影关系：侧逆光增强了轮廓，但高光区域可以适当压低。",
        "正在整理色彩建议：降低背景饱和度，让视觉焦点集中在主体。",
        "正在模拟 SSE 分片：每一段文本均不同，用于观察纹理缓存和内存变化。"
    ];
    var phrase = phrases[sequence % phrases.length];
    return "[" + (sequence + 1) + " | " + Date.now() + "] " + phrase + "\n";
}

function typewriterPerfText() {
    if (!hasPerfApi()) {
        return "perf 不可用";
    }
    var frame = YUI.perf.getFrameStats();
    if (!frame) {
        return "perf 未就绪";
    }
    return formatFrameStats(frame);
}

function refreshTypewriterStats(force) {
    var now = Date.now();
    if (!force && now - g_typewriter.lastStatsMs < 500) {
        return;
    }
    g_typewriter.lastStatsMs = now;
    var elapsed = g_typewriter.startMs ? now - g_typewriter.startMs : 0;
    var text = "打字机：" + (g_typewriter.running ? "运行" : "停止") +
        " | 模式 " + (g_typewriter.mode || "-") +
        " | 字符 " + g_typewriter.chars + "/" + g_typewriter.maxChars +
        " | UI 更新 " + g_typewriter.uiUpdates +
        " | " + elapsed + "ms" +
        " | " + typewriterPerfText();
    YUI.setText("typewriterStats", text);
    refreshPerfStats();
}

function typewriterRender() {
    if (!g_typewriter.pending) {
        return;
    }
    g_typewriter.visible += g_typewriter.pending;
    g_typewriter.pending = "";
    YUI.setText("typewriterText", g_typewriter.visible);
    g_typewriter.uiUpdates++;
}

function stopTypewriter() {
    if (g_typewriter.timer) {
        clearTimeout(g_typewriter.timer);
        g_typewriter.timer = null;
    }
    if (g_typewriter.pending) {
        typewriterRender();
    }
    g_typewriter.running = false;
    refreshTypewriterStats(true);
    setStatus("打字机已停止；请对比任务管理器内存和 perf 数据", "#a6adc8");
}

function typewriterTick() {
    if (!g_typewriter.running) {
        return;
    }

    var remaining = g_typewriter.maxChars - g_typewriter.chars;
    if (remaining <= 0) {
        stopTypewriter();
        setStatus("打字机已达到 " + g_typewriter.maxChars + " 字上限", "#f9e2af");
        return;
    }

    if (!g_typewriter.currentSeed ||
        g_typewriter.seedOffset >= g_typewriter.currentSeed.length) {
        g_typewriter.currentSeed = typewriterSeed(g_typewriter.sequence);
        g_typewriter.sequence++;
        g_typewriter.seedOffset = 0;
    }
    var chunk = g_typewriter.currentSeed.substring(
        g_typewriter.seedOffset,
        g_typewriter.seedOffset + 4
    );
    if (chunk.length > remaining) {
        chunk = chunk.substring(0, remaining);
    }

    g_typewriter.seedOffset += chunk.length;
    g_typewriter.chars += chunk.length;

    if (g_typewriter.mode === "逐字") {
        g_typewriter.pending = chunk;
        typewriterRender();
    } else {
        g_typewriter.pending += chunk;
        if (Date.now() - g_typewriter.lastRenderMs >= 200) {
            typewriterRender();
            g_typewriter.lastRenderMs = Date.now();
        }
    }

    refreshTypewriterStats(false);
    g_typewriter.timer = setTimeout(typewriterTick, 40);
}

function startTypewriter(mode) {
    stopTypewriter();
    YUI.renderFromJson("previewLabel", JSON.stringify(buildTypewriterRoot()));
    YUI.show("typewriterRoot");

    g_typewriter.mode = mode;
    g_typewriter.startMs = Date.now();
    g_typewriter.lastRenderMs = g_typewriter.startMs;
    g_typewriter.lastStatsMs = 0;
    g_typewriter.visible = "";
    g_typewriter.pending = "";
    g_typewriter.chars = 0;
    g_typewriter.uiUpdates = 0;
    g_typewriter.sequence = 0;
    g_typewriter.seedOffset = 0;
    g_typewriter.currentSeed = "";
    g_typewriter.running = true;

    enablePerfDefaults();

    setStatus("打字机压测：" + mode + "，每 40ms 产生 4 个字符", "#89b4fa");
    refreshTypewriterStats(true);
    typewriterTick();
}

function startTypewriterPerChunk() {
    startTypewriter("逐字");
}

function startTypewriterBatched() {
    startTypewriter("200ms 合并");
}

function resetTypewriter() {
    stopTypewriter();
    YUI.renderFromJson("previewLabel", JSON.stringify(buildTypewriterRoot()));
    YUI.show("typewriterRoot");
    YUI.setText("typewriterText", "");
    g_typewriter.chars = 0;
    g_typewriter.uiUpdates = 0;
    g_typewriter.mode = "";
    refreshTypewriterStats(true);
    setStatus("打字机已重置", "#a6e3a1");
}

function onLoad() {
    resetPreviewOnly();
    g_stats.rounds = 0;
    g_stats.updates = 0;
    if (enablePerfDefaults()) {
        startPerfPoll();
        YUI.setText("perfPollBtn", "停止刷新");
        YUI.setText("toggleOverlayBtn", g_perf.overlayOn ? "HUD 开" : "HUD 关");
        refreshPerfStats();
        setStatus("就绪：perf 已启用，可观察 HUD / 导出 JSON", "#cdd6f4");
    } else {
        setStatus("就绪：无 YUI.perf，请用任务管理器观察内存", "#fab387");
    }
    YUI.log("test-update-memory: loaded");
    YUI.log("顺序×N = 模拟 SSE 逐条 YUI.update");
    YUI.log("批量×N = 一次 YUI.update 数组");
    YUI.log("N轮×50(不重置) = 模拟优化前的累积场景");
    YUI.log("打字机逐字 = 每 40ms YUI.setText；200ms 合并 = 模拟 SSE 节流刷新");
}
