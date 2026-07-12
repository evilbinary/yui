/**
 * 计时器 App
 */

var timerState = {
    totalSec: 0,
    remainingSec: 0,
    running: false,
    tickId: null
};

function onTimerLoad() {
    resetTimerDisplay();
}

function onTimerShow() {
    updateTimerLabel();
    applyWatchTheme();
}

function onTimerHide() {
    stopTimerTick();
}

function resetTimerDisplay() {
    timerState.totalSec = 0;
    timerState.remainingSec = 0;
    timerState.running = false;
    stopTimerTick();
    updateTimerLabel();
}

function formatTimer(sec) {
    var m = Math.floor(sec / 60);
    var s = sec % 60;
    return (m < 10 ? "0" : "") + m + ":" + (s < 10 ? "0" : "") + s;
}

function updateTimerLabel() {
    YUI.setText("timer_display", formatTimer(timerState.remainingSec));
    var status = timerState.running ? "计时中" : (timerState.remainingSec > 0 ? "已暂停" : "选择时长");
    YUI.setText("timer_status", status);
}

function setTimerMinutes(min) {
    stopTimerTick();
    timerState.totalSec = min * 60;
    timerState.remainingSec = min * 60;
    timerState.running = false;
    updateTimerLabel();
}

function timerPreset1() { setTimerMinutes(1); }
function timerPreset3() { setTimerMinutes(3); }
function timerPreset5() { setTimerMinutes(5); }
function timerPreset10() { setTimerMinutes(10); }

function stopTimerTick() {
    timerState.running = false;
    if (timerState.tickId) {
        clearTimeout(timerState.tickId);
        timerState.tickId = null;
    }
}

function timerTick() {
    if (!timerState.running) return;
    if (timerState.remainingSec <= 0) {
        stopTimerTick();
        YUI.setText("timer_status", "时间到！");
        return;
    }
    timerState.remainingSec -= 1;
    updateTimerLabel();
    timerState.tickId = setTimeout(timerTick, 1000);
}

function toggleTimer() {
    if (timerState.remainingSec <= 0) return;

    if (timerState.running) {
        stopTimerTick();
        updateTimerLabel();
        return;
    }

    timerState.running = true;
    updateTimerLabel();
    timerState.tickId = setTimeout(timerTick, 1000);
}

function resetTimer() {
    resetTimerDisplay();
}
