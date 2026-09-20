# YUI Game OS · 游戏机桌面

面向掌机 / 游戏机的桌面壳层：**桌面图标网格 + 模拟器动态加载 + 状态栏常用信息**（时间、电量、音量、存储、WiFi）。

基准分辨率 **640×480**（4:3 掌机屏），YUI 框架 + Router 多页导航 + JSON 主题。

## 快速运行

```bash
make console-os
# 等价于
ya -b console-os && ya -r console-os -- app/console-os/app.json
```

mquickjs 引擎（资源受限设备）：

```bash
make console-os-mqjs
```

## 目录结构

```
app/console-os/
├── app.json                  # 壳：状态栏 + page_outlet + 按键提示栏
├── app.js                    # 路由表 / 主题 / 时钟 / 电量 / 常用信息 / 导航
├── config.json               # 持久化配置（主题、音量、亮度、最近游玩）
├── main.c                    # 入口，root = app/console-os
├── console-os.md
├── lib/
│   └── emulator-registry.js  # 扫描 emulators/ 目录，生成模拟器列表与 ROM 列表
├── themes/
│   ├── dark.json
│   └── light.json
├── emulators/                # ★ 每个子目录 = 一个可加载的模拟器
│   ├── nes/nes.json
│   ├── snes/snes.json
│   ├── gb/gb.json
│   ├── gba/gba.json
│   ├── md/md.json
│   ├── n64/n64.json
│   ├── arcade/arcade.json
│   ├── ps1/ps1.json
│   ├── psp/psp.json
│   ├── nds/nds.json
│   ├── saturn/saturn.json
│   └── dos/dos.json
├── roms/                     # ROM 目录（按模拟器分目录，放入即自动收录）
└── apps/
    ├── desktop/    # 桌面 /
    ├── library/    # 游戏库 /library
    ├── emulator/   # 模拟器详情 /emulator
    ├── player/     # 运行界面 /player
    ├── battery/    # 电池详情 /battery
    └── settings/   # 系统设置 /settings
```

## 桌面（`/`）

| 区域 | 内容 |
|------|------|
| 状态栏 | 系统标识 · 时间 · WiFi · 音量 · 存储 · 电量（可点进电池页）· 设置 |
| 信息卡 | 电量（带进度条）、存储可用、音量/亮度，均为运行时更新 |
| 模拟器网格 | 6 列图标磁贴，来自 `emulators/` 扫描结果；含图标、名称、游戏数量 |
| 最近游玩 | 3 张卡片，点击直接回到上次的游戏 |
| 提示栏 | 按键提示 + 「N 个模拟器 · M 个游戏」 |

## 模拟器加载机制

`lib/emulator-registry.js` 在启动时 `YUI.listDir("emulators")`，逐个读取
`emulators/<id>/<id>.json` 元数据并注册；扫描失败时回退到内置最小列表，保证桌面始终可用。

模拟器定义字段（除 `id` 外均可省略）：

```json
{
  "id": "gba",
  "title": "Game Boy Advance",
  "short": "GBA",
  "icon": "🐉",
  "subtitle": "Nintendo Game Boy Advance",
  "core": "mgba",
  "systems": ["GBA"],
  "exts": ["gba", "agb"],
  "romDir": "roms/gba",
  "accent": "#F59E0B",
  "order": 40,
  "bio": "32 位掌机，横版与 RPG 的黄金组合。",
  "roms": [
    { "title": "口袋妖怪 绿宝石", "region": "JP", "year": 2004, "size": "16 MB" }
  ]
}
```

**新增一个模拟器无需改任何代码**：建目录 + 放 `<id>.json`，桌面图标自动出现。

ROM 列表 = `romDir` 下的真实文件 ∪ 定义中的 `roms` 演示条目（按标题去重）。
把 ROM 丢进 `roms/<id>/` 即被收录（文件名作为游戏名）。

## 路由

```javascript
"/"          →  app/console-os/apps/desktop/desktop.json
"/library"   →  app/console-os/apps/library/library.json
"/emulator"  →  app/console-os/apps/emulator/emulator.json   // 当前选中模拟器
"/player"    →  app/console-os/apps/player/player.json
"/battery"   →  app/console-os/apps/battery/battery.json
"/settings"  →  app/console-os/apps/settings/settings.json
```

导航约定：

| 操作 | 行为 |
|------|------|
| 点击桌面磁贴 | 选中模拟器 → `/emulator` |
| 点击游戏条目 | 加载模拟器 + ROM → `/player` |
| `Ⓑ` / 右滑 | 返回上一层 |
| 桌面左滑 | 进入游戏库 |
| 状态栏电量 / ⚙ | `/battery` / `/settings` |

## 常用信息

- **时间**：`Date.now()` 自行换算（兼容 mquickjs，不用 `new Date()`），20 秒刷新。
- **日期**：`civil_from_days` 算法换算公历年月日 + 星期。
- **电量**：状态栏 + 桌面信息卡 + 电池页（环形电量、电压、温度、循环、健康度），
  并有 **近 12 小时电量柱状图**；45 秒模拟一次耗电／充电。
- **存储 / 音量 / 亮度 / WiFi**：状态栏与桌面信息卡同步显示，设置页可调。

## 实现说明（框架约束）

- **Progress 组件不支持运行时改值**（无 `on_data_update`，`value` 也不在属性处理器表中），
  因此所有动态进度条用「轨道 `View` + 填充 `View`」实现，通过 `YUI.update` 改
  `size`（横向）/ `position + size`（纵向柱）。
  电池页环形电量改为按值重建该图层。
- `renderFromJson` 创建的新图层是不可见状态，渲染后必须 `YUI.show(id)`。
- 动态磁贴的样式来自主题规则（`View.tile` / `Button.tile-icon` / `Label.tile-title`），
  `renderFromJson` 内部会执行 `theme_manager_apply_to_tree`。

## 主题

`themes/dark.json`（深空底 + 青色强调）与 `themes/light.json`，设置页或状态栏可切换，
选择结果写入 `config.json`。

关键选择器：`View.console-shell`、`View.hero`、`View.card`、`View.tile`、`View.stat-card`、
`Label.clock`、`Label.stat-value`、`Button.tile-icon`、`Button.tile-icon-active`、`Button.key`。

## 后续可接入

- `roms/<id>/` 真实 ROM 解析（封面、CRC、模拟器核心映射表）
- libretro 前端：把 `/player` 的 `player_screen` 区域替换为核心输出
- 手柄按键映射（`YUI.setEvent` / 自定义 C 事件）
