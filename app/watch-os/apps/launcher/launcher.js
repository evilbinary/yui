/** Watch 启动器 · 蜂窝视图：六边形排布 + 中心表盘 + 拖动平移 */

var BUBBLE_D = 48;
var BUBBLE_H = BUBBLE_D * Math.sqrt(3) / 2;
var BUBBLE_V = BUBBLE_D * 1.5;
var BUBBLE_CX = 171;
var BUBBLE_CY = 140;
var BUBBLE_CLIP_R = 138;

var HEX_DIRS = [[1, -1], [1, 0], [0, 1], [-1, 1], [-1, 0], [0, -1]];

var launcherBuilt = false;
var bubbleApps = [];
var bubbleSlots = [];
var bubblePanX = 0;
var bubblePanY = 0;
var bubbleDragging = false;

function onLauncherLoad() {
    rebuildLauncher();
    applyWatchTheme();
}

function onLauncherShow() {
    if (!launcherBuilt) {
        rebuildLauncher();
    }
    layoutBubbleIcons();
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
    launcherBuilt = true;
}

function rebuildLauncherGrid() {
    launcherBuilt = false;
    rebuildLauncher();
}

function bubbleHexSlots(count) {
    var slots = [];
    var ring = 1;
    var q, r, side, step;

    while (slots.length < count) {
        q = 0;
        r = -ring;
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
    var row = r;
    var col = q + ((r - (r & 1)) >> 1);
    return {
        x: col * BUBBLE_H + (row & 1 ? BUBBLE_H * 0.5 : 0),
        y: row * BUBBLE_V
    };
}

function buildLauncherBubble(apps) {
    var i, app, layerId;

    YUI.update({ target: "launcher_bubble", change: { children: null } });

    YUI.renderFromJson("launcher_bubble", JSON.stringify({
        id: "launcher_bubble_face",
        type: "Button",
        variant: "dock-accent",
        text: "◉",
        size: [BUBBLE_D, BUBBLE_D],
        position: [BUBBLE_CX - BUBBLE_D / 2, BUBBLE_CY - BUBBLE_D / 2],
        events: { onClick: "@goWatchBack" }
    }), true);
    YUI.show("launcher_bubble_face");

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

function layoutBubbleIcons() {
    var updates = [];
    var i, app, slot, world, cx, cy, dist;

    updates.push({
        target: "launcher_bubble_face",
        change: {
            position: [Math.round(BUBBLE_CX - BUBBLE_D / 2), Math.round(BUBBLE_CY - BUBBLE_D / 2)],
            size: [BUBBLE_D, BUBBLE_D],
            visible: true
        }
    });

    for (i = 0; i < bubbleApps.length; i++) {
        app = bubbleApps[i];
        slot = bubbleSlots[i] || { q: 0, r: 1 };
        world = bubbleAxialToPixel(slot.q, slot.r);
        cx = BUBBLE_CX + world.x + bubblePanX;
        cy = BUBBLE_CY + world.y + bubblePanY;
        dist = Math.sqrt((cx - BUBBLE_CX) * (cx - BUBBLE_CX) + (cy - BUBBLE_CY) * (cy - BUBBLE_CY));

        updates.push({
            target: "launcher_app_" + app.id,
            change: {
                position: [Math.round(cx - BUBBLE_D / 2), Math.round(cy - BUBBLE_D / 2)],
                size: [BUBBLE_D, BUBBLE_D],
                visible: dist <= BUBBLE_CLIP_R + BUBBLE_D * 0.5
            }
        });
    }

    YUI.update(updates);
}

function onLauncherTouch(type, deltaX, deltaY) {
    if (type === "start") {
        bubbleDragging = true;
        return;
    }
    if (type === "move" && bubbleDragging) {
        bubblePanX += deltaX;
        bubblePanY += deltaY;
        layoutBubbleIcons();
        return;
    }
    if (type === "end" || type === "cancel") {
        bubbleDragging = false;
    }
}

function onLauncherAppClick(layerId) {
    if (!layerId || layerId.indexOf("launcher_app_") !== 0) {
        return;
    }
    WatchAppRegistry.openById(layerId.substring("launcher_app_".length));
}
