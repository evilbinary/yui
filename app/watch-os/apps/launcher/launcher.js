/** Watch 启动器 · 浏览态蜂窝（BubbleCloudView 球面投影）
 *
 * 参考 https://github.com/dodola/BubbleCloudView
 * - 六边形正交坐标 + scroll(pan)
 * - 极坐标 → 球面径向 swing / depth 缩放
 * - 再投回正交；边缘再软化 scale
 * - 表冠 zoom；松手惯性 + 橡皮筋边界
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
/** 球面半径：控制近大远小的曲率（越大越平） */
var BUBBLE_SPHERE_R = 160;
var BUBBLE_EDGE = 36;
var BUBBLE_CLIP_PAD = 8;
var BUBBLE_ZOOM_MIN = 0.55;
var BUBBLE_ZOOM_MAX = 1.45;
var BUBBLE_SCROLL_RANGE = 220;

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
var bubblePanRawX = 0;
var bubblePanRawY = 0;
var bubbleZoom = 1;
var bubbleVelX = 0;
var bubbleVelY = 0;
var bubbleInertiaId = null;
var bubbleDispSize = {};
var bubbleSizeAnimId = null;
var BUBBLE_SIZE_LERP = 0.22;

/** 蜂窝测试用：额外 mock 应用数量（设 0 关闭） */
var MOCK_LAUNCHER_COUNT = 48;
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
    bubblePanRawX = 0;
    bubblePanRawY = 0;
    bubbleZoom = 1;
    bubbleDispSize = {};
    bubbleStopSizeAnim();
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

function bubbleEaseInOutCubic(t, b, c, d) {
    t = t / (d / 2);
    if (t < 1) {
        return c / 2 * t * t * t + b;
    }
    t -= 2;
    return c / 2 * (t * t * t + 2) + b;
}

function bubbleSwing(t, b, c, d) {
    t = t / d;
    return -c * t * (t - 2) + b;
}

function bubbleEaseOutSine(t, b, c, d) {
    return c * Math.sin(t / d * (Math.PI / 2)) + b;
}

function bubbleEaseInSine(t, b, c, d) {
    return -c * Math.cos(t / d * (Math.PI / 2)) + c + b;
}

/**
 * BubbleCloudView.iconMapRefresh：正交 → 极坐标 → 球面 → 正交 + depth
 * 返回相对视口中心的屏幕偏移与 scale
 */
function bubbleProjectIcon(worldX, worldY) {
    var ox = (worldX + bubblePanX) * bubbleZoom;
    var oy = (worldY + bubblePanY) * bubbleZoom;
    var r0 = Math.sqrt(ox * ox + oy * oy);
    var rad = Math.atan2(oy, ox);
    var sphereR = BUBBLE_SPHERE_R * bubbleZoom;
    var t;
    var rOut;
    var depth;
    var sx;
    var sy;
    var halfW;
    var halfH;
    var edge;
    var scale;

    if (r0 < 0.001) {
        return { x: 0, y: 0, scale: 1 };
    }

    t = r0 / sphereR;
    if (t < Math.PI / 2) {
        rOut = r0 * bubbleSwing(t / (Math.PI / 2), 1.5, -0.5, 1);
        depth = bubbleEaseInOutCubic(t / (Math.PI / 2), 1, -0.5, 1);
    } else {
        rOut = r0;
        depth = bubbleEaseInOutCubic(1, 1, -0.5, 1);
    }

    sx = rOut * Math.cos(rad);
    sy = rOut * Math.sin(rad) * 1.14;

    halfW = BUBBLE_VP_W / 2;
    halfH = BUBBLE_VP_H / 2;
    edge = BUBBLE_EDGE * bubbleZoom;

    if (Math.abs(sx) > halfW - edge || Math.abs(sy) > halfH - edge) {
        scale = depth * 0.4;
    } else if (Math.abs(sx) > halfW - 2 * edge && Math.abs(sy) > halfH - 2 * edge) {
        scale = Math.min(
            depth * bubbleEaseOutSine(halfW - Math.abs(sx) - edge, 0.4, 0.6, edge),
            depth * bubbleEaseOutSine(halfH - Math.abs(sy) - edge, 0.3, 0.7, edge)
        );
    } else if (Math.abs(sx) > halfW - 2 * edge) {
        scale = depth * bubbleEaseOutSine(halfW - Math.abs(sx) - edge, 0.4, 0.6, edge);
    } else if (Math.abs(sy) > halfH - 2 * edge) {
        scale = depth * bubbleEaseOutSine(halfH - Math.abs(sy) - edge, 0.4, 0.6, edge);
    } else {
        scale = depth;
    }

    if (sx < -halfW + 2 * edge) {
        sx += bubbleEaseInSine(halfW - Math.abs(sx) - 2 * edge, 0, 6, 2 * edge);
    } else if (sx > halfW - 2 * edge) {
        sx += bubbleEaseInSine(halfW - Math.abs(sx) - 2 * edge, 0, -6, 2 * edge);
    }
    if (sy < -halfH + 2 * edge) {
        sy += bubbleEaseInSine(halfH - Math.abs(sy) - 2 * edge, 0, 8, 2 * edge);
    } else if (sy > halfH - 2 * edge) {
        sy += bubbleEaseInSine(halfH - Math.abs(sy) - 2 * edge, 0, -8, 2 * edge);
    }

    if (scale < 0.28) {
        scale = 0.28;
    }
    return { x: sx, y: sy, scale: scale };
}

function bubbleInClip(sx, sy, size) {
    return sx + size * 0.5 > -BUBBLE_CLIP_PAD &&
        sx - size * 0.5 < BUBBLE_VP_W + BUBBLE_CLIP_PAD &&
        sy + size * 0.5 > -BUBBLE_CLIP_PAD &&
        sy - size * 0.5 < BUBBLE_VP_H + BUBBLE_CLIP_PAD;
}

