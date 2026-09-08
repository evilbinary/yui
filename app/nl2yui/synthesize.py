#!/usr/bin/env python3
"""Synthesize NL → YUI JSON SFT data from seeds + templates."""

from __future__ import annotations

import argparse
import json
import random
import sys
from pathlib import Path
from typing import Any, Iterator

ROOT = Path(__file__).resolve().parent
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from schema import validate_example  # noqa: E402

DEFAULT_SEEDS = ROOT / "data" / "seeds.jsonl"
DEFAULT_OUT = ROOT / "data" / "train.jsonl"

IDS = [
    ("titleLabel", "Label", "标题"),
    ("statusLabel", "Label", "就绪"),
    ("hintLabel", "Label", "提示"),
    ("okBtn", "Button", "确定"),
    ("cancelBtn", "Button", "取消"),
    ("saveBtn", "Button", "保存"),
    ("nameInput", "Input", None),
    ("panel", "View", None),
    ("loadPanel", "View", None),
    ("listRoot", "List", None),
    ("gridRoot", "Grid", None),
    ("progressBar", "Progress", None),
    ("mainDialog", "Dialog", None),
]

TEXTS_CN = ["欢迎", "成功", "失败", "加载中", "请稍候", "下一步", "完成", "重试", "设置", "你好"]
TEXTS_EN = ["Welcome", "OK", "Error", "Loading", "Next", "Done", "Retry", "Settings", "Hello"]
COLORS = ["#4caf50", "#f44336", "#2196f3", "#89b4fa", "#cdd6f4", "#a6e3a1", "#f9e2af", "#fab387"]


def dumps_out(updates: list[dict[str, Any]]) -> dict[str, Any]:
    return {"updates": updates}


def ctx_from_ids(pairs: list[tuple[str, str, Any]]) -> str:
    parts = []
    for cid, ctype, text in pairs:
        if text is None:
            parts.append(f"{cid}:{ctype}")
        else:
            parts.append(f"{cid}:{ctype}:{text}")
    return ", ".join(parts)


def default_context(rng: random.Random, extra: list[tuple[str, str, Any]] | None = None) -> str:
    base = rng.sample(IDS, k=rng.randint(3, 6))
    if extra:
        seen = {x[0] for x in base}
        for e in extra:
            if e[0] not in seen:
                base.append(e)
                seen.add(e[0])
    return ctx_from_ids(base)


def pick_id(rng: random.Random, types: set[str] | None = None) -> tuple[str, str, Any]:
    pool = [x for x in IDS if types is None or x[1] in types]
    return rng.choice(pool)


