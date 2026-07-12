/**
 * 闹钟 App
 */

var alarmState = [
    { time: "07:00", label: "工作日", enabled: true },
    { time: "08:30", label: "周末", enabled: false },
    { time: "22:00", label: "就寝", enabled: true }
];

function onAlarmLoad() {
    refreshAlarmUI();
}

function onAlarmShow() {
    applyWatchTheme();
    refreshAlarmUI();
}

function refreshAlarmUI() {
    for (var i = 0; i < alarmState.length; i++) {
        var a = alarmState[i];
        YUI.setText("alarm_time_" + i, a.time);
        YUI.setText("alarm_label_" + i, a.label);
        YUI.setText("btn_alarm_" + i, a.enabled ? "开" : "关");
    }
}

function toggleAlarm(index) {
    alarmState[index].enabled = !alarmState[index].enabled;
    refreshAlarmUI();
}

function toggleAlarm0() { toggleAlarm(0); }
function toggleAlarm1() { toggleAlarm(1); }
function toggleAlarm2() { toggleAlarm(2); }
