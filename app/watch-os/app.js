/**
 * YUI Watch OS - 壳层：路由、主题、状态栏、共享数据
 */

var Watch = {
    battery: 85,
    themeMode: "dark",
    complications: {
        steps: { value: 8432, goal: 10000 },
        heart: { value: 72, unit: "bpm" },
        weather: { temp: 26, cond: "Mostly Clear" }
    },
    apps: [
        { id: "face", path: "/", json: "app/watch-os/apps/face/face.json" },
        { id: "launcher", path: "/launcher", json: "app/watch-os/apps/launcher/launcher.json" },
        { id: "health", path: "/apps/health", json: "app/watch-os/apps/health/health.json" },
        { id: "timer", path: "/apps/timer", json: "app/watch-os/apps/timer/timer.json" },
        { id: "clock", path: "/apps/clock", json: "app/watch-os/apps/clock/clock.json" },
        { id: "alarm", path: "/apps/alarm", json: "app/watch-os/apps/alarm/alarm.json" },
        { id: "stopwatch", path: "/apps/stopwatch", json: "app/watch-os/apps/stopwatch/stopwatch.json" },
        { id: "calc", path: "/apps/calc", json: "app/watch-os/apps/calc/calc.json" },
        { id: "workout", path: "/apps/workout", json: "app/watch-os/apps/workout/workout.json" },
        { id: "phone", path: "/apps/phone", json: "app/watch-os/apps/phone/phone.json" },
        { id: "maps", path: "/apps/maps", json: "app/watch-os/apps/maps/maps.json" },
        { id: "wallet", path: "/apps/wallet", json: "app/watch-os/apps/wallet/wallet.json" },
        { id: "settings", path: "/settings", json: "app/watch-os/apps/settings/settings.json" }
    ],
    upcoming: []
};

var watchRoutes = {
    "/": { json: "app/watch-os/apps/face/face.json", keepAlive: true },
    "/launcher": { json: "app/watch-os/apps/launcher/launcher.json", keepAlive: true },
    "/apps/health": { json: "app/watch-os/apps/health/health.json", keepAlive: true },
    "/apps/timer": { json: "app/watch-os/apps/timer/timer.json", keepAlive: true },
    "/apps/clock": { json: "app/watch-os/apps/clock/clock.json", keepAlive: true },
    "/apps/alarm": { json: "app/watch-os/apps/alarm/alarm.json", keepAlive: true },
    "/apps/stopwatch": { json: "app/watch-os/apps/stopwatch/stopwatch.json", keepAlive: true },
    "/apps/calc": { json: "app/watch-os/apps/calc/calc.json", keepAlive: true },
    "/apps/workout": { json: "app/watch-os/apps/workout/workout.json", keepAlive: true },
    "/apps/phone": { json: "app/watch-os/apps/phone/phone.json", keepAlive: true },
    "/apps/maps": { json: "app/watch-os/apps/maps/maps.json", keepAlive: true },
    "/apps/wallet": { json: "app/watch-os/apps/wallet/wallet.json", keepAlive: true },
    "/settings": { json: "app/watch-os/apps/settings/settings.json", keepAlive: true }
};

var clockTimer = null;

var watchThemeDir = "app/watch-os/themes";

function initWatchThemes() {
    Theme.load(watchThemeDir + "/dark.json", "dark");
    Theme.load(watchThemeDir + "/light.json", "light");
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
    startWatchClock();
}

function onWatchShow() {
    updateWatchChrome();
    tickWatchClock();
}

function startWatchClock() {
    if (clockTimer !== null) return;
    tickWatchClock();
    clockTimer = setTimeout(function loop() {
        tickWatchClock();
        clockTimer = setTimeout(loop, 30000);
    }, 30000);
}

function tickWatchClock() {
    var now = new Date();
    var h = now.getHours();
    var m = now.getMinutes();
    var timeStr = (h < 10 ? "0" : "") + h + ":" + (m < 10 ? "0" : "") + m;
    YUI.setText("status_time", timeStr);

    var route = YUI.currentRoute ? YUI.currentRoute() : null;
    if (route && route.path === "/") {
        updateFaceClock();
        if (typeof updateFaceData === "function") {
            updateFaceData();
        }
    }
}

function updateFaceClock() {
    var now = new Date();
    var h = now.getHours();
    var m = now.getMinutes();
    var timeStr = (h < 10 ? "0" : "") + h + ":" + (m < 10 ? "0" : "") + m;
    YUI.setText("face_time", timeStr);
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
    if (typeof updateFaceData === "function") {
        updateFaceData();
    }
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
function openClock() { openWatchApp("/apps/clock"); }
function openAlarm() { openWatchApp("/apps/alarm"); }
function openStopwatch() { openWatchApp("/apps/stopwatch"); }
function openCalc() { openWatchApp("/apps/calc"); }
function openWorkout() { openWatchApp("/apps/workout"); }
function openPhone() { openWatchApp("/apps/phone"); }
function openMaps() { openWatchApp("/apps/maps"); }
function openWallet() { openWatchApp("/apps/wallet"); }
function openSettings() { openWatchApp("/settings"); }

function applyWatchTheme() {
    Theme.setCurrent(Watch.themeMode);
    Theme.apply();
}

function getWatchThemeLabel() {
    return Watch.themeMode === "dark" ? "暗色" : "亮色";
}
                                                                                                                                                                                                                                                                                                                 