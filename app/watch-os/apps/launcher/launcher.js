/** Watch 启动器 · 浏览态蜂窝（刚性网格 + 视口透视）
 *
 * 相对位置锁死；pan 平移整盘；表冠 zoom 以视口中心为锚；
 * 每帧按图标到视口中心距离重算 scale（近大远小）。
 */

var BUBBLE_D = 48;
/** 相邻 App 边与边之间的逻辑边距（改这个即可调疏密） */
var BUBBLE_MARGIN = 14;
var BUBBLE_PITCH = BUBBLE_D + BUBBLE_MARGIN;
var BUBBLE_H = BUBBLE_PITCH * Math.sqrt(3) / 2;
var BUBBLE_VP_W = 380;
var BUBBLE_VP_H = 380;
var BUBBLE_CX = BUBBLE_VP_W / 2;
var BUBBLE_CY = BUBBLE_VP_H / 2;
var BUBBLE_CLIP_R = 145;
var BUBBLE_FISHEYE_R = 150;
var BUBBLE_ZOOM_MIN = 0.55;
var BUBBLE_ZOOM_MAX = 1.45;

var HEX_DIRS = [[1, 0], [1, -1], [0, -1], [-1, 0], [-1, 1], [0, 1]];

var LAUNCHER_COLS = 4;
var LAUNCHER_BTN = 58;
var LAUNCHER_SPACING = 10;

var launcherMode = "bubble";
var launcherBuilt = false;
var bubbleApps = [];
var bubbleSlots = [];
var bubblePanX = 0;
var bubblePanY = 0;
var bubbleZoom = 1;
var bubbleVelX = 0;
var bubbleVelY = 0;
var bubbleInertiaId = null;

/** 蜂窝测试用：额外 mock 应用数量（设 0 关闭） */
var MOCK_LAUNCHER_COUNT = 24;
var MOCK_LAUNCHER_ICONS = [
    "🎵", "📷", "📚", "🎮", "✈️", "🏠", "💡", "🔑",
    "🎁", "🧩", "🎯", "🚀", "🌙", "⭐", "🔥", "💎",
    "🎧", "📺", "🗺", "🌧", "🍕", "⚽", "🛠", "📦"
];

function getMockLauncherApps() {
    var list = [];
    var i;
    if (MOCK_LAUNCHER_COUNT <= 0) {
        return list;
    }
    for (i = 0; i < MOCK_LAUNCHER_COUNT; i++) {
        list.push({
            id: "mock_" + i,
            icon: MOCK_LAUNCHER_ICONS[i % MOCK_LAUNCHER_ICONS.length],
            title: "Mock " + (i + 1),
            launcher: true,
            mock: true
        });
    }
    return list;
}

function onLauncherLoad() {
    rebuildLauncher();
    setLauncherMode(getWatchLauncherMode());
    applyWatchTheme();
}

function onLauncherShow() {
    if (!launcherBuilt) {
        rebuildLauncher();
    }
    setLauncherMode(getWatchLauncherMode());
    applyWatchTheme();
}

function setLauncherMode(mode) {
    var isBubble = mode === "bubble";
    launcherMode = mode;
    YUI.update([
        { target: "page_launcher", change: { scrollable: isBubble ? 0 : 1 } },
        { target: "launcher_bubble", change: { visible: isBubble } },
        { target: "launcher_grid", change: { visible: !isBubble } },
        { target: "launcher_title", change: { text: isBubble ? "APPS · 蜂窝" : "APPS · 网格" } }
    ]);
    if (isBubble) {
        layoutBubbleIcons();
    }
    applyWatchTheme();
}

function rebuildLauncher() {
    if (typeof WatchAppRegistry === "undefined") {
        return;
    }

    var apps = bubblePreferCenterClock(
        WatchAppRegistry.getLauncherApps().concat(getMockLauncherApps())
    );
    bubbleApps = apps;
    bubbleSlots = bubbleHexSlots(apps.length);
    bubblePanX = 0;
    bubblePanY = 0;
    bubbleZoom = 1;
    bubbleStopInertia();
    YUI.setText("launcher_count", apps.length + " 个应用");

    buildLauncherBubble(apps);
    buildLauncherGrid(apps);
    launcherBuilt = true;
}

function rebuildLauncherGrid() {
    launcherBuilt = false;
    rebuildLauncher();
    setLauncherMode(getWatchLauncherMode());
}

