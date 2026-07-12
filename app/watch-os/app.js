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
    notifications: [
        { app: "信息", from: "妈妈", text: "晚上回来吃饭吗？", time: "18:32", unread: true },
        { app: "电话", from: "快递", text: "未接来电", time: "14:20", unread: true },
        { app: "健康", from: "活动", text: "今日步数目标已完成", time: "12:00", unread: true },
        { app: "日历", from: "会议", text: "15:00 项目例会", time: "09:15", unread: false }
    ],
    messages: [
        { name: "妈妈", preview: "晚上回来吃饭吗？", time: "18:32", unread: 2, reply: "" },
        { name: "项目组", preview: "明天下午三点例会", time: "15:10", unread: 1, reply: "" },
        { name: "爸爸", preview: "好的，知道了", time: "昨天", unread: 0, reply: "周末见" },
        { name: "Alice", preview: "See you tonight!", time: "周一", unread: 0, reply: "" }
    ],
    contacts: [
        { name: "妈妈", phone: "138****1234", emoji: "👩" },
        { name: "爸爸", phone: "139****5678", emoji: "👨" },
        { name: "姐姐", phone: "136****9012", emoji: "👧" },
        { name: "弟弟", phone: "135****3456", emoji: "👦" },
        { name: "Alice", phone: "+1 *** 7890", emoji: "👩" },
        { name: "Bob", phone: "+1 *** 4321", emoji: "👨" },
        { name: "公司前台", phone: "021-****8888", emoji: "🏢" },
        { name: "出租车", phone: "96123", emoji: "🚕" }
    ],
    sleep: {
        lastNight: { total: 7.5, deep: 2.1, core: 4.1, rem: 1.3, awake: 0.2, bed: "23:15", wake: "06:45" },
        week: [6.8, 7.2, 7.5, 6.5, 8.0, 7.5, 7.5]
    },
    apps: []
};

var clockTimer = null;
var watchThemeDir = "app/watch-os/themes";

function initWatchThemes() {
    Theme.load(watchThemeDir + "/dark.json", "dark");
    Theme.load(watchThemeDir + "/light.json", "light");
    Theme.setCurrent(Watch.themeMode);
    Theme.apply();
}

function initWatchApps() {
    var routes = WatchAppRegistry.init();
    Watch.apps = WatchAppRegistry.getAll();
    return routes;
}

function onWatchLoad() {
    initWatchThemes();

    Router.init({
        outlet: "page_outlet",
        routes: initWatchApps()
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

function getUnreadNotificationCount() {
    var count = 0;
    for (var i = 0; i < Watch.notifications.length; i++) {
        if (Watch.notifications[i].unread) count++;
    }
    return count;
}

function updateWatchChrome() {
    var route = YUI.currentRoute ? YUI.currentRoute() : null;
    var onFace = route && route.path === "/";

    if (onFace) {
        YUI.hide("btn_back");
    } else {
        YUI.show("btn_back");
    }

    var unread = getUnreadNotificationCount();
    YUI.setText("btn_status_notif", unread > 0 ? String(unread) : "🔔");
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
    YUI.setText("health_sleep", String(Watch.sleep.lastNight.total));
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

function openNotifications() { openWatchApp("/notifications"); }
function openBattery() { WatchAppRegistry.openById("battery"); }
function openMessages() { WatchAppRegistry.openById("messages"); }

function rescanWatchApps() {
    WatchAppRegistry.init();
    Router.routes = WatchAppRegistry.getRoutes();
    Watch.apps = WatchAppRegistry.getAll();
    if (typeof buildLauncherGrid === "function") buildLauncherGrid();
    if (typeof refreshFaceDock === "function") refreshFaceDock();
}

function applyWatchTheme() {
    Theme.setCurrent(Watch.themeMode);
    Theme.apply();
}

function getWatchThemeLabel() {
    return Watch.themeMode === "dark" ? "暗色" : "亮色";
}
