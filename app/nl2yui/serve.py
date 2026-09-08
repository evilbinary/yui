#!/usr/bin/env python3
"""NL → YUI JSON inference API (Flask).

POST /api/nl2yui
{
  "message": "...",
  "context": "id:Type:text, ..." | {"ids":[...]} | null,
  "mode": "update" | "full"
}

update → { status, mode, updates, usage }
full   → { status, mode, ui, usage }
"""

from __future__ import annotations

import argparse
import json
import os
import sys
import time
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parent
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from flask import Flask, jsonify, request
from flask_cors import CORS

from prompt import build_inference_messages, format_context
from schema import parse_model_json, validate_output

app = Flask(__name__)
CORS(app)

_state: dict[str, Any] = {
    "model": None,
    "tokenizer": None,
    "device": "cpu",
    "model_path": None,
}


def load_model(model_path: str, device: str = "auto") -> None:
    import torch
    from transformers import AutoModelForCausalLM, AutoTokenizer

    path = Path(model_path)
    if not path.exists():
        raise FileNotFoundError(f"model not found: {path}")

    use_cuda = device in ("auto", "cuda") and torch.cuda.is_available()
    dtype = torch.float16 if use_cuda else torch.float32
    print(f"loading {path} device={'cuda' if use_cuda else 'cpu'} dtype={dtype}")

    tok = AutoTokenizer.from_pretrained(str(path), trust_remote_code=True)
    if tok.pad_token is None:
        tok.pad_token = tok.eos_token

    kwargs: dict[str, Any] = {
        "trust_remote_code": True,
        "torch_dtype": dtype,
        "low_cpu_mem_usage": True,
    }
    if use_cuda:
        kwargs["device_map"] = "cuda"

    model = AutoModelForCausalLM.from_pretrained(str(path), **kwargs)
    model.eval()

    _state["model"] = model
    _state["tokenizer"] = tok
    _state["device"] = "cuda" if use_cuda else "cpu"
    _state["model_path"] = str(path.resolve())
    print("model ready")


def generate(message: str, context: Any, mode: str) -> tuple[Any, dict[str, Any]]:
    import torch

    model = _state["model"]
    tok = _state["tokenizer"]
    if model is None or tok is None:
        raise RuntimeError("model not loaded")

    mode = mode or "update"
    msgs = build_inference_messages(
        message,
        context=format_context(context) if context is not None else "(none)",
        mode=mode,
    )
    prompt = tok.apply_chat_template(msgs, tokenize=False, add_generation_prompt=True)
    inputs = tok(prompt, return_tensors="pt")
    if _state["device"] == "cuda":
        inputs = {k: v.to(model.device) for k, v in inputs.items()}

    max_new = 384 if mode == "full" else 128
    t0 = time.perf_counter()
    with torch.inference_mode():
        out = model.generate(
            **inputs,
            max_new_tokens=max_new,
            do_sample=False,
            pad_token_id=tok.pad_token_id or tok.eos_token_id,
        )
    elapsed = time.perf_counter() - t0
    new = out[0][inputs["input_ids"].shape[-1] :]
    text = tok.decode(new, skip_special_tokens=True)
    n_tok = int(new.numel())
    usage = {
        "prompt_tokens": int(inputs["input_ids"].shape[-1]),
        "completion_tokens": n_tok,
        "elapsed_s": round(elapsed, 3),
        "tok_per_s": round(n_tok / elapsed, 2) if elapsed > 0 else 0.0,
    }

    obj = parse_model_json(text)
    errs = validate_output(obj, mode=mode)
    if errs:
        raise ValueError("invalid model output: " + "; ".join(errs[:3]))
    return obj, usage


@app.get("/health")
def health():
    return jsonify(
        {
            "status": "ok",
            "model": _state.get("model_path"),
            "device": _state.get("device"),
            "loaded": _state.get("model") is not None,
        }
    )


@app.post("/api/nl2yui")
def nl2yui():
    body = request.get_json(silent=True) or {}
    message = (body.get("message") or "").strip()
    if not message:
        return jsonify({"status": "error", "error": "message required"}), 400

    mode = body.get("mode") or "update"
    if mode not in ("update", "full"):
        return jsonify({"status": "error", "error": "mode must be update|full"}), 400

    try:
        obj, usage = generate(message, body.get("context"), mode)
    except Exception as e:
        return jsonify({"status": "error", "error": str(e)}), 500

    if mode == "full":
        return jsonify({"status": "success", "mode": mode, "ui": obj, "usage": usage})
    return jsonify(
        {
            "status": "success",
            "mode": mode,
            "updates": obj.get("updates", []),
            "usage": usage,
        }
    )


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(description="nl2yui inference server")
    p.add_argument(
        "--model",
        default=os.environ.get(
            "NL2YUI_MODEL",
            str(ROOT / "output" / "nl2yui-lora-0.5b" / "merged"),
        ),
    )
    p.add_argument("--host", default="0.0.0.0")
    p.add_argument("--port", type=int, default=8765)
    p.add_argument("--device", default="auto", choices=["auto", "cuda", "cpu"])
    args = p.parse_args(argv)

    load_model(args.model, device=args.device)
    print(f"listening http://{args.host}:{args.port}")
    app.run(host=args.host, port=args.port, threaded=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
