/**
 * Jump + shoot vertical slice for Mini Game Engine V0.
 * Controls: A/D or Left/Right move, Space jump, mouse click or F shoot.
 */
var gScore = 0;
var gShootCool = 0;
var gBulletSeq = 0;

function setScore(n) {
  gScore = n;
  if (typeof YUI !== "undefined" && YUI.setText) {
    YUI.setText("hudScore", "Score: " + gScore);
  }
}

function onGameDemoLoad() {
  setScore(0);
  if (typeof Game === "undefined") {
    print("Game API missing");
    return;
  }
  Game.loadScene("app/game/level1.json");
  print("Game demo loaded. A/D move, Space jump, Click/F shoot.");
}

function playerUpdate(entity, dt) {
  var speed = 190;
  var gravity = 900;
  var jump = -340;
  var axis = Game.input.axis("Horizontal");
  var ptr;
  var screen;
  var dx;
  var bullet;
  var aimx;
  var aimy;
  var len;

  entity.vx = axis * speed;
  entity.vy += gravity * dt;
  if (Game.input.pressed("Space") && entity.grounded) {
    entity.vy = jump;
  }

  gShootCool -= dt;
  if (gShootCool < 0) gShootCool = 0;
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
      script: "bulletUpdate",
      transform: {
        x: entity.x + entity.w * 0.5,
        y: entity.y + entity.h * 0.4,
        z: 20
      },
      sprite: { w: 8, h: 8, color: "#ffd43b" },
      collider: { w: 8, h: 8, trigger: 1 },
      vx: aimx * 420,
      vy: aimy * 420
    });
    gShootCool = 0.18;
  }
}

function bulletUpdate(entity, dt) {
  var enemy = Game.findByTag("enemy");
  var n = 0;
  var i;
  /* scan all enemies via repeated findByTag only finds first — use overlaps with known ids */
  var ids = ["enemy1", "enemy2", "enemy3"];
  for (i = 0; i < ids.length; i++) {
    enemy = Game.find(ids[i]);
    if (enemy && Game.overlaps(entity, enemy)) {
      Game.destroy(enemy);
      Game.destroy(entity);
      setScore(gScore + 1);
      return;
    }
  }
  if (entity.x < -200 || entity.x > 1200 || entity.y < -200 || entity.y > 800) {
    Game.destroy(entity);
  }
}

function enemyUpdate(entity, dt) {
  /* idle target */
  entity.vx = 0;
  entity.vy = 0;
}
