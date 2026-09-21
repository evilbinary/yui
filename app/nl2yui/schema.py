"""Lightweight validation for NL→YUI training samples / model outputs."""

from __future__ import annotations

import json
import re
from typing import Any

ALLOWED_TYPES = frozenset(
    {
        "View",
        "Label",
        "Button",
        "Input",
        "Image",
        "Loading",
        "Progress",
        "List",
        "Grid",
        "Checkbox",
        "Select",
        "Dialog",
    }
)

# Flat change keys for updates; create may nest style/events/children.
ALLOWED_CHANGE_KEYS = frozenset(
    {
        "text",
        "label",
        "placeholder",
        "color",
        "bgColor",
        "fontSize",
        "borderRadius",
        "size",
        "position",
        "layout",
        "flex",
        "padding",
        "visible",
        "enabled",
        "source",
        "variant",
        "value",
        "data",
        "children",
        "events",
        "style",
        "type",
        "id",
        "width",
        "height",
        "opacity",
        "strokeWidth",
        "trackColor",
        "speed",
        "max",
        "min",
        "step",
    }
)

COLOR_RE = re.compile(r"^#[0-9A-Fa-f]{6}$")


def parse_model_json(text: str) -> Any:
    """Parse model output; strip optional ```json fences."""
    s = (text or "").strip()
    if s.startswith("```"):
        lines = s.splitlines()
        if lines and lines[0].startswith("```"):
            lines = lines[1:]
        if lines and lines[-1].strip() == "```":
            lines = lines[:-1]
        s = "\n".join(lines).strip()
    return json.loads(s)


def _check_component(node: Any, path: str, errors: list[str]) -> None:
    if not isinstance(node, dict):
        errors.append(f"{path}: component must be object")
        return
    ctype = node.get("type")
    if ctype is not None and ctype not in ALLOWED_TYPES:
        errors.append(f"{path}: type {ctype!r} not in whitelist")
    style = node.get("style")
    if isinstance(style, dict):
        for k, v in style.items():
            if k in ("color", "bgColor") and isinstance(v, str) and not COLOR_RE.match(v):
                errors.append(f"{path}.style.{k}: bad color {v!r}")
    children = node.get("children")
    if children is not None:
        if not isinstance(children, list):
            errors.append(f"{path}.children: must be array")
        else:
            for i, child in enumerate(children):
                _check_component(child, f"{path}.children[{i}]", errors)


def _check_change(change: Any, path: str, errors: list[str]) -> None:
    if not isinstance(change, dict) or not change:
        errors.append(f"{path}: change must be non-empty object")
        return
    for key, value in change.items():
        if key not in ALLOWED_CHANGE_KEYS:
            errors.append(f"{path}: unknown key {key!r}")
            continue
        if key in ("color", "bgColor") and isinstance(value, str) and value is not None:
            if not COLOR_RE.match(value):
                errors.append(f"{path}.{key}: bad color {value!r}")
        if key == "children":
            if value is None:
                continue
            if not isinstance(value, list):
                errors.append(f"{path}.children: must be array or null")
            else:
                for i, child in enumerate(value):
                    _check_component(child, f"{path}.children[{i}]", errors)


def validate_updates(obj: Any) -> list[str]:
    """Validate incremental {updates:[...]} output."""
    errors: list[str] = []
    if not isinstance(obj, dict):
        return ["root must be object"]
    if "updates" not in obj:
        return ["missing updates"]
    updates = obj["updates"]
    if not isinstance(updates, list) or not updates:
        return ["updates must be non-empty array"]
    if len(updates) > 8:
        errors.append("updates length > 8")
    for i, item in enumerate(updates):
        p = f"updates[{i}]"
        if not isinstance(item, dict):
            errors.append(f"{p}: must be object")
            continue
        if "target" not in item or not isinstance(item["target"], str) or not item["target"]:
            errors.append(f"{p}: missing target string")
        if "change" not in item:
            errors.append(f"{p}: missing change")
        else:
            _check_change(item["change"], f"{p}.change", errors)
    extra = set(obj.keys()) - {"updates"}
    if extra:
        errors.append(f"unexpected root keys: {sorted(extra)}")
    return errors


def validate_full_tree(obj: Any) -> list[str]:
    """Validate mode=full: one complete UI root component (not updates)."""
    if not isinstance(obj, dict):
        return ["full root must be object"]
    if "updates" in obj:
        return ["full mode must NOT use updates; emit a complete UI tree"]
    if "type" not in obj:
        return ["full root missing type"]
    if obj.get("type") not in ALLOWED_TYPES:
        return [f"full root type {obj.get('type')!r} not in whitelist"]
    if "id" not in obj or not isinstance(obj["id"], str) or not obj["id"]:
        return ["full root missing id"]
    errors: list[str] = []
    _check_component(obj, "root", errors)
    return errors


def validate_output(obj: Any, mode: str = "update") -> list[str]:
    """Return list of error strings; empty means OK. Mode-aware."""
    if (mode or "update") == "full":
        return validate_full_tree(obj)
    return validate_updates(obj)


def validate_example(example: dict[str, Any]) -> list[str]:
    errors: list[str] = []
    if not example.get("message"):
        errors.append("missing message")
    mode = example.get("mode", "update")
    output = example.get("output")
    if output is None:
        errors.append("missing output")
    else:
        if isinstance(output, str):
            try:
                output = json.loads(output)
            except json.JSONDecodeError as e:
                return errors + [f"output not JSON: {e}"]
        errors.extend(validate_output(output, mode=mode))
    return errors
