/**
 * 电池详情
 *
 * - 环形电量 + 电压 / 温度 / 循环 / 健康度
 * - 近 12 小时电量柱状图（用 View 填充层绘制，支持运行时更新）
 */

var BATTERY_BAR_COUNT = 12;
var BATTERY_BAR_W = 41;
var BATTERY_BAR_H = 72;
var batteryRingValue = -1;
var batterySaver = false;
var batteryCharts = null;

function onBatteryLoad() {
    YUI.log("[battery] onLoad");
    buildBatteryChart();
    refreshBatteryUI();
}

function onBatteryShow() {
    applyConsoleTheme();
    buildBatteryChart();
    batteryRingValue = -1;
    refreshBatteryUI();
}

/* ==================== 环形电量 ==================== */

function renderBatteryRing() {
    if (batteryRingValue === Console.battery) return;
    batteryRingValue = Console.battery;

    /* Progress 不支持运行时改值，按电量重建该图层 */
    YUI.renderFromJson("battery_ring_box", JSON.stringify({
        id: "battery_ring",
        type: "Progress",
        size: [118, 118],
        shape: "circle",
        value: Console.battery,
        showPercentage: false,
        fillColor: consoleBatteryColor(),
        circleWidth: 9,
        style: { bgColor: "#131B2E", borderRadius: 59 }
    }));
    YUI.show("battery_ring");
}

/* ==================== 电量柱状图 ==================== */

function buildBatteryChart() {
    if (batteryCharts !== null) return;

    YUI.update({ target: "battery_chart_bars", change: { children: null } });

    batteryCharts = [];
    for (var i = 0; i < BATTERY_BAR_COUNT; i++) {
        /* 轨道用垂直布局 + flex-end：填充从底部向上生长，只改 size 不改 position */
        YUI.renderFromJson("battery_chart_bars", JSON.stringify({
            id: "battery_bar_track_" + i,
            type: "View",
            variant: "bar-track",
            size: [BATTERY_BAR_W, BATTERY_BAR_H],
            layout: { type: "vertical", justifyContent: "flex-end" },
            style: { bgColor: "#131B2E", borderRadius: 6 },
            children: [
                {
                    id: "battery_bar_fill_" + i,
                    type: "View",
                    variant: "bar-fill",
                    size: [BATTERY_BAR_W, 2],
                    style: { bgColor: "#34D399", borderRadius: 6 }
                }
            ]
        }), true);
        YUI.show("battery_bar_track_" + i);
        batteryCharts.push(i);
    }
}

/* 由当前电量反推过去 12 小时的采样值（确定性抖动，避免每帧跳动） */
function batterySampleAt(index) {
    var hoursAgo = BATTERY_BAR_COUNT - 1 - index;
    var v = Console.battery + hoursAgo * 6;
    v -= (index % 3) * 3;
    if (v > 100) v = 100;
    if (v < 4) v = 4;
    return v;
}

function batterySampleColor(v) {
    if (v <= 15) return "#FF6B6B";
    if (v <= 35) return "#FACC15";
    return "#34D399";
}

function refreshBatteryChart() {
    if (batteryCharts === null) return;
    var peak = 0;
    for (var i = 0; i < BATTERY_BAR_COUNT; i++) {
        var v = batterySampleAt(i);
        var h = Math.round(BATTERY_BAR_H * v / 100);
        if (h < 2) h = 2;
        consoleSetBar("battery_bar_fill_" + i, BATTERY_BAR_W, h, batterySampleColor(v));
        if (v > peak) peak = v;
    }
    YUI.setText("battery_chart_peak", "峰值 " + peak + "% · 每小时采样");
    YUI.setText("battery_chart_now", "当前 " + Console.battery + "%");
}

/* ==================== 汇总刷新 ==================== */

function refreshBatteryUI() {
    renderBatteryRing();

    YUI.setText("battery_pct", Console.battery + "%");
    YUI.setText("battery_state", Console.charging
        ? "充电中 · 约 25 分钟充满"
        : (batterySaver ? "省电模式" : (Console.battery <= 15 ? "电量偏低" : "正常使用")));
    YUI.setText("battery_estimate", Console.charging
        ? "预计 25 分钟后满电"
        : "剩余约 " + (Math.round(Console.battery * 0.07 * 10) / 10) + " 小时");
    YUI.setText("btn_battery_saver", batterySaver ? "退出省电" : "省电模式");
    YUI.setText("battery_sub", "锂聚合物 4000 mAh · " + Console.wifi
        + " · 存储 " + Console.storage.used + "/" + Console.storage.total + "G");

    YUI.setText("battery_volt_value", Console.voltage.toFixed(2) + " V");
    consoleSetBar("battery_volt_fill",
        consoleBarWidth((Console.voltage - 3.2) / 1.0 * 100, 150), 6, "#38BDF8");

    YUI.setText("battery_temp_value", Console.temperature + "°C");
    consoleSetBar("battery_temp_fill",
        consoleBarWidth((Console.temperature - 20) / 40 * 100, 150), 6, "#FACC15");

    YUI.setText("battery_cycle_value", Console.cycles + " 次");
    consoleSetBar("battery_cycle_fill",
        consoleBarWidth(Console.cycles / 500 * 100, 150), 6, "#34D399");

    var health = 100 - Math.floor(Console.cycles / 50);
    YUI.setText("battery_health_value", health + "%");
    YUI.setText("battery_health_label", "健康度（随循环下降）");
    consoleSetBar("battery_health_fill", consoleBarWidth(health, 150), 6, "#A78BFA");

    YUI.setText("battery_chart_now", "当前 " + Console.battery + "%");
    refreshBatteryChart();

    if (batterySaver) {
        YUI.setText("battery_tip", "省电模式：已降低亮度与音量，暂停后台扫描");
    } else if (Console.battery <= 15) {
        YUI.setText("battery_tip", "电量偏低：建议插电或开启省电模式");
    } else {
        YUI.setText("battery_tip", "低电量（≤15%）时桌面与状态栏会变为红色提示");
    }
}

/* ==================== 操作 ==================== */

function onBatteryToggleCharge() {
    Console.charging = !Console.charging;
    YUI.setText("btn_battery_toggle_charge", Console.charging ? "停止充电" : "模拟充电");
    batteryRingValue = -1;
    updateConsoleChrome();
    refreshBatteryUI();
}

function onBatteryRefresh() {
    batteryRingValue = -1;
    updateConsoleChrome();
    refreshBatteryUI();
    setConsoleHint("电量信息已刷新");
}

function onBatterySaver() {
    batterySaver = !batterySaver;
    if (batterySaver) {
        Console.brightness = 45;
        Console.volume = 30;
    } else {
        Console.brightness = 80;
        Console.volume = 70;
    }
    saveConsoleConfig();
    refreshBatteryUI();
    if (typeof refreshDesktopHero === "function") refreshDesktopHero();
}
