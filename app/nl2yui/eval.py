#!/usr/bin/env python3
"""Evaluate a fine-tuned nl2yui model on eval.jsonl (Parse / Schema / Exact)."""

from __future__ import annotations

import argparse
import json
import sys
import time
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parent
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from prompt import build_inference_messages  # noqa: E402
from schema import parse_model_json, validate_output  # noqa: E402


def load_jsonl(path: Path) -> list[dict[str, Any]]:
    rows = []
    with path.open("r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if line:
                rows.append(json.loads(line))
    return rows


def canonical(obj: Any) -> str:
    return json.dumps(obj, ensure_ascii=False, sort_keys=True, separators=(",", ":"))


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(description="Eval nl2yui model")
    p.add_argument(
        "--model",
        default=str(ROOT / "output" / "nl2yui-lora"),
        help="LoRA dir, merged dir, or HF id",
    )
    p.add_argument(
        "--base",
        default=None,
        help="base model if --model is a LoRA adapter dir",
    )
    p.add_argument("--data", type=Path, default=ROOT / "data" / "eval.jsonl")
    p.add_argument("--limit", type=int, default=0, help="0 = all")
    p.add_argument("--max-new-tokens", type=int, default=256)
    p.add_argument("--temperature", type=float, default=0.0)
    args = p.parse_args(argv)

    try:
        import torch
        from peft import PeftModel
        from transformers import AutoModelForCausalLM, AutoTokenizer
    except ImportError as e:
        print(f"Missing deps: {e}", file=sys.stderr)
        return 1

    if not args.data.exists():
        print(f"missing {args.data}; run synthesize.py first", file=sys.stderr)
        return 1

    rows = load_jsonl(args.data)
    if args.limit > 0:
        rows = rows[: args.limit]

    model_path = Path(args.model)
    adapter_config = model_path / "adapter_config.json"
    is_lora = adapter_config.exists()

    if is_lora:
        meta_path = model_path / "nl2yui_train_meta.json"
        base = args.base
        if base is None and meta_path.exists():
            base = json.loads(meta_path.read_text(encoding="utf-8")).get("base_model")
        if not base:
            print("LoRA dir needs --base or nl2yui_train_meta.json", file=sys.stderr)
            return 1
        print(f"load base={base} + adapter={model_path}")
        tokenizer = AutoTokenizer.from_pretrained(str(model_path), trust_remote_code=True)
        dtype = torch.float16 if torch.cuda.is_available() else torch.float32
        model = AutoModelForCausalLM.from_pretrained(
            base, trust_remote_code=True, torch_dtype=dtype, device_map="auto" if torch.cuda.is_available() else None
        )
        model = PeftModel.from_pretrained(model, str(model_path))
    else:
        print(f"load model={args.model}")
        tokenizer = AutoTokenizer.from_pretrained(args.model, trust_remote_code=True)
        dtype = torch.float16 if torch.cuda.is_available() else torch.float32
        model = AutoModelForCausalLM.from_pretrained(
            args.model,
            trust_remote_code=True,
            torch_dtype=dtype,
            device_map="auto" if torch.cuda.is_available() else None,
        )

    model.eval()
    if tokenizer.pad_token is None:
        tokenizer.pad_token = tokenizer.eos_token

    n = len(rows)
    parse_ok = schema_ok = exact_ok = 0
    gen_tokens = 0
    elapsed = 0.0

    for i, ex in enumerate(rows):
        messages = build_inference_messages(
            ex.get("message", ""),
            context=ex.get("context"),
            mode=ex.get("mode", "update"),
        )
        prompt = tokenizer.apply_chat_template(
            messages, tokenize=False, add_generation_prompt=True
        )
        inputs = tokenizer(prompt, return_tensors="pt")
        if torch.cuda.is_available():
            inputs = {k: v.to(model.device) for k, v in inputs.items()}

        gen_kwargs: dict[str, Any] = {
            "max_new_tokens": args.max_new_tokens,
            "pad_token_id": tokenizer.pad_token_id,
        }
        if args.temperature and args.temperature > 0:
            gen_kwargs["do_sample"] = True
            gen_kwargs["temperature"] = args.temperature
        else:
            gen_kwargs["do_sample"] = False

        t0 = time.perf_counter()
        with torch.inference_mode():
            out = model.generate(**inputs, **gen_kwargs)
        dt = time.perf_counter() - t0
        elapsed += dt

        new_tokens = out[0][inputs["input_ids"].shape[-1] :]
        gen_tokens += int(new_tokens.numel())
        text = tokenizer.decode(new_tokens, skip_special_tokens=True)

        gold = ex["output"]
        if isinstance(gold, str):
            gold = json.loads(gold)

        try:
            pred = parse_model_json(text)
            parse_ok += 1
        except Exception:
            pred = None

        if pred is not None:
            errs = validate_output(pred, mode=ex.get("mode", "update"))
            if not errs:
                schema_ok += 1
            if canonical(pred) == canonical(gold):
                exact_ok += 1

        if (i + 1) % 20 == 0 or i == 0:
            print(f"[{i+1}/{n}] parse={parse_ok} schema={schema_ok} exact={exact_ok}")

    tok_s = (gen_tokens / elapsed) if elapsed > 0 else 0.0
    report = {
        "n": n,
        "parse_at_1": parse_ok / n if n else 0,
        "schema_at_1": schema_ok / n if n else 0,
        "exact_at_1": exact_ok / n if n else 0,
        "gen_tokens": gen_tokens,
        "elapsed_s": round(elapsed, 3),
        "tok_per_s": round(tok_s, 2),
    }
    print(json.dumps(report, ensure_ascii=False, indent=2))
    out_path = ROOT / "output" / "eval_report.json"
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    print(f"wrote {out_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
