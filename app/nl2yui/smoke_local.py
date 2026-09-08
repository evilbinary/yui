#!/usr/bin/env python3
"""Quick local smoke test for synced merged model."""
from __future__ import annotations

import json
import time
from pathlib import Path

import torch
from transformers import AutoModelForCausalLM, AutoTokenizer

from prompt import build_inference_messages
from schema import parse_model_json, validate_output

ROOT = Path(__file__).resolve().parent
MODEL = ROOT / "output" / "nl2yui-lora-0.5b" / "merged"

CASES = [
    {
        "mode": "update",
        "context": "loginTitle:Label:欢迎登录, loginBtn:Button:登录, loginHint:Label",
        "message": "登录按钮改成登录中并禁用",
    },
    {
        "mode": "update",
        "context": "loadMask:View, overlayLoader:Loading:加载中...",
        "message": "显示加载遮罩",
    },
    {
        "mode": "update",
        "context": "productPrice:Label:¥299, buyBtn:Button:立即购买, productStock:Label:库存 12",
        "message": "价格改成 ¥199",
    },
    {
        "mode": "full",
        "context": "(none)",
        "message": "做一个登录页",
    },
    {
        "mode": "full",
        "context": "(none)",
        "message": "做一个设置页",
    },
]


def main() -> int:
    print(f"load {MODEL}")
    tok = AutoTokenizer.from_pretrained(str(MODEL), trust_remote_code=True)
    if tok.pad_token is None:
        tok.pad_token = tok.eos_token

    model = AutoModelForCausalLM.from_pretrained(
        str(MODEL),
        trust_remote_code=True,
        torch_dtype=torch.float32,
        low_cpu_mem_usage=True,
    )
    model.eval()

    for i, ex in enumerate(CASES, 1):
        msgs = build_inference_messages(
            ex["message"], context=ex.get("context"), mode=ex.get("mode", "update")
        )
        prompt = tok.apply_chat_template(msgs, tokenize=False, add_generation_prompt=True)
        inputs = tok(prompt, return_tensors="pt")
        t0 = time.perf_counter()
        max_new = 384 if ex.get("mode") == "full" else 128
        with torch.inference_mode():
            out = model.generate(
                **inputs,
                max_new_tokens=max_new,
                do_sample=False,
                pad_token_id=tok.pad_token_id or tok.eos_token_id,
            )
        dt = time.perf_counter() - t0
        new = out[0][inputs["input_ids"].shape[-1] :]
        text = tok.decode(new, skip_special_tokens=True)
        try:
            pred = parse_model_json(text)
            errs = validate_output(pred)
            status = "OK" if not errs else f"SCHEMA {errs[:2]}"
            shown = json.dumps(pred, ensure_ascii=False)[:400]
        except Exception as e:
            status = f"PARSE {e}"
            shown = text.replace("\n", " ")[:240]
        print("=" * 60)
        print(f"[{i}] {ex['message']}")
        print(f"pred: {shown}")
        print(f"status: {status}  ({dt:.1f}s, {int(new.numel())} tok)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
