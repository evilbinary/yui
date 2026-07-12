/**
 * 表盘 App
 */

function onFaceLoad() {
    updateFaceClock();
    refreshFaceComplications();
}

function onFaceShow() {
    updateFaceClock();
    refreshFaceComplications();
    applyWatchTheme();
}
