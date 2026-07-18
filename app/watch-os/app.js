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
var swipeAnimating = false;

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
    updateSwipeIndex(route ? route.path : "/");
}

function updateSwipeIndex(path) {
    var active = -1;
    if (path === "/notifications") active = 0;
    else if (path === "/") active = 1;
    else if (path === "/launcher") active = 2;

    if (active < 0) {
        YUI.hide("swipe_index");
        return;
    }

    YUI.show("swipe_index");
    YUI.setText("swipe_dot_left", active === 0 ? "●" : "•");
    YUI.setText("swipe_dot_center", active === 1 ? "●" : "•");
    YUI.setText("swipe_dot_right", active === 2 ? "●" : "•");
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

function isShellPath(path) {
    return path === "/" || path === "/launcher" || path === "/notifications";
}

function lockSwipe(ms) {
    swipeAnimating = true;
    setTimeout(function() {
        swipeAnimating = false;
    }, ms || 350);
}

// 按进入路径返回：表盘进 → 回表盘；启动器进 → 回启动器（不写死目标）
function exitWatchApp() {
    var route = YUI.currentRoute ? YUI.currentRoute() : null;
    var fromPath = route ? route.path : "?";
    YUI.log("[backToLauncher] from=" + fromPath);

    if (!route || isShellPath(route.path)) {
        updateWatchChrome();
        Theme.apply();
        return;
    }

    if (typeof Router === "undefined" || !Router.canBack()) {
        YUI.navigate("/");
        updateWatchChrome();
        Theme.apply();
        return;
    }

    var leaving = Router.stack.pop();
    if (leaving && leaving.layerId) {
        YUI.hide(leaving.layerId);
    }

    var returning = Router.current();
    if (returning && returning.layerId) {
        if (typeof Router._showEntry === "function") {
            Router._showEntry(returning, leaving ? leaving.layerId : null);
        } else {
            YUI.show(returning.layerId);
        }
        YUI.log("[exitWatchApp] -> " + returning.path);
    } else {
        YUI.navigate("/");
        YUI.log("[exitWatchApp] -> / (fallback)");
    }

    updateWatchChrome();
    Theme.apply();
}

function goWatchBack() {
    var route = YUI.currentRoute ? YUI.currentRoute() : null;
    var path = route ? route.path : "/";
    var stackPaths = [];
    if (typeof Router !== "undefined" && Router.stack) {
        for (var i = 0; i < Router.stack.length; i++) {
            stackPaths.push(Router.stack[i].path);
        }
    }
    YUI.log("[goWatchBack] path=" + path + " stack=[" + stackPaths.join(" > ") + "]");

    if (!isShellPath(path)) {
        exitWatchApp();
        return;
    }

    if (path === "/launcher" || path === "/notifications") {
        if (YUI.canBack()) {
            YUI.back();
        } else {
            YUI.navigate("/");
        }
    }
    route = YUI.currentRoute ? YUI.currentRoute() : null;
    YUI.log("[goWatchBack] after -> " + (route ? route.path : "?"));
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

function onPageTouch(type, deltaX, deltaY) {
    if (type !== "swipe") {
        return;
    }
    if (swipeAnimating) {
        YUI.log("[swipe] ignored: locked dir=" + (deltaX < 0 ? "left" : "right")
            + " deltaX=" + deltaX + " path=" + ((YUI.currentRoute && YUI.currentRoute()) ? YUI.currentRoute().path : "?"));
        return;
    }

    var route = YUI.currentRoute ? YUI.currentRoute() : null;
    var path = route ? route.path : "/";
    var direction = deltaX < 0 ? "left" : "right";
    var stackPaths = [];
    if (typeof Router !== "undefined" && Router.stack) {
        for (var i = 0; i < Router.stack.length; i++) {
            stackPaths.push(Router.stack[i].path);
        }
    }

    YUI.log("[swipe] dir=" + direction + " deltaX=" + deltaX + " deltaY=" + deltaY
        + " path=" + path + " stack=[" + stackPaths.join(" > ") + "]");

    // 应用内右滑：回到进入前的页面（表盘或启动器），并锁手势防二次 swipe
    if (!isShellPath(path) && direction === "right") {
        YUI.log("[swipe] action=exitWatchApp");
        lockSwipe(400);
        exitWatchApp();
        return;
    }

    if (path === "/") {
        if (direction === "left") {
            YUI.log("[swipe] action=face->launcher");
            swipeNavigate("/launcher", "left");
        } else if (direction === "right") {
            YUI.log("[swipe] action=face->notifications");
            swipeNavigate("/notifications", "right");
        }
        return;
    }
    if (path === "/launcher" && direction === "right") {
        YUI.log("[swipe] action=launcher->face");
        swipeNavigate("/", "right");
        return;
    }
    if (path === "/notifications" && direction === "left") {
        YUI.log("[swipe] action=notifications->face");
        swipeNavigate("/", "left");
        return;
    }

    YUI.log("[swipe] action=none");
}

function swipeNavigate(path, direction) {
    var startX = direction === "left" ? 390 : -390;
    var layerId = routeLayerId(path);

    swipeAnimating = true;
    YUI.navigate(path);
    updateWatchChrome();
    YUI.update({
        target: layerId,
        change: {
            position: [startX, 24],
            animation: {
                duration: 0.28,
                easing: "easeOut",
                fillMode: "forwards",
                x: 0,
                autoPlay: true
            }
        }
    });
    setTimeout(function() {
        swipeAnimating = false;
    }, 320);
}

function routeLayerId(path) {
    if (path === "/notifications") return "page_notifications";
    if (path === "/launcher") return "page_launcher";
    return "page_face";
}

function rescanWatchApps() {
    WatchAppRegistry.init();
    Router.routes = WatchAppRegistry.getRoutes();
    Watch.apps = WatchAppRegistry.getAll();
    if (typeof rebuildLauncherGrid === "function") rebuildLauncherGrid();
    if (typeof refreshFaceDock === "function") refreshFaceDock();
}

function applyWatchTheme() {
    Theme.setCurrent(Watch.themeMode);
    Theme.apply();
}

function getWatchThemeLabel() {
    return Watch.themeMode === "dark" ? "暗色" : "亮色";
}
