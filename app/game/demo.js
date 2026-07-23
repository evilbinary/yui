/**
 * Jump + shoot demo — V1/V2 features:
 * patrol/fly/slime/boss AI, triggers, particles, audio, pool, multi-level.
 * Controls: A/D or arrows move, Space jump, click or F shoot.
 */
var gScore = 0;
var gShootCool = 0;
var gBulletSeq = 0;
var gCurrentLevel = 0;
var gLevelClearDelay = 0;
var gPendingLevel = -1;
var gVictory = false;
var gEnemyRoster = [];
var gEnemyHp = {};
var gEnemyState = {};

var LEVELS = [
  {
    scene: "app/game/level1.json",
    title: "Level 1 — Training Ground",
    enemies: ["e_patrol_1", "e_fly_1", "e_slime_1"],
    hint: "Crab patrols, bat flies, slime hops."
  },
  {
    scene: "app/game/level2.json",
    title: "Level 2 — Dark Cave",
    enemies: ["e_patrol_1", "e_patrol_2", "e_fly_1", "e_fly_2", "e_boss_1"],
    hint: "Boss needs 3 hits!"
  }
];

var ENEMY_CFG = {};

var LEVEL_ENEMY_CFG = [
  {
    e_patrol_1: { minX: 440, maxX: 700, speed: 70, dir: 1 },
    e_fly_1: { baseY: 360, amp: 22, drift: 28, t: 0 },
    e_slime_1: { hopEvery: 2.2, hopVy: -260 }
  },
  {
    e_patrol_1: { minX: 130, maxX: 240, speed: 75, dir: 1 },
    e_patrol_2: { minX: 520, maxX: 780, speed: 90, dir: -1 },
    e_fly_1: { baseY: 320, amp: 20, drift: 24, t: 0 },
    e_fly_2: { baseY: 340, amp: 26, drift: -30, t: 1.2 },
    e_boss_1: { minX: 660, maxX: 820, speed: 45, dir: 1, hp: 3 }
  }
];

function enemyCfg(id) {
  if (!gEnemyState[id]) {
    gEnemyState[id] = { hopTimer: 1, squash: 0, anim: 0 };
  }
  return ENEMY_CFG[id] || null;
}

function setHudText(id, text) {
  if (typeof YUI !== "undefined" && YUI.setText) {
    YUI.setText(id, text);
  }
}

function setScore(n) {
  gScore = n;
  setHudText("hudScore", "Score: " + gScore);
}

function updateLevelHud() {
  var lvl = LEVELS[gCurrentLevel];
  if (!lvl) {
    return;
  }
  setHudText("hudTitle", lvl.title + "  |  " + lvl.hint);
  setHudText("hudLevel", "Lv " + (gCurrentLevel + 1) + "/" + LEVELS.length);
}

function countLiveEnemies() {
  var list = Game.findAllByTag("enemy");
  return list ? list.length : 0;
}

function initEnemyHp() {
  var i;
  var id;
  var cfg;
  gEnemyHp = {};
  for (i = 0; i < gEnemyRoster.length; i++) {
    id = gEnemyRoster[i];
    cfg = ENEMY_CFG[id];
    gEnemyHp[id] = cfg && cfg.hp ? cfg.hp : 1;
  }
}

function sfx(path) {
  if (Game.audio && Game.audio.play) {
    Game.audio.play(path);
  }
}

function burst(x, y, color) {
  Game.spawnParticles({
    x: x,
    y: y,
    count: 14,
    color: color || "#ff922b",
    speed: 160,
    life: 0.4
  });
}

function loadLevel(index) {
  var lvl;
  if (index < 0 || index >= LEVELS.length) {
    return;
  }
  lvl = LEVELS[index];
  gCurrentLevel = index;
  gLevelClearDelay = 0;
  gEnemyRoster = lvl.enemies.slice();
  gEnemyState = {};
  ENEMY_CFG = LEVEL_ENEMY_CFG[index] || {};
  Game.loadScene(lvl.scene);
  initEnemyHp();
  setScore(gScore);
  updateLevelHud();
  print("Loaded " + lvl.title);
}

