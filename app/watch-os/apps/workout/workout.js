/**
 * 训练 App
 */

var workoutState = {
    activity: "run",
    activityLabel: "🏃 户外跑步",
    running: false,
    startMs: 0,
    elapsedSec: 0,
    tickId: null
};

var WORKOUT_TYPES = {
    run: { label: "🏃 户外跑步", distRate: 0.003 },
    cycle: { label: "🚴 户外骑行", distRate: 0.008 },
    swim: { label: "🏊 泳池游泳", distRate: 0.001 }
};

function onWorkoutLoad() {
    resetWorkoutUI();
}

function onWorkoutShow() {
    applyWatchTheme();
    updateWorkoutUI();
}

function onWorkoutHide() {
    stopWorkoutTick();
}

function formatWorkoutTime(sec) {
    var m = Math.floor(sec / 60);
    var s = sec % 60;
    return (m < 10 ? "0" : "") + m + ":" + (s < 10 ? "0" : "") + s;
}

function currentWorkoutSec() {
    if (workoutState.running) {
        return workoutState.elapsedSec + Math.floor((Date.now() - workoutState.startMs) / 1000);
    }
    return workoutState.elapsedSec;
}

function updateWorkoutUI() {
    var sec = currentWorkoutSec();
    var type = WORKOUT_TYPES[workoutState.activity];

    YUI.setText("workout_activity", workoutState.activityLabel);
    YUI.setText("workout_elapsed", formatWorkoutTime(sec));

    if (workoutState.running) {
        YUI.setText("workout_status", "训练中");
        YUI.setText("btn_workout_go", "暂停");
        var dist = (sec * type.distRate).toFixed(2);
        YUI.setText("workout_dist", dist);
        YUI.setText("workout_hr", String(120 + Math.floor(sec % 30)));
        YUI.setText("workout_cal", String(Math.floor(sec * 0.12)));
    } else if (workoutState.elapsedSec > 0) {
        YUI.setText("workout_status", "已暂停");
        YUI.setText("btn_workout_go", "继续");
    } else {
        YUI.setText("workout_status", "选择运动类型");
        YUI.setText("btn_workout_go", "开始");
        YUI.setText("workout_dist", "—");
        YUI.setText("workout_hr", "—");
        YUI.setText("workout_cal", "—");
    }
}

function stopWorkoutTick() {
    if (workoutState.tickId) {
        clearTimeout(workoutState.tickId);
        workoutState.tickId = null;
    }
}

function workoutTick() {
    if (!workoutState.running) return;
    updateWorkoutUI();
    workoutState.tickId = setTimeout(workoutTick, 1000);
}

function selectWorkoutType(key) {
    if (workoutState.running) return;
    workoutState.activity = key;
    workoutState.activityLabel = WORKOUT_TYPES[key].label;
    updateWorkoutUI();
}

function workoutSelectRun() { selectWorkoutType("run"); }
function workoutSelectCycle() { selectWorkoutType("cycle"); }
function workoutSelectSwim() { selectWorkoutType("swim"); }

function toggleWorkout() {
    if (workoutState.running) {
        workoutState.elapsedSec = currentWorkoutSec();
        workoutState.running = false;
        stopWorkoutTick();
    } else {
        workoutState.startMs = Date.now();
        workoutState.running = true;
        workoutTick();
    }
    updateWorkoutUI();
}

function endWorkout() {
    workoutState.running = false;
    workoutState.elapsedSec = 0;
    stopWorkoutTick();
    updateWorkoutUI();
}

function resetWorkoutUI() {
    workoutState.activity = "run";
    workoutState.activityLabel = WORKOUT_TYPES.run.label;
    workoutState.running = false;
    workoutState.elapsedSec = 0;
    stopWorkoutTick();
    updateWorkoutUI();
}
