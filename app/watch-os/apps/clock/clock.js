/**
 * 时钟 App - 模拟表盘 + 世界时钟
 */

var CLOCK_DAYS = ["SUNDAY", "MONDAY", "TUESDAY", "WEDNESDAY", "THURSDAY", "FRIDAY", "SATURDAY"];
var CLOCK_MONTHS = ["JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"];

var clockWorldCities = [
    { name: "纽约", offsetMin: -5 * 60 },
    { name: "伦敦", offsetMin: 0 },
    { name: "东京", offsetMin: 9 * 60 },
    { name: "巴黎", offsetMin: 1 * 60 }
];

var clockTickId = null;

function onClockLoad() {
    refreshClockDisplay();
}

function onClockShow() {
    applyWatchTheme();
    refreshClockDisplay();
    startClockTick();
}

function onClockHide() {
    stopClockTick();
}

function startClockTick() {
    stopClockTick();
    clockTickId = setTimeout(function loop() {
        refreshClockDisplay();
        clockTickId = setTimeout(loop, 1000);
    }, 1000);
}

function stopClockTick() {
    if (clockTickId) {
        clearTimeout(clockTickId);
        clockTickId = null;
    }
}

function formatClockTime(date) {
    var h = date.getHours();
    var m = date.getMinutes();
    return (h < 10 ? "0" : "") + h + ":" + (m < 10 ? "0" : "") + m;
}

function cityTimeAtOffset(offsetMin) {
    var now = new Date();
    var utc = now.getTime() + now.getTimezoneOffset() * 60000;
    return new Date(utc + offsetMin * 60000);
}

function refreshClockDisplay() {
    var now = new Date();
    YUI.setText("clock_local_time", formatClockTime(now));
    YUI.setText("clock_local_date", CLOCK_DAYS[now.getDay()] + " · " + CLOCK_MONTHS[now.getMonth()] + " " + now.getDate());

    for (var i = 0; i < clockWorldCities.length; i++) {
        var city = clockWorldCities[i];
        var t = cityTimeAtOffset(city.offsetMin);
        YUI.setText("clock_city_" + i + "_name", city.name);
        YUI.setText("clock_city_" + i + "_time", formatClockTime(t));
    }
}
