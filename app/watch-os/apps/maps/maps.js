/**
 * 地图 App
 */

var mapsState = {
    heading: 0,
    tickId: null
};

var MAPS_HEADINGS = ["北", "东北", "东", "东南", "南", "西南", "西", "西北"];

function onMapsLoad() {
    refreshMapsLocation();
}

function onMapsShow() {
    applyWatchTheme();
    startMapsCompass();
}

function onMapsHide() {
    stopMapsCompass();
}

function headingLabel(deg) {
    var idx = Math.round(deg / 45) % 8;
    return MAPS_HEADINGS[idx] + " " + Math.round(deg) + "°";
}

function refreshMapsCompass() {
    mapsState.heading = (mapsState.heading + 2) % 360;
    var icons = ["↑", "↗", "→", "↘", "↓", "↙", "←", "↖"];
    var idx = Math.round(mapsState.heading / 45) % 8;
    YUI.setText("maps_heading_icon", icons[idx]);
    YUI.setText("maps_heading", headingLabel(mapsState.heading));
}

function startMapsCompass() {
    stopMapsCompass();
    refreshMapsCompass();
    mapsState.tickId = setTimeout(function loop() {
        refreshMapsCompass();
        mapsState.tickId = setTimeout(loop, 800);
    }, 800);
}

function stopMapsCompass() {
    if (mapsState.tickId) {
        clearTimeout(mapsState.tickId);
        mapsState.tickId = null;
    }
}

function refreshMapsLocation() {
    YUI.setText("maps_location", "上海市 · 浦东新区");
    YUI.setText("maps_coords", "31.23°N, 121.47°E");
    YUI.setText("maps_dest", "陆家嘴 · 2.4 km");
    YUI.setText("maps_eta", "步行约 28 分钟");
}

function startMapsNav() {
    YUI.setText("maps_dest", "导航中 · 陆家嘴");
    YUI.setText("maps_eta", "剩余 2.1 km · 24 分钟");
}
