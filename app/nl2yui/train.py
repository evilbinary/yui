#!/usr/bin/env python3
"""LoRA SFT for NL → YUI JSON (HuggingFace + PEFT + TRL 0.11)."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parent
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from prompt import build_messages  # noqa: E402


def load_jsonl(path: Path) -> list[dict[str, Any]]:
    rows: list[dict[str, Any]] = []
    with path.open("r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            rows.append(json.loads(line))
    return rows


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(description="SFT train nl2yui small model (LoRA)")
    p.add_argument(
        "--model",
        default="Qwen/Qwen2.5-0.5B-Instruct",
        help="HF model id or local path",
    )
    p.add_argument("--train", type=Path, default=ROOT / "data" / "train.jsonl")
    p.add_argument("--eval", type=Path, default=ROOT / "data" / "eval.jsonl")
    p.add_argument("--out", type=Path, default=ROOT / "output" / "nl2yui-lora")
    p.add_argument("--max-seq-len", type=int, default=768)
    p.add_argument("--epochs", type=float, default=2.0)
    p.add_argument("--batch-size", type=int, default=4)
    p.add_argument("--grad-accum", type=int, default=4)
    p.add_argument("--lr", type=float, default=2e-4)
    p.add_argument("--lora-r", type=int, default=16)
    p.add_argument("--lora-alpha", type=int, default=32)
    p.add_argument("--lora-dropout", type=float, default=0.05)
    p.add_argument("--warmup-ratio", type=float, default=0.03)
    p.add_argument("--logging-steps", type=int, default=10)
    p.add_argument("--save-steps", type=int, default=200)
    p.add_argument("--eval-steps", type=int, default=200)
    p.add_argument("--seed", type=int, default=42)
    p.add_argument(
        "--device",
        default="auto",
        choices=["auto", "cuda", "cpu", "mps"],
        help="training device hint (default auto)",
    )
    p.add_argument(
        "--merge",
        action="store_true",
        help="merge LoRA into base and save full model under out/merged",
    )
    args = p.parse_args(argv)

    try:
        import torch
        from datasets import Dataset
        from peft import LoraConfig, PeftModel
        from transformers import AutoModelForCausalLM, AutoTokenizer
        from trl import SFTConfig, SFTTrainer
    except ImportError as e:
        print(
            "Missing deps. From app/nl2yui run:\n"
            "  pip install -r requirements.txt\n"
            f"Detail: {e}",
            file=sys.stderr,
        )
        return 1

    if not args.train.exists():
        print(f"train file missing: {args.train}\nRun: python synthesize.py -n 3000", file=sys.stderr)
        return 1

    train_rows = load_jsonl(args.train)
    eval_rows = load_jsonl(args.eval) if args.eval.exists() else []

    print(f"loading model {args.model} ...")
    tokenizer = AutoTokenizer.from_pretrained(args.model, trust_remote_code=True)
    if tokenizer.pad_token is None:
        tokenizer.pad_token = tokenizer.eos_token

    def row_to_text(example: dict[str, Any]) -> dict[str, str]:
        text = tokenizer.apply_chat_template(
            build_messages(example),
            tokenize=False,
            add_generation_prompt=False,
        )
        return {"text": text}

    train_ds = Dataset.from_list([row_to_text(r) for r in train_rows])
    eval_ds = Dataset.from_list([row_to_text(r) for r in eval_rows]) if eval_rows else None

    use_cuda = torch.cuda.is_available() and args.device in ("auto", "cuda")
    use_mps = (
        args.device in ("auto", "mps")
        and hasattr(torch.backends, "mps")
        and torch.backends.mps.is_available()
        and not use_cuda
    )
    if use_cuda and getattr(torch.cuda, "is_bf16_supported", lambda: False)():
        dtype = torch.bfloat16
    elif use_cuda:
        dtype = torch.float16
    else:
        dtype = torch.float32

    model_kwargs: dict[str, Any] = {
        "trust_remote_code": True,
        "torch_dtype": dtype,
    }
    if use_cuda:
        model_kwargs["device_map"] = "auto"
    elif use_mps:
        model_kwargs["device_map"] = {"": "mps"}

    model = AutoModelForCausalLM.from_pretrained(args.model, **model_kwargs)
    model.config.use_cache = False
    if not use_cuda and not use_mps:
        model = model.to("cpu")

    peft_config = LoraConfig(
        r=args.lora_r,
        lora_alpha=args.lora_alpha,
        lora_dropout=args.lora_dropout,
        bias="none",
        task_type="CAUSAL_LM",
        target_modules=[
            "q_proj",
            "k_proj",
            "v_proj",
            "o_proj",
            "gate_proj",
            "up_proj",
            "down_proj",
        ],
    )

    args.out.mkdir(parents=True, exist_ok=True)

    # Compatible with TRL 0.11 + transformers 4.46
    sft_kwargs: dict[str, Any] = dict(
        output_dir=str(args.out),
        num_train_epochs=args.epochs,
        per_device_train_batch_size=args.batch_size,
        per_device_eval_batch_size=max(1, args.batch_size),
        gradient_accumulation_steps=args.grad_accum,
        learning_rate=args.lr,
        warmup_ratio=args.warmup_ratio,
        logging_steps=args.logging_steps,
        save_steps=args.save_steps,
        save_total_limit=2,
        bf16=bool(use_cuda and dtype == torch.bfloat16),
        fp16=bool(use_cuda and dtype == torch.float16),
        packing=False,
        report_to=[],
        seed=args.seed,
        lr_scheduler_type="cosine",
        optim="adamw_torch",
        gradient_checkpointing=True,
        dataset_text_field="text",
        max_seq_length=args.max_seq_len,
    )
    if eval_ds is not None:
        sft_kwargs["eval_strategy"] = "steps"
        sft_kwargs["eval_steps"] = args.eval_steps
    else:
        sft_kwargs["eval_strategy"] = "no"

    try:
        sft_args = SFTConfig(**sft_kwargs)
    except TypeError:
        # Older TrainingArguments: evaluation_strategy / no max_seq_length on config
        sft_kwargs.pop("max_seq_length", None)
        if "eval_strategy" in sft_kwargs:
            sft_kwargs["evaluation_strategy"] = sft_kwargs.pop("eval_strategy")
        sft_args = SFTConfig(**sft_kwargs)

    trainer = SFTTrainer(
        model=model,
        args=sft_args,
        train_dataset=train_ds,
        eval_dataset=eval_ds,
        tokenizer=tokenizer,
        peft_config=peft_config,
        dataset_text_field="text",
        max_seq_length=args.max_seq_len,
        packing=False,
    )

    print(
        f"device={'cuda' if use_cuda else ('mps' if use_mps else 'cpu')} "
        f"train={len(train_rows)} eval={len(eval_rows)}"
    )
    trainer.train()
    trainer.save_model(str(args.out))
    tokenizer.save_pretrained(str(args.out))
    meta = {
        "base_model": args.model,
        "train": str(args.train),
        "epochs": args.epochs,
        "lora_r": args.lora_r,
        "max_seq_len": args.max_seq_len,
    }
    (args.out / "nl2yui_train_meta.json").write_text(
        json.dumps(meta, ensure_ascii=False, indent=2), encoding="utf-8"
    )
    print(f"saved LoRA adapter -> {args.out}")

    if args.merge:
        merge_dir = args.out / "merged"
        print(f"merging LoRA into base -> {merge_dir}")
        base = AutoModelForCausalLM.from_pretrained(
            args.model, trust_remote_code=True, torch_dtype=dtype
        )
        merged = PeftModel.from_pretrained(base, str(args.out))
        merged = merged.merge_and_unload()
        merge_dir.mkdir(parents=True, exist_ok=True)
        merged.save_pretrained(str(merge_dir))
        tokenizer.save_pretrained(str(merge_dir))
        print(f"merged model saved -> {merge_dir}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