/** 时钟 App 固定占蜂窝中心锚点（若存在） */
function bubblePreferCenterClock(apps) {
    var list = apps.slice();
    var i;
    for (i = 0; i < list.length; i++) {
        if (list[i].id === "clock") {
            if (i !== 0) {
                list.unshift(list.splice(i, 1)[0]);
            }
            break;
        }
    }
    return list;
}

function bubbleHexSlots(count) {
    var slots = [];
    var ring = 1;
    var q, r, side, step;

    if (count <= 0) {
        return slots;
    }
    slots.push({ q: 0, r: 0 });
    if (count === 1) {
        return slots;
    }

    while (slots.length < count) {
        q = -ring;
        r = ring;
        for (side = 0; side < 6; side++) {
            for (step = 0; step < ring; step++) {
                if (slots.length >= count) {
                    return slots;
                }
                slots.push({ q: q, r: r });
                q += HEX_DIRS[side][0];
                r += HEX_DIRS[side][1];
            }
        }
        ring++;
    }
    return slots;
}

function bubbleAxialToPixel(q, r) {
    return {
        x: BUBBLE_PITCH * (q + r / 2),
        y: BUBBLE_H * r
    };
}

/** 运行时改边距并刷新布局（单位：逻辑像素） */
function setBubbleMargin(margin) {
    if (typeof margin !== "number" || margin < 0) {
        return;
    }
    BUBBLE_MARGIN = margin;
    BUBBLE_PITCH = BUBBLE_D + BUBBLE_MARGIN;
    BUBBLE_H = BUBBLE_PITCH * Math.sqrt(3) / 2;
    if (launcherBuilt && launcherMode === "bubble") {
        layoutBubbleIcons();
    }
}

/** 视口位置透视：指数衰减，中心 1 → 边缘 ~0.55 */
function bubblePerspective(dist) {
    var t = dist / BUBBLE_FISHEYE_R;
    if (t > 1) {
        t = 1;
    }
    /* t^0.8：外圈掉得更快，中心更突出 */
    t = Math.pow(t, 0.8);
    return 1 - t * 0.45;
}

function bubbleInClip(sx, sy, size) {
    var d = Math.sqrt((sx - BUBBLE_CX) * (sx - BUBBLE_CX) + (sy - BUBBLE_CY) * (sy - BUBBLE_CY));
    return d < BUBBLE_CLIP_R + size * 0.55;
}

function buildLauncherBubble(apps) {
    var i, app, layerId;

    YUI.update({ target: "launcher_bubble", change: { children: null } });

    for (i = 0; i < apps.length; i++) {
        app = apps[i];
        layerId = "launcher_app_" + app.id;
        YUI.renderFromJson("launcher_bubble", JSON.stringify({
            id: layerId,
            type: "Button",
            variant: "dock-flat",
            text: app.icon,
            size: [BUBBLE_D, BUBBLE_D],
            position: [0, 0],
            events: { onClick: "@onLauncherAppClick" }
        }), true);
        YUI.show(layerId);
    }

    layoutBubbleIcons();
}

function buildLauncherGrid(apps) {
    var rows = Math.max(1, Math.ceil(apps.length / LAUNCHER_COLS));
    var gridHeight = rows * LAUNCHER_BTN + (rows - 1) * LAUNCHER_SPACING;
    var i, app, layerId;

    YUI.update({
        target: "launcher_grid",
        change: { width: 342, height: gridHeight, children: null }
    });

    for (i = 0; i < apps.length; i++) {
        app = apps[i];
        layerId = "launcher_app_grid_" + app.id;
        YUI.renderFromJson("launcher_grid", JSON.stringify({
            id: layerId,
            type: "Button",
            variant: "dock-flat",
            text: app.icon,
            size: [LAUNCHER_BTN, LAUNCHER_BTN],
            events: { onClick: "@onLauncherAppClick" }
        }), true);
        YUI.show(layerId);
    }
}