function onLevelCleared() {
  sfx("app/game/assets/clear.wav");
  if (gCurrentLevel + 1 < LEVELS.length) {
    setHudText("hudTitle", "Level clear! Next stage...");
    gLevelClearDelay = 1.8;
  } else {
    gVictory = true;
    setHudText("hudTitle", "Victory! All levels cleared.");
    setHudText("hudLevel", "WIN");
    print("You beat the demo!");
  }
}

function damageEnemy(enemy) {
  var id;
  var hp;
  if (!enemy) {
    return;
  }
  id = enemy.id;
  if (!gEnemyHp[id]) {
    gEnemyHp[id] = 1;
  }
  hp = gEnemyHp[id] - 1;
  burst(enemy.x + enemy.w * 0.5, enemy.y + enemy.h * 0.5, "#fa5252");
  sfx("app/game/assets/hit.wav");
  if (hp <= 0) {
    Game.destroy(enemy);
    delete gEnemyHp[id];
    setScore(gScore + (id.indexOf("boss") >= 0 ? 30 : 10));
    if (countLiveEnemies() <= 0) {
      onLevelCleared();
    }
  } else {
    gEnemyHp[id] = hp;
    setScore(gScore + 5);
  }
}

function onTrigger(a, b, phase) {
  var bullet;
  var other;
  if (phase !== "enter") {
    return;
  }
  if (!a || !b) {
    return;
  }
  if (a.tag === "bullet") {
    bullet = a;
    other = b;
  } else if (b.tag === "bullet") {
    bullet = b;
    other = a;
  } else {
    return;
  }
  if (other.tag === "enemy") {
    damageEnemy(other);
    if (Game.pool && Game.pool.release) {
      Game.pool.release(bullet);
    } else {
      Game.destroy(bullet);
    }
  }
}

var gDebugUi = -1;

function updateDebugButton() {
  var on = 0;
  if (Game.debug && Game.debug.boxes) {
    on = Game.debug.boxes() ? 1 : 0;
  }
  gDebugUi = on;
  setHudText("btnDebugBoxes", on ? "Debug On" : "Debug Off");
  if (typeof YUI !== "undefined" && YUI.setStyle) {
    YUI.setStyle("btnDebugBoxes", {
      bgColor: on ? "#2f9e44" : "#495057"
    });
  }
}

function syncDebugButtonIfNeeded() {
  var on = 0;
  if (typeof Game === "undefined" || !Game.debug || !Game.debug.boxes) {
    return;
  }
  on = Game.debug.boxes() ? 1 : 0;
  if (on !== gDebugUi) {
    updateDebugButton();
  }
}

function onToggleDebugBoxes() {
  var on;
  if (typeof Game === "undefined" || !Game.debug || !Game.debug.setBoxes) {
    print("Game.debug API missing");
    return;
  }
  on = Game.debug.boxes() ? 0 : 1;
  Game.debug.setBoxes(on);
  updateDebugButton();
}

function onGameDemoLoad() {
  gScore = 0;
  gVictory = false;
  gBulletSeq = 0;
  if (typeof Game === "undefined") {
    print("Game API missing");
    return;
  }
  Game.onTrigger = onTrigger;
  if (Game.debug && Game.debug.setBoxes) {
    Game.debug.setBoxes(0);
  }
  updateDebugButton();
  loadLevel(0);
  print("A/D move, Space jump, Click/F shoot. Debug button or F3 toggles collider boxes.");
}

