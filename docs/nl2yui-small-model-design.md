# NL → YUI JSON 小模型设计

> 版本：草案 v0.1  
> 日期：2026-09-08  
> 状态：**设计草案**（未实现）  
> 相关：[`json-format-spec.md`](json-format-spec.md) · [`json-update-spec.md`](json-update-spec.md) · [`json-update-examples.md`](json-update-examples.md) · [`app/playground/API_README.md`](../app/playground/API_README.md)

## 目录

- [背景与目标](#背景与目标)
- [设计原则](#设计原则)
- [任务定义](#任务定义)
- [输出契约（必须严格）](#输出契约必须严格)
- [总体架构](#总体架构)
- [模型选型与 100 tok/s](#模型选型与-100-toks)
- [约束解码与 Schema](#约束解码与-schema)
- [数据合成](#数据合成)
- [训练方案](#训练方案)
- [推理服务与 YUI 集成](#推理服务与-yui-集成)
- [评测](#评测)
- [分阶段实施](#分阶段实施)
- [明确不做](#明确不做)
- [风险与对策](#风险与对策)

---

## 背景与目标

YUI 的 UI 由 JSON 树 + `YUI.update({ target, change })` 驱动。Playground 已有「自然语言 → 增量 updates」的接口雏形，但依赖外部大模型，延迟高、成本高、格式偶发出错。

目标是落地一个**专用小模型**：一句话 → 合法 YUI JSON，本地/边端可跑，吞吐 **≥ 100 token/s**（单请求 decode，consumer GPU 或较好 CPU + 量化）。

### 目标

1. **一句话生成**：中文/英文短指令 → JSON（优先增量 `updates[]`，可选全量树）
2. **格式可靠**：输出可直接 `YUI.update` / `renderFromJson`，非法率接近 0（靠 schema + 校验）
3. **≥ 100 tok/s**：小参数 + 量化 + 短输出；不追求开放域对话能力
4. **可接现有 Playground**：替换/旁路 `api-server` 里对大模型的调用

### 非目标（本设计范围外）

- 通用聊天、多轮长对话、代码解释
- 任意复杂页面设计（几十层嵌套、自由布局艺术）
- 端侧 NPU 全平台适配（V1 先定一个主力 runtime）
- 自研 tokenizer / 自研训练框架

---

## 设计原则

1. **任务极窄**：只学「指令 → YUI JSON」，不学通用知识
2. **输出短**：默认增量 patch，目标 32–128 tokens；全量树单独模式、限深限宽
3. **硬约束优先于软提示**：JSON Schema / grammar 约束解码 + 运行时校验，模型只填槽
4. **白名单组件**：先支持高频类型，扩展靠数据而非调大模型
5. **合成数据为主**：从现有 `docs/`、`app/tests/`、`app/*/ui` 抽模板，再程序化扩增
6. **失败可降级**：非法 JSON → 重试一次或回退模板规则

---

## 任务定义

### 输入

| 字段 | 说明 |
|------|------|
| `message` | 用户一句话，如「把标题改成欢迎」 |
| `context`（可选） | 当前 UI 摘要：可见 `id` 列表、类型、关键 text；**不要**把整棵大树塞进 prompt |
| `mode` | `update`（默认）\| `create` \| `full` |

推荐把 context 压成紧凑摘要，例如：

```text
ids: titleLabel:Label:"你好", okBtn:Button:"确定", panel:View
```

### 输出

**默认（增量）**：

```json
{
  "updates": [
    { "target": "titleLabel", "change": { "text": "欢迎" } }
  ]
}
```

**创建子树**（挂到已有父节点）：

```json
{
  "updates": [
    {
      "target": "panel",
      "change": {
        "children": [
          {
            "id": "hint",
            "type": "Label",
            "text": "请稍候",
            "style": { "color": "#cdd6f4", "fontSize": 14 }
          }
        ]
      }
    }
  ]
}
```

**全量（`mode=full`，限深）**：单根 `View` 树，字段遵循 [`json-format-spec.md`](json-format-spec.md)。

### 示例对（训练/评测用）

| 用户话 | 期望输出（摘要） |
|--------|------------------|
| 把 statusLabel 改成成功，绿色 | `target=statusLabel, change={text, color:#4caf50}` |
| 在 loadPanel 里加一个加载中 | `children` 追加 `Loading` |
| 隐藏 cancelBtn | `visible: null` 或 `visible: false`（与规范一致处用 null 删/隐） |
| 做一个竖排面板，标题和两个按钮 | `full`：`View` + `Label` + 2×`Button` |

---

## 输出契约（必须严格）

与运行时 `yui_update_from_json` 对齐，模型与后处理都必须遵守：

1. 增量条目形如 `{ "target": "<id|path>", "change": { ... } }`；**必须有 `change`**
2. 批量用数组；服务层可包一层 `{ "updates": [...] }`
3. **改已有节点**：`change` 内属性尽量**扁平**（`text` / `bgColor` / `color` / `visible` / `layout` …）
4. **新建**：`change.children: [{ id, type, ... }]`；新建可用嵌套 `style` / `events`
5. **删除**：属性或节点置 `null`（见 [`json-update-spec.md`](json-update-spec.md)）
6. 颜色 `#RRGGBB`；`size` / `position` 为二元整数数组
7. 事件值为 `"@handlerName"`，不生成内联 JS 源码
8. `type` 仅来自白名单（见下）

### V0 组件白名单

| type | 用途 |
|------|------|
| `View` | 容器 |
| `Label` / `Button` / `Input` | 基础交互 |
| `Image` | 图片 |
| `Loading` / `Progress` | 反馈 |
| `List` / `Grid` | 简单列表布局 |
| `Checkbox` / `Select` | 表单 |
| `Dialog` | 弹层（字段子集） |

后置：`Table`、`Tab`、`Terminal`、`Connector`、`Draggable` 等（数据与 schema 就绪再开）。

### V0 属性白名单（高频）

`id`, `type`, `text`, `label`, `placeholder`, `color`, `bgColor`, `fontSize`, `borderRadius`, `size`, `position`, `layout`, `flex`, `padding`, `visible`, `enabled`, `source`, `variant`, `value`/`data`（按组件）, `children`, `events`, `style`（仅 create）

超出白名单的键：解码阶段拒绝或后处理剥掉。

---

## 总体架构

```text
┌─────────────┐     ┌──────────────────┐     ┌─────────────────┐
│ 一句话 +     │────▶│  Prompt 压缩      │────▶│ 小模型 decode   │
│ context 摘要 │     │  (模板 + 截断)    │     │ + JSON grammar  │
└─────────────┘     └──────────────────┘     └────────┬────────┘
                                                      │
                                                      ▼
                                            ┌─────────────────┐
                                            │ JSON 校验 / 修复 │
                                            │ id 存在性检查    │
                                            └────────┬────────┘
                                                      │
                          ┌───────────────────────────┼───────────────────────────┐
                          ▼                           ▼                           ▼
                   YUI.update(updates)         renderFromJson              返回错误/重试
                   (Playground / App)
```

两条产出路径（与现有 Playground API 对齐）：

| 路径 | 用途 | 对接 |
|------|------|------|
| `updates[]` | 改现有 UI | `YUI.update` |
| 全量 root | 新开一屏 / 草稿 | 编辑器替换或 `renderFromJson` |

---

## 模型选型与 100 tok/s

「≥ 100 tok/s」约束会直接决定参数量与 runtime，而不是事后优化。

### 推荐路线（按优先级）

| 方案 | 模型体量 | 预期吞吐（约） | 说明 |
|------|----------|----------------|------|
| **A. 主推** | 0.5B–1.5B Instruct，Q4_K / INT4 | GPU 上易过 100；CPU 看机器 | 专用 SFT + 约束解码 |
| B. 备选 | 2B–3B Q4 | 需较好 GPU 才稳过 100 | 格式更稳，但更重 |
| C. 极致边端 | ≤0.5B 或蒸馏到 0.1–0.3B | CPU 冲 100 | 配合更强模板/填槽 |

建议 **V0 锁定方案 A**：例如 Qwen2.5-0.5B / 1.5B、Llama-3.2-1B、Gemma-2-2B（偏大，作对照）中择一，以 **llama.cpp / ggml** 或 **MLC / ONNX Runtime GenAI** 部署。

### 吞吐预算（经验公式）

- 目标输出长度 **L ≈ 64 tokens**（中位）
- 端到端希望 **&lt; 0.7s** 感知延迟 → 需要 **≳ 90–100 tok/s** 仅 decode；再加 prefill（短 prompt 通常 &lt; 200 tokens）
- 手段：
  1. **短 prompt**：system 固定短指令 + 白名单摘要，不贴长文档
  2. **量化**：Q4_K_M / INT4；权重常驻内存
  3. **批大小 1** 先保单请求延迟；多用户再考虑 continuous batching
  4. **约束解码**略降速，用短输出补偿；禁止无约束自由生成再正则抠 JSON

### Prompt 形态（示意）

```text
你是 YUI JSON 生成器。只输出 JSON，不要解释。
mode=update
context: titleLabel:Label, okBtn:Button, panel:View
user: 把标题改成欢迎
```

期望模型直接输出：

```json
{"updates":[{"target":"titleLabel","change":{"text":"欢迎"}}]}
```

---

## 约束解码与 Schema

软提示无法保证括号匹配；V0 起就上 **grammar / JSON Schema guided decoding**。

### Schema 要点（增量）

```json
{
  "type": "object",
  "required": ["updates"],
  "properties": {
    "updates": {
      "type": "array",
      "minItems": 1,
      "maxItems": 8,
      "items": {
        "type": "object",
        "required": ["target", "change"],
        "properties": {
          "target": { "type": "string", "minLength": 1 },
          "change": { "type": "object", "minProperties": 1 }
        },
        "additionalProperties": false
      }
    }
  },
  "additionalProperties": false
}
```

`change` 内属性用 **oneOf / propertyNames enum** 限制到白名单；`children[]` 用 discriminated union（按 `type`）限制字段。  
实现可选：llama.cpp GBNF、Outlines、xgrammar、LM Format Enforcer。

### 运行时二次校验（必做）

1. `JSON.parse` 成功  
2. Schema 校验  
3. `target` 是否在 context id 集（create 挂载点除外）  
4. `type` ∈ 白名单  
5. 可选：用现有 C/JS 路径 dry-run（测试环境）

失败策略：同 prompt **温度降低重试 1 次** → 仍失败则返回 `status: error`，由 UI 提示，不把半截 JSON 喂给 `YUI.update`。

---

## 数据合成

小模型质量 ≈ 数据覆盖；不必百万级通用语料。

### 来源

1. **金标种子**：从 `docs/json-update-examples.md`、`app/tests/test-*.js`、`app/playground` 抽取真实 `YUI.update` 片段  
2. **模板扩增**：对每种意图（改字、改色、显隐、追加 Button/Label/Loading、清 children、改 layout）写 20–50 条模板，替换 id/文案/颜色  
3. **反向生成**：已有 UI JSON → 用规则或大模型（一次性离线）写中文指令，再人工抽检  
4. **难例**：错误 id、歧义（「那个按钮」）、多 target 批量、null 删除

### 规模建议

| 阶段 | 条数（量级） | 说明 |
|------|--------------|------|
| V0 | 2k–5k | 覆盖白名单 × 意图 |
| V1 | 10k–30k | 加难例、多组件组合、中英 |
| V2 | + 真实日志 | Playground 采纳/拒绝反馈 |

格式统一：

```json
{
  "mode": "update",
  "context": "titleLabel:Label:你好, okBtn:Button:确定",
  "message": "把标题改成欢迎",
  "output": { "updates": [ { "target": "titleLabel", "change": { "text": "欢迎" } } ] }
}
```

---

## 训练方案

### V0：SFT only

- 基座：0.5B–1.5B instruct  
- 损失：仅对 `output` JSON token 计算（instruction masking）  
- 序列短：`max_len` 512 或 1024 足够  
- 1–3 epoch，防过拟合到模板句式（加同义指令改写）

### V1：可选偏好 / 修复

- 非法 JSON / 错 id 的负例 → 正确输出（DPO 或简单 reject sampling 再 SFT）  
- 或「先自由生成再教模型输出修复后的 canonical JSON」

### 不建议 V0 做

- 继续预训练全量网页语料  
- 超长 CoT（浪费 token，伤害 100 tok/s 目标）

---

## 推理服务与 YUI 集成

### 服务形态

```text
POST /api/nl2yui
{
  "message": "...",
  "context": { "ids": [ {"id","type","text"?} ] },
  "mode": "update"
}

→ 200
{
  "status": "success",
  "updates": [ { "target", "change" }, ... ],
  "usage": { "prompt_tokens", "completion_tokens", "tok_per_s" }
}
```

与 [`app/playground/API_README.md`](../app/playground/API_README.md) 的 incremental 接口兼容：Playground 把 `response.updates` 交给 `YUI.update`。

### 部署拓扑

| 环境 | 建议 |
|------|------|
| 开发机 | 本地 llama.cpp HTTP 或 Python 包装，Playground Flask 反代 |
| 演示 | 单进程：量化模型常驻 + 上述 API |
| 嵌入（后置） | 同进程 FFI 调 llama.cpp；注意与 UI 主线程隔离 |

### 客户端注意

- mquickjs 场景对 `YUI.update` 传 **字符串** 更稳：`YUI.update(JSON.stringify(updates))`  
- 流式：可按「完整 JSON 闭合」后再 update；或服务端只在合法后一次性返回（V0 推荐非流式，保格式）

---

## 评测

### 自动指标

| 指标 | 定义 | V0 门槛（建议） |
|------|------|-----------------|
| Parse@1 | 可 JSON.parse | ≥ 99% |
| Schema@1 | 过 JSON Schema | ≥ 97% |
| Exact match | 与金标 canonical 一致 | 视集合，作参考 |
| Apply@1 | 在测试 harness 中 `YUI.update` 不报错 | ≥ 95% |
| Intent@1 | 人工/规则判定意图正确 | ≥ 90%（抽检） |
| tok/s | 服务上报 decode 速率 | **≥ 100**（约定硬件） |

### 约定基准硬件（写入 CI/文档）

先定一条可复现线，例如：

- **GPU**：RTX 3060 / 4060 级，batch=1，Q4，prompt≤256，gen≤128  
- 或 **CPU**：近年 8 核 + 足够 RAM，同量化（若 CPU 达不到 100，文档标明「GPU 达标；CPU 为 best-effort」）

未约定硬件前，「100 tok/s」只作为产品目标，不作为模型卡虚标。

### 评测集

固定 `eval/nl2yui-v0.jsonl`（约 200–500 条），含：改属性、创建、删除、批量、全量小树、对抗（缺 id、超白名单）。

---

## 分阶段实施

### V0 — 可用闭环（优先）

1. 冻结白名单 + JSON Schema / GBNF  
2. 合成 2k+ 样本 + 手写 100 条金标  
3. SFT 0.5B/1.5B，导出 GGUF/ONNX  
4. 本地 API：`/api/nl2yui`，测 tok/s  
5. 接 Playground incremental，演示「一句话改 UI」  
6. 文档与 `eval` 集入库

### V1 — 稳与快

1. context 自动从当前编辑器 JSON 抽取 id 摘要  
2. 约束解码 + 校验重试  
3. 吞吐优化（kv cache、prompt cache、更短 system）  
4. 扩大组件白名单（Table / Tab 等）  
5. 采纳/拒绝日志 → 增量训练

### V2 — 产品化（可选）

1. 多模态（草图/截图 → JSON）另案  
2. 端侧包（Android/iOS）与主工程集成策略  
3. 与主题/设计 token 对齐的颜色与间距词表

---

## 明确不做

- 让模型生成任意 JS 业务逻辑源码（只允许 `"@handler"` 引用）  
- 无 schema 的「自由发挥」大段 UI  
- 用 7B+ 云端大模型冒充本方案的性能指标  
- 在 UI 线程同步跑 prefill/decode  

---

## 风险与对策

| 风险 | 对策 |
|------|------|
| 小模型胡编 id / 类型 | context 摘要 + schema enum；Apply 前校验 |
| 约束解码拖慢 &lt; 100 tok/s | 缩短输出上限；简化 grammar；测 A/B |
| 合成数据句式单一 | 同义改写、多模板、难例比例 ≥ 15% |
| 全量树爆炸 | `mode=full` 限深（如 ≤3）限宽（如 ≤8 children） |
| 与文档/实现不一致（如缺 `change`） | 以 `src/layer_update.c` + 测试为准；训练数据只收合法样本 |
| Playground 已有大模型路径 | 小模型作默认；大模型作 fallback 开关 |

---

## 已确认方向（草案）

1. **默认增量 `updates[]`**，全量为显式 mode  
2. **硬 schema + 白名单**，不以 prompt 工程替代  
3. **0.5B–1.5B + 量化** 作为冲 100 tok/s 的主路径  
4. **Playground 先集成**，再考虑进程内嵌  

待拍板：具体基座型号、主力 runtime（llama.cpp vs 其他）、达标硬件写死哪一档。

---

## 参考

- [`json-format-spec.md`](json-format-spec.md) — UI 树  
- [`json-update-spec.md`](json-update-spec.md) / [`json-update-examples.md`](json-update-examples.md) — 增量协议  
- [`layer-prop.md`](layer-prop.md) — 公共属性  
- [`yui-js-api.md`](yui-js-api.md) — `YUI.update`  
- [`app/playground/API_README.md`](../app/playground/API_README.md) — 现有 NL API 雏形  