def templates(rng: random.Random) -> Iterator[dict[str, Any]]:
    # --- change text ---
    for lang in ("cn", "en"):
        cid, ctype, _ = pick_id(rng, {"Label", "Button"})
        new_text = rng.choice(TEXTS_CN if lang == "cn" else TEXTS_EN)
        if lang == "cn":
            msgs = [
                f"把 {cid} 的文字改成{new_text}",
                f"将{cid}文本设为{new_text}",
                f"{cid} 显示「{new_text}」",
                f"标题改成{new_text}" if "title" in cid.lower() or ctype == "Label" else f"按钮改成{new_text}",
            ]
        else:
            msgs = [
                f"Set {cid} text to {new_text}",
                f"Change {cid} to say {new_text}",
            ]
        yield {
            "mode": "update",
            "context": default_context(rng, [(cid, ctype, "旧文案")]),
            "message": rng.choice(msgs),
            "output": dumps_out([{"target": cid, "change": {"text": new_text}}]),
        }

    # --- change color ---
    cid, ctype, _ = pick_id(rng, {"Label", "Button"})
    color = rng.choice(COLORS)
    msg = rng.choice(
        [
            f"把 {cid} 改成绿色" if color == "#4caf50" else f"把 {cid} 文字颜色设为 {color}",
            f"{cid} 颜色 {color}",
            f"Set {cid} color to {color}",
        ]
    )
    change: dict[str, Any] = {"color": color}
    if rng.random() < 0.35:
        change["text"] = rng.choice(TEXTS_CN)
        if "成功" in change["text"] or change["text"] == "OK":
            msg = f"把 {cid} 改成成功，颜色 {color}"
    yield {
        "mode": "update",
        "context": default_context(rng, [(cid, ctype, None)]),
        "message": msg,
        "output": dumps_out([{"target": cid, "change": change}]),
    }

    # --- visibility ---
    cid, ctype, _ = pick_id(rng, {"Button", "Label", "Dialog", "View"})
    hide = rng.random() < 0.6
    if hide:
        msg = rng.choice([f"隐藏 {cid}", f"不要显示 {cid}", f"Hide {cid}"])
        change = {"visible": False} if rng.random() < 0.5 else {"visible": None}
    else:
        msg = rng.choice([f"显示 {cid}", f"让 {cid} 可见", f"Show {cid}"])
        change = {"visible": True}
    yield {
        "mode": "update",
        "context": default_context(rng, [(cid, ctype, None)]),
        "message": msg,
        "output": dumps_out([{"target": cid, "change": change}]),
    }

    # --- bgColor / enabled ---
    cid, ctype, _ = pick_id(rng, {"Button", "View"})
    bg = rng.choice(COLORS)
    yield {
        "mode": "update",
        "context": default_context(rng, [(cid, ctype, None)]),
        "message": rng.choice([f"{cid} 背景改成 {bg}", f"Set {cid} bgColor to {bg}"]),
        "output": dumps_out([{"target": cid, "change": {"bgColor": bg}}]),
    }
    cid, ctype, _ = pick_id(rng, {"Button", "Input"})
    yield {
        "mode": "update",
        "context": default_context(rng, [(cid, ctype, None)]),
        "message": rng.choice([f"禁用 {cid}", f"Disable {cid}"]),
        "output": dumps_out([{"target": cid, "change": {"enabled": False}}]),
    }

    # --- create Label / Button / Loading ---
    parent = rng.choice(["panel", "loadPanel", "listRoot"])
    new_id = f"gen_{rng.randint(1, 9999)}"
    kind = rng.choice(["Label", "Button", "Loading"])
    if kind == "Label":
        text = rng.choice(TEXTS_CN)
        child = {
            "id": new_id,
            "type": "Label",
            "text": text,
            "style": {"color": rng.choice(COLORS), "fontSize": rng.choice([12, 14, 16])},
        }
        msg = rng.choice(
            [
                f"在 {parent} 里加一个标签写{text}",
                f"往 {parent} 追加 Label「{text}」",
                f"Add a label {text} under {parent}",
            ]
        )
    elif kind == "Button":
        text = rng.choice(TEXTS_CN)
        child = {
            "id": new_id,
            "type": "Button",
            "text": text,
            "size": [rng.choice([80, 100, 120]), 36],
            "style": {"bgColor": rng.choice(COLORS), "color": "#1e1e2e", "borderRadius": 6},
            "events": {"onClick": "@onGenClick"},
        }
        msg = rng.choice(
            [
                f"在 {parent} 增加按钮 {text}",
                f"Add button {text} to {parent}",
            ]
        )
    else:
        child = {
            "id": new_id,
            "type": "Loading",
            "size": [56, 56],
            "variant": "spinner",
            "text": "加载中...",
            "color": "#89b4fa",
            "trackColor": "#45475a",
            "strokeWidth": 4,
            "speed": 1.0,
        }
        msg = rng.choice(
            [
                f"在 {parent} 里加一个加载中",
                f"给 {parent} 加 Loading",
                f"Add a loading spinner to {parent}",
            ]
        )
    yield {
        "mode": "update",
        "context": default_context(rng, [(parent, "View", None)]),
        "message": msg,
        "output": dumps_out([{"target": parent, "change": {"children": [child]}}]),
    }

    # --- clear children ---
    parent = rng.choice(["panel", "gridRoot", "listRoot"])
    yield {
        "mode": "update",
        "context": default_context(rng, [(parent, "View", None)]),
        "message": rng.choice([f"清空 {parent} 的子节点", f"Clear children of {parent}"]),
        "output": dumps_out([{"target": parent, "change": {"children": None}}]),
    }

    # --- layout ---
    parent = rng.choice(["panel", "loadPanel"])
    layout = {
        "type": rng.choice(["vertical", "horizontal"]),
        "spacing": rng.choice([4, 8, 12]),
        "padding": [rng.choice([8, 10, 12])],
    }
    yield {
        "mode": "update",
        "context": default_context(rng, [(parent, "View", None)]),
        "message": rng.choice(
            [
                f"{parent} 改成{layout['type']}布局，间距{layout['spacing']}",
                f"Set {parent} layout to {layout['type']} spacing {layout['spacing']}",
            ]
        ),
        "output": dumps_out([{"target": parent, "change": {"layout": layout}}]),
    }

    # --- progress value ---
    cid = "progressBar"
    val = rng.randint(0, 100)
    yield {
        "mode": "update",
        "context": default_context(rng, [(cid, "Progress", None)]),
        "message": rng.choice([f"进度条设为 {val}%", f"Set progress to {val}"]),
        "output": dumps_out([{"target": cid, "change": {"value": val}}]),
    }

    # --- batch ---
    a, b = rng.sample([x for x in IDS if x[1] in {"Label", "Button"}], 2)
    t1, t2 = rng.choice(TEXTS_CN), rng.choice(TEXTS_CN)
    yield {
        "mode": "update",
        "context": default_context(rng, [a, b]),
        "message": rng.choice(
            [
                f"把 {a[0]} 改成{t1}，{b[0]} 改成{t2}",
                f"Update {a[0]} to {t1} and {b[0]} to {t2}",
            ]
        ),
        "output": dumps_out(
            [
                {"target": a[0], "change": {"text": t1}},
                {"target": b[0], "change": {"text": t2}},
            ]
        ),
    }

    # --- full mini tree ---
    title = rng.choice(TEXTS_CN)
    btn = rng.choice(TEXTS_CN)
    root_id = "screenRoot"
    yield {
        "mode": "full",
        "context": "(none)",
        "message": rng.choice(
            [
                f"做一个竖排面板，标题{title}，一个{btn}按钮",
                f"Create a vertical panel with title {title} and button {btn}",
            ]
        ),
        "output": dumps_out(
            [
                {
                    "target": "canvas",
                    "change": {
                        "children": [
                            {
                                "id": root_id,
                                "type": "View",
                                "layout": {"type": "vertical", "spacing": 10, "padding": [12]},
                                "style": {"bgColor": "#1e1e2e"},
                                "children": [
                                    {
                                        "id": "fullTitle",
                                        "type": "Label",
                                        "text": title,
                                        "style": {"color": "#cdd6f4", "fontSize": 18},
                                    },
                                    {
                                        "id": "fullBtn",
                                        "type": "Button",
                                        "text": btn,
                                        "size": [120, 36],
                                        "style": {
                                            "bgColor": "#89b4fa",
                                            "color": "#1e1e2e",
                                            "borderRadius": 6,
                                        },
                                        "events": {"onClick": "@onFullClick"},
                                    },
                                ],
                            }
                        ]
                    },
                }
            ]
        ),
    }