function layoutBubbleIcons() {
    var n = bubbleApps.length;
    var items = [];
    var updates = [];
    var i, j, app, slot, world, lx, ly, dist, persp, size, sx, sy;
    var dx, dy, d, minDist, push, nx, ny, gap, iter;

    for (i = 0; i < n; i++) {
        app = bubbleApps[i];
        slot = bubbleSlots[i] || { q: 0, r: 0 };
        world = bubbleAxialToPixel(slot.q, slot.r);
        lx = (world.x + bubblePanX) * bubbleZoom;
        ly = (world.y + bubblePanY) * bubbleZoom;
        dist = Math.sqrt(lx * lx + ly * ly);
        persp = bubblePerspective(dist);
        /* 先按透视收拢；后面用互推修正重叠 */
        sx = BUBBLE_CX + lx * persp;
        sy = BUBBLE_CY + ly * persp;
        size = Math.round(BUBBLE_D * bubbleZoom * persp);
        items.push({ id: app.id, x: sx, y: sy, size: size });
    }

    gap = BUBBLE_MARGIN * bubbleZoom;
    for (iter = 0; iter < 6; iter++) {
        for (i = 0; i < n; i++) {
            for (j = i + 1; j < n; j++) {
                dx = items[j].x - items[i].x;
                dy = items[j].y - items[i].y;
                d = Math.sqrt(dx * dx + dy * dy);
                minDist = (items[i].size + items[j].size) * 0.5 + gap;
                if (d < 0.001) {
                    dx = 0.01 * (i + 1);
                    dy = 0.01 * (j + 1);
                    d = Math.sqrt(dx * dx + dy * dy);
                }
                if (d < minDist) {
                    push = (minDist - d) * 0.5;
                    nx = dx / d;
                    ny = dy / d;
                    items[i].x -= nx * push;
                    items[i].y -= ny * push;
                    items[j].x += nx * push;
                    items[j].y += ny * push;
                }
            }
        }
    }

    for (i = 0; i < n; i++) {
        updates.push({
            target: "launcher_app_" + items[i].id,
            change: {
                position: [
                    Math.round(items[i].x - items[i].size / 2),
                    Math.round(items[i].y - items[i].size / 2)
                ],
                size: [items[i].size, items[i].size],
                visible: bubbleInClip(items[i].x, items[i].y, items[i].size)
            }
        });
    }

    YUI.update(updates);
}

function bubbleStopInertia() {
    if (bubbleInertiaId !== null) {
        clearTimeout(bubbleInertiaId);
        bubbleInertiaId = null;
    }
    bubbleVelX = 0;
    bubbleVelY = 0;
}

function bubbleStartInertia() {
    bubbleStopInertia();
    if (Math.abs(bubbleVelX) < 0.5 && Math.abs(bubbleVelY) < 0.5) {
        return;
    }
    function step() {
        bubblePanX += bubbleVelX;
        bubblePanY += bubbleVelY;
        bubbleVelX *= 0.86;
        bubbleVelY *= 0.86;
        layoutBubbleIcons();
        if (Math.abs(bubbleVelX) > 0.4 || Math.abs(bubbleVelY) > 0.4) {
            bubbleInertiaId = setTimeout(step, 16);
        } else {
            bubbleInertiaId = null;
            bubbleVelX = 0;
            bubbleVelY = 0;
        }
    }
    bubbleInertiaId = setTimeout(step, 16);
}

function onLauncherTouch(type, deltaX, deltaY) {
    if (launcherMode !== "bubble") {
        return;
    }
    if (type === "move" && (deltaX !== 0 || deltaY !== 0)) {
        bubbleStopInertia();
        bubblePanX += deltaX / bubbleZoom;
        bubblePanY += deltaY / bubbleZoom;
        bubbleVelX = deltaX / bubbleZoom;
        bubbleVelY = deltaY / bubbleZoom;
        layoutBubbleIcons();
        return;
    }
    if (type === "wheel") {
        bubbleStopInertia();
        /* 表冠：以视口中心缩放整盘；中心锚点屏幕位置不变 */
        bubbleZoom += deltaY > 0 ? -0.06 : 0.06;
        if (bubbleZoom < BUBBLE_ZOOM_MIN) {
            bubbleZoom = BUBBLE_ZOOM_MIN;
        }
        if (bubbleZoom > BUBBLE_ZOOM_MAX) {
            bubbleZoom = BUBBLE_ZOOM_MAX;
        }
        layoutBubbleIcons();
        return;
    }
    if (type === "end" || type === "cancel") {
        bubbleStartInertia();
    }
}

function onLauncherAppClick(layerId) {
    if (!layerId) {
        return;
    }
    var id = null;
    if (layerId.indexOf("launcher_app_grid_") === 0) {
        id = layerId.substring("launcher_app_grid_".length);
    } else if (layerId.indexOf("launcher_app_") === 0) {
        id = layerId.substring("launcher_app_".length);
    }
    if (!id) {
        return;
    }
    if (id.indexOf("mock_") === 0) {
        YUI.log("Launcher mock app: " + id);
        return;
    }
    WatchAppRegistry.openById(id);
}
