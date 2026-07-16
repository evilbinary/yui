# Layer 渲染性能监控（YUI.perf）

监控每个 Layer 的**每帧渲染次数**和**自身渲染耗时**（不含子节点），用于排查掉帧、重复绘制等问题。

## 快速开始

在页面 JS 中：

```js
YUI.perf.enable();
YUI.perf.setOverlay(true);   // 左上角 HUD
YUI.perf.setTopN(8);         // HUD 显示前 8 个最慢 layer

// 高亮指定 layer
YUI.perf.watch("jsonEditor");
YUI.perf.watch("messageInput");
```

**交互测试页**：`app/tests/test-perf.json`（配套 `test-perf.js`）

```bash
ya -r playground -- app/tests/test-perf.json
```

加载后自动跑一轮检测；也可手动点「刷新统计」「开始压测」「HUD 开/关」。

`YUI.perf` 绑定方式：

- **QuickJS**（`jsmodule-quickjs`）：`lib/jsmodule-quickjs/js_perf.c` 运行时注册
- **mquickjs**（`jsmodule`）：`lib/jsmodule/yui_stdlib.c` 中 `perf.*` 条目，需重新生成 ROM 表：

```bash
ya -c yui-stdlib-host && ya -b yui-stdlib-host
```

控制台周期性输出（可选）：

```js
YUI.perf.setLogInterval(120);  // 每 120 帧打印 Top 层
```

## JS API

| API | 说明 |
|-----|------|
| `YUI.perf.enable()` | 开启统计 |
| `YUI.perf.disable()` | 关闭统计（同时关闭 overlay） |
| `YUI.perf.reset()` | 清空累计数据 |
| `YUI.perf.setOverlay(bool)` | 屏幕 HUD（自动 enable） |
| `YUI.perf.setTopN(n)` | HUD 排行条数，默认 10 |
| `YUI.perf.setLogInterval(n)` | 每 n 帧打印日志，0 为关闭 |
| `YUI.perf.watch(layerId)` | 高亮监控指定 layer |
| `YUI.perf.unwatch(layerId)` | 取消监控 |
| `YUI.perf.clearWatch()` | 清空 watch 列表 |
| `YUI.perf.getFrameStats()` | 返回 `{ fps, frameMs, renderMs, layerCount, frameIndex }` |
| `YUI.perf.getLayerStats(sortBy?)` | 返回数组，`sortBy`: `"time"` / `"count"` / `"name"` |

### getLayerStats 单项字段

- `id` — layer id
- `type` — 组件类型枚举值
- `renderCount` — 本帧 `render_layer` 调用次数（正常为 1，`>1` 表示异常重复绘制）
- `renderMs` — 本帧自身渲染耗时（ms）
- `totalRenderMs` — 累计自身耗时
- `maxRenderCount` — 历史单帧最大 render 次数
- `framesSeen` — 出现过渲染的帧数

## 指标含义

| 指标 | 含义 |
|------|------|
| **renderCount** | 该 layer 本帧进入 `render_layer` 的次数 |
| **renderMs（自身）** | 动画更新 + 组件 `render` + 滚动条，**不含**子 layer |
| **renderMs（整树）** | `getFrameStats().renderMs`，从 root 开始的 `render_layer` 总时间 |
| **layerCount** | 本帧实际渲染的 layer 数量 |

HUD 中橙色行表示：`renderCount > 1` 或 `renderMs > 2ms`。

## 实现位置

- `src/perf/perf.c` — 统计与 overlay
- `src/render.c` — `render_layer` 埋点
- `src/backend/backend_sdl.c` — 帧级计时
- `lib/jsmodule/yui_stdlib.c` — mquickjs `YUI.perf.*` 绑定
- `lib/jsmodule-quickjs/js_perf.c` — QuickJS `YUI.perf.*` 绑定

## 与 Inspect 配合

- `YUI.inspect.*` — 看结构与边界
- `YUI.perf.*` — 看渲染次数与耗时

两者可同时开启。

## 后续（P3）

- 与 `dirty_flags` 联动，跳过未脏 layer 并重算 skip 率
- 导出 JSON 日志供 CI 对比 baseline
