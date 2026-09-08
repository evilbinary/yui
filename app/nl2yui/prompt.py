"""Prompt formatting for NL → YUI JSON SFT / inference."""

from __future__ import annotations

import json
from typing import Any, Mapping

SYSTEM_PROMPT = (
    "你是 YUI JSON 生成器。只输出一个 JSON 对象，不要解释、不要 markdown。\n"
    "mode=update：增量，格式 {\"updates\":[{\"target\":\"<id>\",\"change\":{...}}]}，"
    "必须有 change；颜色 #RRGGBB；新建放 change.children。\n"
    "mode=full：一口气输出完整 UI 树（单根组件），格式 "
    "{\"id\":\"...\",\"type\":\"View\",\"layout\":{...},\"style\":{...},\"children\":[...]}，"
    "不要包 updates，不要挂到 canvas。"
)

USER_TEMPLATE = """mode={mode}
context: {context}
user: {message}"""


def format_context(context: str | Mapping[str, Any] | list[Any] | None) -> str:
    if context is None:
        return "(none)"
    if isinstance(context, str):
        return context.strip() or "(none)"
    if isinstance(context, list):
        parts = []
        for item in context:
            if isinstance(item, Mapping):
                cid = item.get("id", "?")
                ctype = item.get("type", "?")
                text = item.get("text")
                parts.append(f"{cid}:{ctype}" + (f":{text}" if text is not None else ""))
            else:
                parts.append(str(item))
        return ", ".join(parts) if parts else "(none)"
    if isinstance(context, Mapping):
        if "ids" in context:
            return format_context(context["ids"])
        return ", ".join(f"{k}:{v}" for k, v in context.items()) or "(none)"
    return str(context)


def build_user_prompt(
    message: str,
    *,
    context: str | Mapping[str, Any] | list[Any] | None = None,
    mode: str = "update",
) -> str:
    return USER_TEMPLATE.format(
        mode=mode or "update",
        context=format_context(context),
        message=(message or "").strip(),
    )


def build_messages(example: Mapping[str, Any]) -> list[dict[str, str]]:
    """Chat messages for instruct models (Qwen / Llama style)."""
    output = example["output"]
    if not isinstance(output, str):
        output = json.dumps(output, ensure_ascii=False, separators=(",", ":"))
    return [
        {"role": "system", "content": SYSTEM_PROMPT},
        {
            "role": "user",
            "content": build_user_prompt(
                example.get("message", ""),
                context=example.get("context"),
                mode=example.get("mode", "update"),
            ),
        },
        {"role": "assistant", "content": output},
    ]


def build_inference_messages(
    message: str,
    *,
    context: str | Mapping[str, Any] | list[Any] | None = None,
    mode: str = "update",
) -> list[dict[str, str]]:
    return [
        {"role": "system", "content": SYSTEM_PROMPT},
        {
            "role": "user",
            "content": build_user_prompt(message, context=context, mode=mode),
        },
    ]
