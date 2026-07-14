/**
 * LVGL demo — layer events from app.json
 */

function onDemoLabelClick() {
    YUI.log("LvLabel clicked: demo_label");
}

function onDemoSliderChange() {
    YUI.log("LvSlider value changed");
}

function onDemoSwitchChange() {
    YUI.log("LvSwitch toggled");
}

function onAppLoad() {
    YUI.log("lvgl app.js loaded");
}
