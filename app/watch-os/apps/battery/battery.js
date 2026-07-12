/**
 * 电量详情 App
 */

var batterySaverOn = false;

function onBatteryLoad() {
    refreshBatteryUI();
}

function onBatteryShow() {
    applyWatchTheme();
    refreshBatteryUI();
}

function refreshBatteryUI() {
    var pct = Watch.battery;
    YUI.setText("battery_pct", pct + "%");
    YUI.setProperty("battery_ring", "value", pct);

    var remainH = Math.round(pct * 0.22);
    YUI.setText("battery_remain", remainH + "h");
    YUI.setText("battery_since", "6h 12m");

    if (batterySaverOn) {
        YUI.setText("battery_status", "省电模式");
        YUI.setText("battery_mode_desc", "已开启 · 后台刷新已关闭");
        YUI.setText("btn_battery_saver", "关闭省电");
    } else if (pct <= 20) {
        YUI.setText("battery_status", "电量不足");
        YUI.setText("battery_mode_desc", "建议开启低电量模式");
        YUI.setText("btn_battery_saver", "省电模式");
    } else {
        YUI.setText("battery_status", "正常使用");
        YUI.setText("battery_mode_desc", "关闭后台刷新，延长续航");
        YUI.setText("btn_battery_saver", "省电模式");
    }

    updateWatchChrome();
}

function toggleBatterySaver() {
    batterySaverOn = !batterySaverOn;
    if (batterySaverOn && Watch.battery > 30) {
        Watch.battery = Math.max(20, Watch.battery - 5);
    }
    refreshBatteryUI();
}
