# NL → YUI JSON 小模型训练

对应设计：[docs/nl2yui-small-model-design.md](../../docs/nl2yui-small-model-design.md)

用现成小 Instruct 基座做 **LoRA SFT**（不是从头训），数据为「一句话 + context → `{updates:[...]}`」。

## 目录

```text
app/nl2yui/
  prompt.py       # system/user 模板
  schema.py       # 输出校验（白名单）
  synthesize.py   # 种子 + 模板扩增 → train/eval jsonl
  train.py        # LoRA SFT
  eval.py         # Parse / Schema / Exact + tok/s
  data/seeds.jsonl
  requirements.txt
```

## 环境

```bash
cd app/nl2yui
python -m venv .venv
# Windows: .venv\Scripts\activate
source .venv/bin/activate
pip install -r requirements.txt
```

有 NVIDIA GPU 时建议装对应 CUDA 版 `torch`（见 https://pytorch.org）。CPU 可训，但 0.5B 也会较慢。

## 一键流程

```bash
cd app/nl2yui

# 1) 合成约 3000 条（含 seeds，划分 10% eval）
python synthesize.py -n 3000

# 2) LoRA 微调（默认 Qwen2.5-0.5B-Instruct）
python train.py --model Qwen/Qwen2.5-0.5B-Instruct --out output/nl2yui-lora

# 可选：合并权重，便于 llama.cpp 再量化
python train.py --merge --out output/nl2yui-lora

# 3) 评测
python eval.py --model output/nl2yui-lora --limit 100
```

换 1.5B：

```bash
python train.py --model Qwen/Qwen2.5-1.5B-Instruct --batch-size 2 --grad-accum 8
```

## 数据格式

每行一个 JSON：

```json
{
  "mode": "update",
  "context": "titleLabel:Label:你好, okBtn:Button:确定",
  "message": "把标题改成欢迎",
  "output": { "updates": [ { "target": "titleLabel", "change": { "text": "欢迎" } } ] }
}
```

## 显存经验

| 基座 | LoRA + batch | 约需显存 |
|------|----------------|----------|
| 0.5B | bs=4, accum=4 | ~4–6 GB |
| 1.5B | bs=2, accum=8 | ~8–12 GB |

CPU：把 `--batch-size 1`，并加 `--device cpu`（见 `train.py`）。

## 产出

- `output/nl2yui-lora/`：LoRA adapter（可直接 `eval.py` / PEFT 加载）
- `output/nl2yui-lora/merged/`：`--merge` 后的完整模型，可再转 GGUF 做手机/CPU 推理

导出 GGUF（需另装 llama.cpp 转换脚本）不在本目录内，见设计文档推理章节。
