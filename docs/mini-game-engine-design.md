# YUI Mini Game Engine 设计

> 版本：草案 v0.3 + V0 实现  
> 日期：2026-07-23  
> 状态：**V0 已实现**（可运行 `app/game/demo.json`）

## 目录

- [背景与目标](#背景与目标)
- [设计原则](#设计原则)
- [与完整游戏引擎的取舍](#与完整游戏引擎的取舍)
- [总体架构](#总体架构)
- [复用 YUI 平台壳 vs 新写 Game 语义](#复用-yui-平台壳-vs-新写-game-语义)
- [目录与模块划分](#目录与模块划分)
- [JS Binding](#js-binding)
- [核心对象模型](#核心对象模型)
- [主循环与帧流程](#主循环与帧流程)
- [场景与资源约定](#场景与资源约定)
- [API 草图](#api-草图)
- [分阶段实施](#分阶段实施)
- [明确不做](#明确不做)
- [已确认决策](#已确认决策)
- [风险与对策](#风险与对策)

---

## 背景与目标

YUI 已是轻量 GUI 运行时：主循环、SDL/多后端渲染、输入、资源加载、QuickJS 脚本、跨平台抽象均已具备。目标不是再造 Unity/Unreal，而是在其上叠加 **2D Mini Game Engine**，使开发者能用 JSON + JS 做小游戏，同时 UI（菜单、HUD）继续走现有 Layer 体系。

### 目标

1. **单机 2D 小游戏闭环**：输入 → 逻辑更新 → 简单碰撞 → 精灵绘制（→ 可选音频）
2. **复用平台壳**：不重写 tick / backend / JS 引擎 / 输入 / 资源加载
3. **Game 语义独立**：Entity 世界与 UI Layer 树分离，避免把控件当玩家
4. **KISS**：V0 能跑通一个示例关卡即可，物理/编辑器/网络后置

### 非目标（本设计范围外）

- 3D、PBR、GI、后处理管线
- 权威多人、帧同步 / rollback
- 完整 Cook / AssetBundle 资源烹调
- 自研脚本语言或可视化蓝图

---

## 设计原则

1. **壳复用、语义新写**：平台能力走 YUI；玩法概念走 `Game`
2. **两套树并存**：`Layer` = UI；`Entity` = 游戏世界
3. **Binding 扩展而非换引擎**：在现有 `js_module` 上挂 `Game` 命名空间
4. **中间件能嵌就不自研**：物理（V1+）、音频（V1+）优先现成库
5. **工具链最后做**：先手写 JSON 场景，有内容需求再做编辑器

---

## 与完整游戏引擎的取舍

| 完整引擎模块 | Mini 是否要 | 策略 |
|--------------|-------------|------|
| Core / Runtime | **要** | 复用 `backend_run` / `yui_tick` / ticks；抽 `game_update(dt)` |
| Rendering | **要（极简）** | 精灵 + 相机 + z 排序 + 简单可见性；可后续接 rect batch |
| Physics | **可选** | V0 不用或仅 AABB；V1 可嵌 Box2D 等 |
| ECS / Scene | **要（薄）** | 简易 Entity + 少量 Component，非完整 ECS |
| Scripting | **要** | 复用 QuickJS；实体脚本 `update(dt)` |
| Asset Pipeline | **要（瘦）** | 贴图 + 精灵表 + scene JSON；无 Cook |
| Audio | **后置** | V1：SDL_mixer / miniaudio |
| Input & Platform | **要** | 在现有 pointer/key 上做 `Game.input` 边沿/轴 |
| Tools / Editor | **后置** | V2 再考虑 |
| Networking | **砍（V1）** | 单机优先 |

一句话：**Runtime + 2D 渲染 + 薄场景 + 脚本 + 输入 + 瘦资源 = mini engine。**

---

## 总体架构

```text
┌──────────────────────────────────────────────┐
│  App：菜单/HUD（YUI JSON+JS） + 关卡逻辑（Game JS）│
├──────────────────────────────────────────────┤
│  Game 语义层（新写）                           │
│  Scene · Entity · Transform · Sprite · Camera │
│  Collider(AABB) · Time · Input 边沿            │
├──────────────────────────────────────────────┤
│  JS Binding 扩展                               │
│  全局 Game.*  （与 YUI.* / Socket.* 并列）      │
├──────────────────────────────────────────────┤
│  YUI 平台壳（复用）                             │
│  tick · backend 渲染 · 输入 · 资源 · QuickJS   │
└──────────────────────────────────────────────┘
```

同屏协作示例：暂停菜单、血条用 `YUI` Layer；角色与地图用 `Game` Entity。

---

## 复用 YUI 平台壳 vs 新写 Game 语义

### 平台壳（复用，不重写）

- 窗口与主循环：`backend_run` / `backend_tick` / `yui_tick`
- 绘制：`backend_render_fill_rect` / texture copy / 字体
- 输入原始事件：key / pointer
- 资源：贴图与字体加载、路径约定
- 脚本运行时：现有 `js_module_init` / `js_module_register_api`
- 跨平台 backend 抽象

### Game 语义（新写）

| UI（已有） | Game（新建） |
|------------|--------------|
| Layer / Component | Entity + Components |
| 布局、onClick | `update(dt)`、移动、跳跃 |
| 屏幕控件树 | 世界坐标 + Camera |
| 焦点与命中测试 | AABB / trigger |
| 界面 JSON | 关卡 / 场景 JSON |

**禁止**：把 `Layer` 直接当成玩家或子弹实体。

---

## 目录与模块划分

建议（实现时可微调，确认后按此落地）：

```text
src/game/
  game.h / game.c           # init / shutdown / update / render 入口
  time.c                    # dt、timescale
  input.c                   # 薄封装：读 src/input/state（由 pointer/key listener 喂入）
  scene.c                   # load / clear / spawn / destroy
  entity.c                  # entity 池、组件挂载
  sprite.c                  # 精灵绘制（世界→屏幕）
  camera.c                  # 跟随、视口
  collide.c                 # AABB（V0/V1）

lib/jsmodule-quickjs/
  js_game.c / js_game.h     # 注册 Game.* （风格对齐 js_perf.c）

app/game/                   # 示例小游戏（确认后加）
  scene.json + *.js + assets
```

构建：`src/game/*.c` 编入 `libyui`（或可选 `YUI_WITH_GAME=1`）；binding 在 `js_module_register_api` 中调用 `js_game_register_api`。

---

## JS Binding

### 决策

- **不新建**第二套 jsmodule / JS Context
- 在现有 `js_module_register_api()` 中增加 **`Game` 全局对象**（与 `YUI`、`Socket` 并列）
- 可选别名：`YUI.game` 指向同一对象（默认不挂；需要时可加）

### 注册方式（对齐现有）

参考 `js_perf.c` / Socket：

```c
// js_module_register_api() 内
js_game_register_api(ctx);   // 创建 global.Game
```

### 职责边界

| API 前缀 | 职责 |
|----------|------|
| `YUI.*` | UI Layer：属性、事件、布局、dump/click 等 |
| `Game.*` | 场景、实体、输入边沿、时间、相机 |
| 共用 | 同一 tick、同一渲染 Present；顺序：Game 世界 → UI HUD → Present |

---

## 核心对象模型

V0 组件集（尽量少）：

| Component | 字段（示意） | 说明 |
|-----------|--------------|------|
| `transform` | x, y, rotation?, z | 世界坐标 |
| `sprite` | texture / frame, w, h, anchor | 贴图或精灵表帧 |
| `collider` | w, h, trigger? | AABB，相对 transform |
| `script` | name / path | 每帧 `update(dt)` |

可选（V1）：`animation`、`rigidbody`（或外置物理）、`audio_source`。

实体生命周期：`spawn` → 每帧 update → `destroy`（可对象池）。

---

## 主循环与帧流程

在现有帧回调中插入（例如 `backend_register_update_callback` 或 `backend_tick` 内明确阶段）：

```text
poll input
  → game_input 采样 / 边沿
  → game_update(dt)           # C：移动、碰撞；回调实体 JS update
  → YUI layout / UI 逻辑（现有）
  → game_render(camera)       # 世界精灵
  → render_layer(ui_root)     # UI 覆在上层（菜单/HUD）
  → Present
```

**时间**：`dt = clamp(backend_get_ticks 差值)`；支持 `Game.time.scale`。

**输入**：`event` 全局 listener → `src/input/state` 维护 down/边沿；JS 侧 `Game.input.pressed("Space")`。详见 `docs/input-device-design.md`。

---

## 场景与资源约定

### 场景 JSON（示意）

```json
{
  "id": "level1",
  "camera": { "x": 0, "y": 0, "follow": "player" },
  "entities": [
    {
      "id": "player",
      "transform": { "x": 32, "y": 64, "z": 10 },
      "sprite": { "src": "assets/hero.png", "w": 16, "h": 24 },
      "collider": { "w": 14, "h": 22 },
      "script": "player.js"
    }
  ]
}
```

### 与 UI JSON 关系

- 启动仍可用现有 playground / app JSON 做壳（标题、按钮「开始」）
- 「开始」后 `Game.loadScene("app/game/level1.json")`
- 或单页全屏仅 Game 世界（实现时定一种默认）

---

## API 草图

```js
// 加载 / 切换
Game.loadScene("app/game/level1.json");
Game.clearScene();

// 实体
var e = Game.spawn({
  transform: { x: 0, y: 0, z: 1 },
  sprite: { src: "coin.png", w: 8, h: 8 },
  collider: { w: 8, h: 8, trigger: true },
  script: "coin.js"
});
Game.destroy(e);

// 时间 / 输入
Game.time.dt;
Game.time.scale = 1;
Game.input.down("Left");
Game.input.pressed("Space");
Game.input.axis("Horizontal"); // -1..1

// 相机
Game.camera.follow("player");
Game.camera.set(x, y);

// 查询（V1）
Game.find("player");
Game.findByTag("enemy");
```

实体脚本约定（已定 B）：

```js
function update(entity, dt) {
  if (Game.input.pressed("Space")) entity.vy = -200;
  entity.x += Game.input.axis("Horizontal") * 120 * dt;
}
```

---

## 分阶段实施

### V0 — 可运行示例（确认后优先）

- [x] `src/game` 最小：time、input 边沿、scene、entity、sprite、camera
- [x] AABB + `game_move_and_collide`
- [x] `js_game.c` 注册 `Game.*`
- [x] 挂入现有 tick / render 顺序（世界在下、HUD 在上）
- [x] 示例：`app/game/` 跳跃 + 简易射击竖切

运行：

```bash
ya -r playground -- app/game/demo.json
```

操作：A/D 移动，Space 跳跃，鼠标点击或 F 射击。

### V1 — 可玩

- [x] 精灵表 / 帧动画（`frame` + `anim.frames`/`fps`，`Game.playAnim`）
- [x] 更完整碰撞（固体 + trigger enter/stay/exit → `onTrigger` / `Game.onTrigger`）
- [x] 音频（miniaudio：`Game.audio.play` / `playBgm` / `stopBgm`）
- [x] `loadScene` 切换、简单对象池（`Game.pool.acquire` / `release`）+ `Game.findAllByTag`

### V2 — 像引擎

- [ ] 可选物理中间件（本轮不做）
- [x] Tilemap / 粒子（`tilemap` 场景字段；`Game.spawnParticles`）
- [ ] 极简场景编辑（导出 JSON）（本轮不做）
- [x] Profiler（`Game.perf.getStats`：entities / draws / fps / updateMs / renderMs）

---

## 明确不做

- Vulkan/Metal 专用抽象、PBR、阴影、GI、TAA
- 完整 PhysX / Ragdoll
- AssetBundle / Cook 全家桶
- 权威服务器 / rollback 网络
- 第二套 JS 引擎或并行 `js_module` 运行时
- 用 Layer 树冒充 Entity 世界

---

## 风险与对策

| 风险 | 对策 |
|------|------|
| UI 与 Game 坐标系混乱 | 严格区分屏幕 vs 世界；相机只作用于 Game 渲染 |
| 每帧 JS update 过多 | V0 实体上限小数；热逻辑可下沉 C |
| 与 UI 渲染抢 Present | 固定顺序：Game 世界 → UI Layer → Present |
| Binding 膨胀 | `Game` API 面保持小；高级能力分阶段加 |
| 多后端不一致 | Game 只调 `backend.h`，不直连 SDL |

---

## 已确认决策

| 项 | 结论 |
|----|------|
| 全局名 | **`Game` 独立全局**（不强制 `YUI.game`） |
| 渲染顺序 | **游戏世界在下 → YUI HUD 在上 → Present** |
| 构建 | **`YUI_WITH_GAME` 可选，默认打开** |
| V0 碰撞 | **要 AABB** |
| 实体脚本 | **B：`function update(entity, dt)`**（不用 `this` 约定） |
| V0 示例方向 | **跳跃 + FPS 射击等**（可先做一个竖切：移动/跳 + 简易射击） |

### 实体脚本约定（已定 B）

```js
// player.js
function update(entity, dt) {
  entity.x += Game.input.axis("Horizontal") * 120 * dt;
  if (Game.input.pressed("Space")) entity.vy = -200;
}
```

- 第一个参数为当前实体包装对象，第二参数为 `dt`
- 不依赖 `this`（箭头函数、普通函数均可）
- C 侧调用形如：`update(entityWrapper, dt)`

### V0 示例范围说明

「跳跃 + FPS 射击」作为方向，V0 建议竖切最小集，避免一次铺太大：

| 能力 | V0 | V1 |
|------|----|----|
| 左右移动 + 跳跃 | ✓ | |
| 地面 / 平台 AABB | ✓ | |
| 朝向准星或固定方向射击 | ✓ 简易（子弹 = entity） | |
| 第一人称相机 / 武器模型 | | ✓ 再增强 |
| 多武器、弹匣、击杀反馈 | | ✓ |

V0 示例可落在 `app/game/`：一侧平台跳跃，一侧可射击的靶子/敌人即可验证管线。

---

## 修订记录

| 版本 | 日期 | 说明 |
|------|------|------|
| v0.1 | 2026-07-23 | 初稿，待确认 |
| v0.2 | 2026-07-23 | 确认 Game 全局、渲染顺序、WITH_GAME 默认开、AABB；补充脚本形态说明 |
| v0.3 | 2026-07-23 | 确认脚本 B、示例方向为跳跃+FPS；设计可进入实现 |
