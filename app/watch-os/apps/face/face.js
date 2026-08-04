/**
 * 表盘 App - 现代极简运动风格
 */

var FACE_DAYS = ["SUNDAY", "MONDAY", "TUESDAY", "WEDNESDAY", "THURSDAY", "FRIDAY", "SATURDAY"];
var FACE_MONTHS = ["JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"];

function onFaceLoad() {
    refreshFaceDock();
    updateFaceData();
    applyWatchTheme();
}

function onFaceShow() {
    refreshFaceDock();
    updateFaceData();
    applyWatchTheme();
}

function refreshFaceDock() {
    if (typeof WatchAppRegistry === "undefined") return;

    var dockApps = WatchAppRegistry.getDockApps(3);
    for (var i = 0; i < dockApps.length; i++) {
        var app = dockApps[i];
        YUI.setText("dock_app_" + app.id, app.icon);
    }
}

function onDockAppClick(layerId) {
    if (!layerId) return;
    var prefix = "dock_app_";
    if (layerId.indexOf(prefix) === 0) {
        WatchAppRegistry.openById(layerId.substring(prefix.length));
    }
}

function onFaceHide() {
}

function updateFaceData() {
    updateFaceClock();
    updateFaceDate();
    updateFaceComplications();
}

function updateFaceDate() {
    /* mquickjs: Date.now() only — approximate weekday/month from UTC ms */
    var ms = Date.now();
    var dayMs = 86400000;
    var days = Math.floor(ms / dayMs);
    var dow = (days + 4) % 7; /* 1970-01-01 was Thursday */
    var y = 1970;
    var rem = days;
    while (1) {
        var ydays = ((y % 4 === 0 && y % 100 !== 0) || (y % 400 === 0)) ? 366 : 365;
        if (rem < ydays) break;
        rem -= ydays;
        y++;
    }
    var mdays = [31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31];
    if ((y % 4 === 0 && y % 100 !== 0) || (y % 400 === 0)) mdays[1] = 29;
    var mo = 0;
    while (mo < 12 && rem >= mdays[mo]) {
        rem -= mdays[mo];
        mo++;
    }
    var dateStr = FACE_DAYS[dow] + " · " + FACE_MONTHS[mo] + " " + (rem + 1);
    YUI.setText("face_date", dateStr);
}

function updateFaceComplications() {
    var c = Watch.complications;
    var w = c.weather;

    YUI.setText("face_weather_temp", w.temp + "°");
    YUI.setText("face_weather_cond", (w.cond || "CLEAR").toUpperCase());
    YUI.setText("face_steps", formatWatchNumber(c.steps.value));
    YUI.setText("face_heart", c.heart.value + " BPM");
}
