/**
 * 睡眠 App
 */

function onSleepLoad() {
    refreshSleepUI();
}

function onSleepShow() {
    applyWatchTheme();
    refreshSleepUI();
}

function formatSleepHours(h) {
    var hours = Math.floor(h);
    var mins = Math.round((h - hours) * 60);
    return hours + ":" + (mins < 10 ? "0" : "") + mins;
}

function formatSleepStage(h) {
    var hours = Math.floor(h);
    var mins = Math.round((h - hours) * 60);
    return hours + ":" + (mins < 10 ? "0" : "") + mins;
}

function refreshSleepUI() {
    var s = Watch.sleep.lastNight;
    var week = Watch.sleep.week;

    YUI.setText("sleep_total", formatSleepHours(s.total));
    YUI.setText("sleep_range", s.bed + " — " + s.wake);
    YUI.setText("sleep_deep_val", formatSleepStage(s.deep));
    YUI.setText("sleep_core_val", formatSleepStage(s.core));
    YUI.setText("sleep_rem_val", formatSleepStage(s.rem));

    var total = s.deep + s.core + s.rem + s.awake;
    var deepPct = Math.round((s.deep / total) * 100);
    var corePct = Math.round((s.core / total) * 100);
    var remPct = Math.round((s.rem / total) * 100);
    YUI.setProperty("sleep_deep_bar", "value", deepPct);
    YUI.setProperty("sleep_core_bar", "value", corePct);
    YUI.setProperty("sleep_rem_bar", "value", remPct);

    var avg = 0;
    for (var i = 0; i < week.length; i++) avg += week[i];
    avg = (avg / week.length).toFixed(1);
    YUI.setText("sleep_week_text", "平均 " + avg + " 小时 · 趋势稳定");

    var bars = "";
    var max = 0;
    for (var j = 0; j < week.length; j++) {
        if (week[j] > max) max = week[j];
    }
    var chars = ["▁", "▂", "▃", "▄", "▅", "▆", "▇", "█"];
    for (var k = 0; k < week.length; k++) {
        var idx = Math.min(7, Math.round((week[k] / max) * 7));
        bars += chars[idx];
    }
    YUI.setText("sleep_week_bars", bars);
}
