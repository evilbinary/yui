#!/usr/bin/env python3
"""Rebuild data/seeds.jsonl with atomic + common page examples."""

from __future__ import annotations

import json
import random
from pathlib import Path

from pages import PAGE_BUILDERS, full_page_update, page_scenarios
from schema import validate_example

ROOT = Path(__file__).resolve().parent
OUT = ROOT / "data" / "seeds.jsonl"

BASE_ATOMIC = [
    {
        "mode": "update",
        "context": "titleLabel:Label:你好, okBtn:Button:确定, panel:View",
        "message": "把标题改成欢迎",
        "output": {"updates": [{"target": "titleLabel", "change": {"text": "欢迎"}}]},
    },
    {
        "mode": "update",
        "context": "statusLabel:Label:就绪, okBtn:Button:确定",
        "message": "把 statusLabel 改成成功，绿色",
        "output": {
            "updates": [
                {"target": "statusLabel", "change": {"text": "成功", "color": "#4caf50"}}
            ]
        },
    },
    {
        "mode": "update",
        "context": "cancelBtn:Button:取消, okBtn:Button:确定, panel:View",
        "message": "隐藏 cancelBtn",
        "output": {"updates": [{"target": "cancelBtn", "change": {"visible": None}}]},
    },
    {
        "mode": "update",
        "context": "loadPanel:View, statusLabel:Label:加载",
        "message": "在 loadPanel 里加一个加载中",
        "output": {
            "updates": [
                {
                    "target": "loadPanel",
                    "change": {
                        "children": [
                            {
                                "id": "overlayLoader",
                                "type": "Loading",
                                "size": [56, 56],
                                "variant": "spinner",
                                "text": "加载中...",
                                "color": "#89b4fa",
                                "trackColor": "#45475a",
                                "strokeWidth": 4,
                                "speed": 1.0,
                            }
                        ]
                    },
                }
            ]
        },
    },
    {
        "mode": "update",
        "context": "panel:View, titleLabel:Label:A",
        "message": "清空 panel 的子节点",
        "output": {"updates": [{"target": "panel", "change": {"children": None}}]},
    },
    {
        "mode": "update",
        "context": "okBtn:Button:确定, cancelBtn:Button:取消",
        "message": "禁用 okBtn",
        "output": {"updates": [{"target": "okBtn", "change": {"enabled": False}}]},
    },
    {
        "mode": "update",
        "context": "progressBar:Progress, statusLabel:Label",
        "message": "进度条设为 75%",
        "output": {"updates": [{"target": "progressBar", "change": {"value": 75}}]},
    },
    {
        "mode": "update",
        "context": "nameInput:Input, okBtn:Button:确定",
        "message": "nameInput 的 placeholder 改成请输入姓名",
        "output": {
            "updates": [{"target": "nameInput", "change": {"placeholder": "请输入姓名"}}]
        },
    },
    {
        "mode": "update",
        "context": "titleLabel:Label:A, okBtn:Button:B",
        "message": "把 titleLabel 改成完成，okBtn 改成关闭",
        "output": {
            "updates": [
                {"target": "titleLabel", "change": {"text": "完成"}},
                {"target": "okBtn", "change": {"text": "关闭"}},
            ]
        },
    },
]

TITLES = {
    "login": "做一个登录页",
    "settings": "做一个设置页",
    "messages": "做一个消息列表页",
    "form": "做一个资料表单页",
    "chat": "做一个聊天助手页",
    "launcher": "做一个应用启动器网格",
    "loading": "做一个带加载遮罩的页面",
    "dialog": "做一个带确认对话框的页面",
    "product": "做一个商品详情页",
    "empty": "做一个空状态页",
}


def main() -> int:
    rows: list[dict] = list(BASE_ATOMIC)
    for name, builder in PAGE_BUILDERS.items():
        rows.append(
            {
                "mode": "full",
                "context": "(none)",
                "message": TITLES[name],
                "output": full_page_update(builder()),
            }
        )

    rng = random.Random(7)
    seen = set(TITLES.values())
    for ex in page_scenarios(rng):
        msg = ex["message"]
        if msg in seen:
            continue
        seen.add(msg)
        rows.append({k: v for k, v in ex.items() if k != "meta"})

    for i, row in enumerate(rows):
        errs = validate_example(row)
        if errs:
            raise SystemExit(f"seed {i} invalid: {errs} :: {row.get('message')}")

    OUT.parent.mkdir(parents=True, exist_ok=True)
    with OUT.open("w", encoding="utf-8") as f:
        for row in rows:
            f.write(json.dumps(row, ensure_ascii=False) + "\n")
    print(f"wrote {len(rows)} seeds -> {OUT}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
