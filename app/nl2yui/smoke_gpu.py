#!/usr/bin/env python3
"""Smoke test on GPU (remote)."""
from __future__ import annotations

import json
import time
from pathlib import Path

import torch
from transformers import AutoModelForCausalLM, AutoTokenizer

from prompt import build_inference_messages
from schema import parse_model_json, validate_output

ROOT = Path(__file__).resolve().parent
MODEL = ROOT / "output" / "nl2yui-lora" / "merged"

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
]


def main() -> int:
    print("load", MODEL)
    tok = AutoTokenizer.from_pretrained(str(MODEL), trust_remote_code=True)
    if tok.pad_token is None:
        tok.pad_token = tok.eos_token
    model = AutoModelForCausalLM.from_pretrained(
        str(MODEL),
        trust_remote_code=True,
        torch_dtype=torch.float16,
        device_map="cuda",
    )
    model.eval()

    for i, ex in enumerate(CASES, 1):
        msgs = build_inference_messages(
            ex["message"], context=ex.get("context"), mode=ex.get("mode", "update")
        )
        prompt = tok.apply_chat_template(msgs, tokenize=False, add_generation_prompt=True)
        inputs = tok(prompt, return_tensors="pt").to(model.device)
        t0 = time.perf_counter()
        with torch.inference_mode():
            out = model.generate(
                **inputs,
                max_new_tokens=128,
                do_sample=False,
                pad_token_id=tok.pad_token_id,
            )
        dt = time.perf_counter() - t0
        new = out[0][inputs["input_ids"].shape[-1] :]
        text = tok.decode(new, skip_special_tokens=True)
        try:
            pred = parse_model_json(text)
            errs = validate_output(pred)
            status = "OK" if not errs else "SCHEMA " + str(errs[:2])
            shown = json.dumps(pred, ensure_ascii=False)[:240]
        except Exception as e:
            status = "PARSE " + str(e)
            shown = text.replace("\n", " ")[:240]
        print("=" * 60)
        print("[%d] %s" % (i, ex["message"]))
        print("pred:", shown)
        print("status: %s  (%.2fs, %d tok)" % (status, dt, int(new.numel())))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
