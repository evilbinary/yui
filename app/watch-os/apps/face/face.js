/**
 * 表盘 App - 现代极简运动风格
 */

var FACE_DAYS = ["SUNDAY", "MONDAY", "TUESDAY", "WEDNESDAY", "THURSDAY", "FRIDAY", "SATURDAY"];
var FACE_MONTHS = ["JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"];

function onFaceLoad() {
    updateFaceData();
    applyWatchTheme();
}

function onFaceShow() {
    updateFaceData();
    applyWatchTheme();
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
