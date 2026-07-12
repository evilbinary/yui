var recState = { running: false, sec: 0, tickId: null };

function onRecorderLoad() {
    resetRecorder();
}

function onRecorderShow() {
    applyWatchTheme();
}

function onRecorderHide() {
    stopRecTick();
}

function formatRec(sec) {
    var m = Math.floor(sec / 60);
    var s = sec % 60;
    return (m < 10 ? "0" : "") + m + ":" + (s < 10 ? "0" : "") + s;
}

function updateRecUI() {
    YUI.setText("rec_display", formatRec(recState.sec));
    YUI.setText("rec_status", recState.running ? "录音中" : (recState.sec > 0 ? "已暂停" : "就绪"));
    YUI.setText("btn_rec_go", recState.running ? "暂停" : "录音");
    YUI.setText("rec_indicator", recState.running ? "🔴" : "●");
}

function stopRecTick() {
    if (recState.tickId) {
        clearTimeout(recState.tickId);
        recState.tickId = null;
    }
}

function recTick() {
    if (!recState.running) return;
    recState.sec += 1;
    updateRecUI();
    recState.tickId = setTimeout(recTick, 1000);
}

function toggleRecording() {
    if (recState.running) {
        recState.running = false;
        stopRecTick();
    } else {
        recState.running = true;
        recTick();
    }
    updateRecUI();
}

function saveRecording() {
    if (recState.sec <= 0) return;
    YUI.setText("rec_status", "已保存 " + formatRec(recState.sec));
    resetRecorder();
}

function resetRecorder() {
    recState.running = false;
    recState.sec = 0;
    stopRecTick();
    updateRecUI();
}
