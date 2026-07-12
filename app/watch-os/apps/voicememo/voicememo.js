function onVoicememoLoad() {
    YUI.setText("vm_status", "点击录制新备忘");
}

function onVoicememoShow() {
    applyWatchTheme();
}

function recordVoicememo() {
    YUI.setText("vm_status", "录音中… 0:00");
    setTimeout(function() {
        YUI.setText("vm_status", "已保存新备忘");
    }, 1200);
}