function playerUpdate(entity, dt) {
  var speed = 190;
  var gravity = 900;
  var jump = -340;
  var axis = Game.input.axis("Horizontal");
  var ptr;
  var screen;
  var aimx;
  var aimy;
  var len;
  var bullet;

  /* Defer scene loads to the start of a frame so we never reload mid-apply. */
  if (gPendingLevel >= 0) {
    var idx = gPendingLevel;
    gPendingLevel = -1;
    loadLevel(idx);
    return;
  }

  syncDebugButtonIfNeeded();

  if (gVictory) {
    entity.vx = 0;
    return;
  }

  if (gLevelClearDelay > 0) {
    gLevelClearDelay -= dt;
    entity.vx = 0;
    if (gLevelClearDelay <= 0 && gCurrentLevel + 1 < LEVELS.length) {
      gPendingLevel = gCurrentLevel + 1;
    }
    return;
  }

  entity.vx = axis * speed;
  entity.vy += gravity * dt;
  if (Game.input.pressed("Space") && entity.grounded) {
    entity.vy = jump;
  }

  gShootCool -= dt;
  if (gShootCool < 0) {
    gShootCool = 0;
  }
  if ((Game.input.mousePressed(1) || Game.input.pressed("F")) && gShootCool <= 0) {
    ptr = Game.input.pointer();
    screen = Game.camera.worldToScreen(entity.x + entity.w * 0.5, entity.y + entity.h * 0.5);
    aimx = ptr.x - screen.x;
    aimy = ptr.y - screen.y;
    len = Math.sqrt(aimx * aimx + aimy * aimy);
    if (len < 1) {
      aimx = entity.vx >= 0 ? 1 : -1;
      aimy = 0;
      len = 1;
    }
    aimx /= len;
    aimy /= len;
    bullet = Game.spawn({
      id: "bullet_" + (gBulletSeq++),
      tag: "bullet",
      prefab: "bullet",
      script: "bulletUpdate",
      transform: {
        x: entity.x + entity.w * 0.5 - 6,
        y: entity.y + entity.h * 0.35,
        z: 20
      },
      sprite: { src: "app/game/assets/bullet.png", w: 14, h: 14 },
      collider: { w: 10, h: 10, ox: 2, oy: 2, trigger: 1 },
      vx: aimx * 420,
      vy: aimy * 420
    });
    sfx("app/game/assets/shoot.wav");
    gShootCool = 0.18;
  }
}

function bgUpdate(entity, dt) {
  var s;
  /* Pin backdrop to camera so it fills the viewport while scrolling */
  if (!Game.camera || !Game.camera.worldToScreen) {
    return;
  }
  s = Game.camera.worldToScreen(0, 0);
  entity.x = -s.x;
  entity.y = -s.y;
  entity.vx = 0;
  entity.vy = 0;
}

function bulletUpdate(entity, dt) {
  /* hit detection via onTrigger; only cull off-screen here */
  if (entity.x < -200 || entity.x > 1400 || entity.y < -200 || entity.y > 800) {
    if (Game.pool && Game.pool.release) {
      Game.pool.release(entity);
    } else {
      Game.destroy(entity);
    }
  }
}

function patrolEnemyUpdate(entity, dt) {
  var cfg = enemyCfg(entity.id);
  var st;
  if (!cfg) {
    return;
  }
  st = gEnemyState[entity.id];
  st.anim = (st.anim || 0) + dt;
  entity.vy += 900 * dt;
  entity.vx = cfg.dir * cfg.speed;
  if (entity.x <= cfg.minX) {
    entity.x = cfg.minX;
    cfg.dir = 1;
  } else if (entity.x + entity.w >= cfg.maxX) {
    entity.x = cfg.maxX - entity.w;
    cfg.dir = -1;
  }
}

function flyEnemyUpdate(entity, dt) {
  var cfg = enemyCfg(entity.id);
  if (!cfg) {
    return;
  }
  cfg.t += dt * 2.6;
  entity.vy = 0;
  entity.vx = cfg.drift * 0.01;
  entity.y = cfg.baseY + Math.sin(cfg.t) * cfg.amp;
}

function slimeEnemyUpdate(entity, dt) {
  var cfg = enemyCfg(entity.id);
  var st = gEnemyState[entity.id];
  var player = Game.find("player");
  var dx;
  if (!cfg || !st) {
    return;
  }
  entity.vy += 900 * dt;
  st.hopTimer -= dt;
  if (entity.grounded && st.hopTimer <= 0) {
    entity.vy = cfg.hopVy;
    st.hopTimer = cfg.hopEvery + Math.random() * 0.8;
    if (player) {
      dx = player.x - entity.x;
      entity.vx = dx > 0 ? 90 : -90;
    }
    burst(entity.x + entity.w * 0.5, entity.y + entity.h, "#69db7c");
  } else if (entity.grounded) {
    entity.vx *= 0.85;
  }
}

function bossEnemyUpdate(entity, dt) {
  var cfg = enemyCfg(entity.id);
  var st = gEnemyState[entity.id];
  if (!cfg || !st) {
    return;
  }
  patrolEnemyUpdate(entity, dt);
  st.hopTimer -= dt;
  if (entity.grounded && st.hopTimer <= 0) {
    entity.vy = -220;
    st.hopTimer = 3.5;
    burst(entity.x + entity.w * 0.5, entity.y + entity.h, "#ae3ec9");
  }
}
