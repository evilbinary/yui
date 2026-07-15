// YUI.update 内存压力测试
// 用法：playground app/tests/test-update-memory.json
// 打开任务管理器观察 playground.exe 专用内存，对比各按钮前后变化

var g_stats = {
    rounds: 0,
    updates: 0,
    lastMs: 0
};

var g_busy = false;

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

function onLoad() {
    resetPreviewOnly();
    g_stats.rounds = 0;
    g_stats.updates = 0;
    setStatus("就绪：先记基准内存，再点按钮对比", "#cdd6f4");
    YUI.log("test-update-memory: loaded");
    YUI.log("顺序×N = 模拟 SSE 逐条 YUI.update");
    YUI.log("批量×N = 一次 YUI.update 数组");
    YUI.log("N轮×50(不重置) = 模拟优化前的累积场景");
}