function bubbleApplyRubberPan() {
    var range = BUBBLE_SCROLL_RANGE;
    if (bubblePanRawX > range) {
        bubblePanX = range + (bubblePanRawX - range) / 2;
    } else if (bubblePanRawX < -range) {
        bubblePanX = -range + (bubblePanRawX + range) / 2;
    } else {
        bubblePanX = bubblePanRawX;
    }
    if (bubblePanRawY > range) {
        bubblePanY = range + (bubblePanRawY - range) / 2;
    } else if (bubblePanRawY < -range) {
        bubblePanY = -range + (bubblePanRawY + range) / 2;
    } else {
        bubblePanY = bubblePanRawY;
    }
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
    var i, app, slot, world, proj, sx, sy, targetSize, cur, next, size;
    var settling = false;

    bubbleApplyRubberPan();

    for (i = 0; i < bubbleApps.length; i++) {
        app = bubbleApps[i];
        slot = bubbleSlots[i] || { q: 0, r: 0 };
        world = bubbleAxialToPixel(slot.q, slot.r);
        proj = bubbleProjectIcon(world.x, world.y);
        sx = BUBBLE_CX + proj.x;
        sy = BUBBLE_CY + proj.y;
        targetSize = BUBBLE_D * bubbleZoom * proj.scale;
        cur = bubbleDispSize[app.id];
        if (typeof cur !== "number") {
            cur = targetSize;
        }
        next = cur + (targetSize - cur) * BUBBLE_SIZE_LERP;
        if (Math.abs(targetSize - next) < 0.5) {
            next = targetSize;
        } else {
            settling = true;
        }
        bubbleDispSize[app.id] = next;
        size = Math.round(next);
        if (size < 1) {
            size = 1;
        }

        updates.push({
            target: "launcher_app_" + app.id,
            change: {
                position: [Math.round(sx - size / 2), Math.round(sy - size / 2)],
                size: [size, size],
                visible: bubbleInClip(sx, sy, size)
            }
        });
    }

    YUI.update(updates);
    if (settling) {
        bubbleScheduleSizeAnim();
    }
}

function bubbleStopSizeAnim() {
    if (bubbleSizeAnimId !== null) {
        clearTimeout(bubbleSizeAnimId);
        bubbleSizeAnimId = null;
    }
}

function bubbleScheduleSizeAnim() {
    if (bubbleSizeAnimId !== null) {
        return;
    }
    bubbleSizeAnimId = setTimeout(function() {
        bubbleSizeAnimId = null;
        if (launcherMode === "bubble" && launcherBuilt) {
            layoutBubbleIcons();
        }
    }, 16);
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
        bubbleSettlePan();
        return;
    }
    function step() {
        bubblePanRawX += bubbleVelX;
        bubblePanRawY += bubbleVelY;
        bubbleVelX *= 0.86;
        bubbleVelY *= 0.86;
        if (bubblePanRawX > BUBBLE_SCROLL_RANGE) {
            bubblePanRawX -= (bubblePanRawX - BUBBLE_SCROLL_RANGE) / 4;
            bubbleVelX *= 0.7;
        } else if (bubblePanRawX < -BUBBLE_SCROLL_RANGE) {
            bubblePanRawX -= (bubblePanRawX + BUBBLE_SCROLL_RANGE) / 4;
            bubbleVelX *= 0.7;
        }
        if (bubblePanRawY > BUBBLE_SCROLL_RANGE) {
            bubblePanRawY -= (bubblePanRawY - BUBBLE_SCROLL_RANGE) / 4;
            bubbleVelY *= 0.7;
        } else if (bubblePanRawY < -BUBBLE_SCROLL_RANGE) {
            bubblePanRawY -= (bubblePanRawY + BUBBLE_SCROLL_RANGE) / 4;
            bubbleVelY *= 0.7;
        }
        layoutBubbleIcons();
        if (Math.abs(bubbleVelX) > 0.4 || Math.abs(bubbleVelY) > 0.4) {
            bubbleInertiaId = setTimeout(step, 16);
        } else {
            bubbleInertiaId = null;
            bubbleVelX = 0;
            bubbleVelY = 0;
            bubbleSettlePan();
        }
    }
    bubbleInertiaId = setTimeout(step, 16);
}

function bubbleSettlePan() {
    if (bubblePanRawX > BUBBLE_SCROLL_RANGE) {
        bubblePanRawX = BUBBLE_SCROLL_RANGE;
    } else if (bubblePanRawX < -BUBBLE_SCROLL_RANGE) {
        bubblePanRawX = -BUBBLE_SCROLL_RANGE;
    }
    if (bubblePanRawY > BUBBLE_SCROLL_RANGE) {
        bubblePanRawY = BUBBLE_SCROLL_RANGE;
    } else if (bubblePanRawY < -BUBBLE_SCROLL_RANGE) {
        bubblePanRawY = -BUBBLE_SCROLL_RANGE;
    }
    bubblePanX = bubblePanRawX;
    bubblePanY = bubblePanRawY;
    layoutBubbleIcons();
}

function onLauncherTouch(type, deltaX, deltaY) {
    if (launcherMode !== "bubble") {
        return;
    }
    if (type === "move" && (deltaX !== 0 || deltaY !== 0)) {
        bubbleStopInertia();
        bubblePanRawX += deltaX / bubbleZoom;
        bubblePanRawY += deltaY / bubbleZoom;
        bubbleVelX = deltaX / bubbleZoom;
        bubbleVelY = deltaY / bubbleZoom;
        layoutBubbleIcons();
        return;
    }
    if (type === "wheel") {
        bubbleStopInertia();
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
