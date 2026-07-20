/** Watch 启动器 · 蜂窝视图：六边形排布 + 视口中心锚点 + 拖动平移 */

var BUBBLE_D = 48;
var BUBBLE_H = BUBBLE_D * Math.sqrt(3) / 2;
var BUBBLE_CX = 171;
var BUBBLE_CY = 140;
var BUBBLE_CLIP_R = 138;
var BUBBLE_FISHEYE_R = 120;

/** pointy-top 六边形轴向方向（与 ring 遍历顺序一致） */
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

    var apps = WatchAppRegistry.getLauncherApps();
    bubbleApps = apps;
    bubbleSlots = bubbleHexSlots(apps.length);
    bubblePanX = 0;
    bubblePanY = 0;
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

function bubbleHexSlots(count) {
    var slots = [];
    var ring = 1;
    var q, r, side, step;

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

/** 轴向 (q,r) → 像素；相邻圆心距 = BUBBLE_D，与图标直径一致 */
function bubbleAxialToPixel(q, r) {
    return {
        x: BUBBLE_D * (q + r / 2),
        y: BUBBLE_H * r
    };
}

function bubbleFisheyeScale(dist) {
    var t = dist / BUBBLE_FISHEYE_R;
    if (t >= 1) {
        return 0.52;
    }
    return 1 - 0.48 * t * t;
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
    var updates = [];
    var i, app, slot, world, cx, cy, dist, scale, size;

    for (i = 0; i < bubbleApps.length; i++) {
        app = bubbleApps[i];
        slot = bubbleSlots[i] || { q: 0, r: 1 };
        world = bubbleAxialToPixel(slot.q, slot.r);
        cx = BUBBLE_CX + world.x + bubblePanX;
        cy = BUBBLE_CY + world.y + bubblePanY;
        dist = Math.sqrt((cx - BUBBLE_CX) * (cx - BUBBLE_CX) + (cy - BUBBLE_CY) * (cy - BUBBLE_CY));
        scale = bubbleFisheyeScale(dist);
        size = Math.round(BUBBLE_D * scale);

        updates.push({
            target: "launcher_app_" + app.id,
            change: {
                position: [Math.round(cx - size / 2), Math.round(cy - size / 2)],
                size: [size, size],
                visible: scale >= 0.4 && dist <= BUBBLE_CLIP_R + size * 0.55
            }
        });
    }

    YUI.update(updates);
}

function onLauncherTouch(type, deltaX, deltaY) {
    if (launcherMode !== "bubble") {
        return;
    }
    /* 只需处理 move：Button 会吞 DOWN，但 MOVE 会冒泡到 launcher_bubble */
    if (type === "move" && (deltaX !== 0 || deltaY !== 0)) {
        bubblePanX += deltaX;
        bubblePanY += deltaY;
        layoutBubbleIcons();
    }
}

function onLauncherAppClick(layerId) {
    if (!layerId) {
        return;
    }
    if (layerId.indexOf("launcher_app_grid_") === 0) {
        WatchAppRegistry.openById(layerId.substring("launcher_app_grid_".length));
        return;
    }
    if (layerId.indexOf("launcher_app_") === 0) {
        WatchAppRegistry.openById(layerId.substring("launcher_app_".length));
    }
}
