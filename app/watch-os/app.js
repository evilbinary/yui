/**
 * YUI Watch OS - 壳层：路由、主题、状态栏、共享数据
 */

var Watch = {
    battery: 85,
    themeMode: "dark",
    complications: {
        steps: { value: 8432, goal: 10000 },
        heart: { value: 72, unit: "bpm" },
        weather: { temp: 26, cond: "晴" }
    },
    apps: [
        { id: "face", path: "/", json: "app/watch-os/apps/face/face.json" },
        { id: "launcher", path: "/launcher", json: "app/watch-os/apps/launcher/launcher.json" },
        { id: "health", path: "/apps/health", json: "app/watch-os/apps/health/health.json" },
        { id: "timer", path: "/apps/timer", json: "app/watch-os/apps/timer/timer.json" },
        { id: "calc", path: "/apps/calc", json: "app/watch-os/apps/calc/calc.json" },
        { id: "settings", path: "/settings", json: "app/watch-os/apps/settings/settings.json" }
    ],
    upcoming: [
        { icon: "🏃", name: "训练" },
        { icon: "📞", name: "电话" },
        { icon: "🗺", name: "地图" },
        { icon: "💳", name: "钱包" }
    ]
};

var watchRoutes = {
    "/": { json: "app/watch-os/apps/face/face.json", keepAlive: true },
    "/launcher": { json: "app/watch-os/apps/launcher/launcher.json", keepAlive: true },
    "/apps/health": { json: "app/watch-os/apps/health/health.json", keepAlive: true },
    "/apps/timer": { json: "app/watch-os/apps/timer/timer.json", keepAlive: true },
    "/apps/calc": { json: "app/watch-os/apps/calc/calc.json", keepAlive: true },
    "/settings": { json: "app/watch-os/apps/settings/settings.json", keepAlive: true }
};

var themePlatform = "watch";

function initWatchThemes() {
    var suffix = themePlatform === "watch" ? "-watch" : "";
    Theme.load("app/lib/themes/dark" + suffix + ".json", "dark");
    Theme.load("app/lib/themes/light" + suffix + ".json", "light");
    Theme.setCurrent(Watch.themeMode);
    Theme.apply();
}

function onWatchLoad() {
    initWatchThemes();

    Router.init({
        outlet: "page_outlet",
        routes: watchRoutes
    });

    YUI.navigate("/");
    updateWatchChrome();
    tickWatchClock();
}

function onWatchShow() {
    updateWatchChrome();
    tickWatchClock();
}

function tickWatchClock() {
    var now = new Date();
    var h = now.getHours();
    var m = now.getMinutes();
    var timeStr = (h < 10 ? "0" : "") + h + ":" + (m < 10 ? "0" : "") + m;
    YUI.setText("status_time", timeStr);
}

function updateFaceClock() {
    var now = new Date();
    var h = now.getHours();
    var m = now.getMinutes();
    var timeStr = (h < 10 ? "0" : "") + h + ":" + (m < 10 ? "0" : "") + m;
    YUI.setText("face_time", timeStr);

    var days = ["周日", "周一", "周二", "周三", "周四", "周五", "周六"];
    YUI.setText("face_date", days[now.getDay()] + " " + (now.getMonth() + 1) + "/" + now.getDate());
}

function updateWatchChrome() {
    var route = YUI.currentRoute ? YUI.currentRoute() : null;
    var onFace = route && route.path === "/";

    if (onFace) {
        YUI.hide("btn_back");
    } else {
        YUI.show("btn_back");
    }

    YUI.setText("status_battery", Watch.battery + "%");
}

function formatWatchNumber(n) {
    if (n >= 1000) {
        return String(n).replace(/\B(?=(\d{3})+(?!\d))/g, ",");
    }
    return String(n);
}

function refreshFaceComplications() {
    var c = Watch.complications;
    YUI.setText("comp_steps_val", formatWatchNumber(c.steps.value));
    YUI.setText("comp_heart_val", String(c.heart.value));
    YUI.setText("comp_weather_val", c.weather.temp + "°");
    YUI.setText("comp_weather_sub", c.weather.cond);

    var pct = Math.min(100, Math.round((c.steps.value / c.steps.goal) * 100));
    YUI.setText("face_ring_label", "活动 " + pct + "%");
}

function refreshHealthData() {
    var c = Watch.complications;
    var pct = Math.min(100, Math.round((c.steps.value / c.steps.goal) * 100));
    YUI.setText("health_steps", formatWatchNumber(c.steps.value));
    YUI.setText("health_heart", String(c.heart.value));
    YUI.setText("health_calories", "342");
    YUI.setText("health_sleep", "7.5");
    YUI.setText("health_ring_text", pct + "% · 还需 " + formatWatchNumber(Math.max(0, c.steps.goal - c.steps.value)) + " 步");
}

function goWatchBack() {
    if (YUI.canBack()) {
        YUI.back();
    } else {
        YUI.navigate("/");
    }
    updateWatchChrome();
    Theme.apply();
}

function openLauncher() {
    YUI.navigate("/launcher");
    updateWatchChrome();
}

function openWatchApp(path) {
    YUI.navigate(path);
    updateWatchChrome();
}

function openHealth() { openWatchApp("/apps/health"); }
function openTimer() { openWatchApp("/apps/timer"); }
function openCalc() { openWatchApp("/apps/calc"); }
function openSettings() { openWatchApp("/settings"); }

function applyWatchTheme() {
    Theme.setCurrent(Watch.themeMode);
    Theme.apply();
}

function getWatchThemeLabel() {
    return Watch.themeMode === "dark" ? "暗色" : "亮色";
}
