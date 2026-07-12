/**
 * 秒表 App
 */

var swState = {
    running: false,
    startMs: 0,
    elapsedMs: 0,
    tickId: null,
    laps: []
};

function onStopwatchLoad() {
    resetStopwatchDisplay();
}

function onStopwatchShow() {
    applyWatchTheme();
    updateStopwatchLabel();
}

function onStopwatchHide() {
    stopSwTick();
}

function formatStopwatch(ms) {
    var totalSec = Math.floor(ms / 1000);
    var m = Math.floor(totalSec / 60);
    var s = totalSec % 60;
    var tenths = Math.floor((ms % 1000) / 100);
    return (m < 10 ? "0" : "") + m + ":" + (s < 10 ? "0" : "") + s + "." + tenths;
}

function currentElapsedMs() {
    if (swState.running) {
        return swState.elapsedMs + (Date.now() - swState.startMs);
    }
    return swState.elapsedMs;
}

function updateStopwatchLabel() {
    YUI.setText("sw_display", formatStopwatch(currentElapsedMs()));
    var status = swState.running ? "计时中" : (swState.elapsedMs > 0 ? "已暂停" : "就绪");
    YUI.setText("sw_status", status);
    YUI.setText("btn_sw_go", swState.running ? "暂停" : "开始");
}

function refreshLapLabels() {
    for (var i = 0; i < 3; i++) {
        var text = "—";
        if (i < swState.laps.length) {
            text = "Lap " + (swState.laps.length - i) + "  " + formatStopwatch(swState.laps[i]);
        }
        YUI.setText("sw_lap_" + i, text);
    }
}

function stopSwTick() {
    if (swState.tickId) {
        clearTimeout(swState.tickId);
        swState.tickId = null;
    }
}

function swTick() {
    if (!swState.running) return;
    updateStopwatchLabel();
    swState.tickId = setTimeout(swTick, 100);
}

function toggleStopwatch() {
    if (swState.running) {
        swState.elapsedMs = currentElapsedMs();
        swState.running = false;
        stopSwTick();
    } else {
        swState.startMs = Date.now();
        swState.running = true;
        swTick();
    }
    updateStopwatchLabel();
}

function lapStopwatch() {
    if (!swState.running && swState.elapsedMs <= 0) return;
    swState.laps.unshift(currentElapsedMs());
    if (swState.laps.length > 10) swState.laps.pop();
    refreshLapLabels();
}

function resetStopwatchDisplay() {
    swState.running = false;
    swState.elapsedMs = 0;
    swState.laps = [];
    stopSwTick();
    updateStopwatchLabel();
    refreshLapLabels();
}

function resetStopwatch() {
    resetStopwatchDisplay();
}
