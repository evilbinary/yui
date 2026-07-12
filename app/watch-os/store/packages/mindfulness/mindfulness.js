var mindState = { running: false, phase: 0, count: 4, tickId: null };
var mindPhases = ["吸气", "屏息", "呼气", "休息"];

function onMindfulnessLoad() {
    resetMindfulness();
}

function onMindfulnessShow() {
    applyWatchTheme();
}

function onMindfulnessHide() {
    stopMindTick();
}

function resetMindfulness() {
    mindState.running = false;
    mindState.phase = 0;
    stopMindTick();
    YUI.setText("mind_phase", "吸气");
    YUI.setText("mind_timer", "4");
    YUI.setText("mind_circle", "◎");
    YUI.setText("btn_mind_go", "开始");
}

function stopMindTick() {
    if (mindState.tickId) {
        clearTimeout(mindState.tickId);
        mindState.tickId = null;
    }
}

function mindTick() {
    if (!mindState.running) return;
    mindState.count -= 1;
    if (mindState.count <= 0) {
        mindState.phase = (mindState.phase + 1) % mindPhases.length;
        mindState.count = 4;
        YUI.setText("mind_phase", mindPhases[mindState.phase]);
        YUI.setText("mind_circle", mindState.phase % 2 === 0 ? "◉" : "◎");
    }
    YUI.setText("mind_timer", String(mindState.count));
    mindState.tickId = setTimeout(mindTick, 1000);
}

function toggleMindfulness() {
    if (mindState.running) {
        mindState.running = false;
        stopMindTick();
        YUI.setText("btn_mind_go", "开始");
        return;
    }
    mindState.running = true;
    YUI.setText("btn_mind_go", "停止");
    mindTick();
}