def load_jsonl(path: Path) -> list[dict[str, Any]]:
    rows: list[dict[str, Any]] = []
    if not path.exists():
        return rows
    with path.open("r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            rows.append(json.loads(line))
    return rows


def write_jsonl(path: Path, rows: list[dict[str, Any]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as f:
        for row in rows:
            f.write(json.dumps(row, ensure_ascii=False) + "\n")


def synthesize(n: int, seed: int, seeds_path: Path) -> list[dict[str, Any]]:
    rng = random.Random(seed)
    rows = load_jsonl(seeds_path)
    for ex in rows:
        errs = validate_example(ex)
        if errs:
            raise SystemExit(f"invalid seed: {errs} :: {ex}")

    while len(rows) < n:
        for ex in templates(rng):
            errs = validate_example(ex)
            if errs:
                continue
            rows.append(ex)
            if len(rows) >= n:
                break
    rng.shuffle(rows)
    return rows[:n]


def split_train_eval(
    rows: list[dict[str, Any]], eval_ratio: float, seed: int
) -> tuple[list[dict[str, Any]], list[dict[str, Any]]]:
    rng = random.Random(seed)
    idx = list(range(len(rows)))
    rng.shuffle(idx)
    n_eval = max(1, int(len(rows) * eval_ratio)) if len(rows) > 1 else 0
    eval_idx = set(idx[:n_eval])
    train, ev = [], []
    for i, row in enumerate(rows):
        (ev if i in eval_idx else train).append(row)
    return train, ev


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(description="Synthesize nl2yui SFT jsonl")
    p.add_argument("-n", type=int, default=3000, help="total examples")
    p.add_argument("--seed", type=int, default=42)
    p.add_argument("--seeds", type=Path, default=DEFAULT_SEEDS)
    p.add_argument("--out", type=Path, default=DEFAULT_OUT)
    p.add_argument("--eval-out", type=Path, default=ROOT / "data" / "eval.jsonl")
    p.add_argument("--eval-ratio", type=float, default=0.1)
    args = p.parse_args(argv)

    rows = synthesize(args.n, args.seed, args.seeds)
    train, ev = split_train_eval(rows, args.eval_ratio, args.seed + 1)
    write_jsonl(args.out, train)
    write_jsonl(args.eval_out, ev)
    print(f"wrote {len(train)} train -> {args.out}")
    print(f"wrote {len(ev)} eval  -> {args.eval_out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
