function onWeatherLoad() {
    refreshWeatherApp();
}

function onWeatherShow() {
    applyWatchTheme();
    refreshWeatherApp();
}

function refreshWeatherApp() {
    var w = Watch.complications.weather;
    YUI.setText("wx_temp", w.temp + "°");
    YUI.setText("wx_cond", w.cond || "晴");
}
