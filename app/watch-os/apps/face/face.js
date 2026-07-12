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
    var now = new Date();
    var dateStr = FACE_DAYS[now.getDay()] + " · " + FACE_MONTHS[now.getMonth()] + " " + now.getDate();
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
