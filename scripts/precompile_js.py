#!/usr/bin/env python3
"""
Pre-compile watch-os JS files to mquickjs 32-bit bytecode (.bc).

For every `xxx.js` under app/watch-os and app/lib, runs the mqjs host tool
with `-o xxx.bc -m32` so the embedded ESP32 runtime can load bytecode via
JS_LoadBytecode() instead of parsing source (saves peak JS-pool memory).

Usage:
  python3 scripts/precompile_js.py [--mqjs PATH] [--out DIR]
"""
import argparse
import os
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WATCH = os.path.join(ROOT, "app", "watch-os")
LIB = os.path.join(ROOT, "app", "lib")


def find_mqjs():
    candidates = [
        os.environ.get("MQJS"),
        os.path.join(ROOT, "scripts", "mqjs32", "mqjs32.exe"),
        "mqjs",
    ]
    for c in candidates:
        if c and shutil.which(c):
            return c
        if c and os.path.isfile(c):
            return c
    return None


def compile_dir(mqjs, src_dir, out_dir):
    """Compile every .js under src_dir into out_dir/<rel>.bc

    The 32-bit mqjs host tool emits 32-bit bytecode natively (JSW == 4),
    so no -m32 conversion is needed (the -m32 path in 64-bit builds crashes
    on large scripts)."""
    n = 0
    for dirpath, _dirs, filenames in os.walk(src_dir):
        rel_dir = os.path.relpath(dirpath, src_dir).replace("\\", "/")
        for fn in filenames:
            if not fn.endswith(".js"):
                continue
            src = os.path.join(dirpath, fn)
            rel = f"{rel_dir}/{fn}" if rel_dir != "." else fn
            # SPIFFS obj name limit: '/' + name must be < 32 chars. Match the
            # make_spiffs skip rule so we never stage a .bc for a skipped .js.
            if len(rel) >= 32:
                continue
            bc_name = fn[:-3] + ".bc"
            dst_rel = f"{rel_dir}/{bc_name}" if rel_dir != "." else bc_name
            dst = os.path.join(out_dir, dst_rel)
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            cmd = [mqjs, "-o", dst, src]
            res = subprocess.run(cmd, capture_output=True, text=True)
            if res.returncode != 0:
                print(f"WARN: compile failed {rel}: {res.stderr.strip()[:200]}")
                continue
            n += 1
    return n


def main():
    ap = argparse.ArgumentParser(description="precompile JS to mquickjs bytecode")
    ap.add_argument("--mqjs", help="path to mqjs host tool")
    ap.add_argument("--out", help="output staging dir (default build/spiffs-staging)")
    args = ap.parse_args()

    mqjs = args.mqjs or find_mqjs()
    if not mqjs:
        print("ERROR: mqjs host tool not found", file=sys.stderr)
        sys.exit(1)
    print(f"Using mqjs: {mqjs}")

    staging = args.out or os.path.join(ROOT, "build", "esp32", "esp32c3", "spiffs-staging")
    os.makedirs(staging, exist_ok=True)

    n_watch = compile_dir(mqjs, WATCH, staging)
    print(f"watch-os: compiled {n_watch} files")

    lib_staging = os.path.join(staging, "lib")
    os.makedirs(lib_staging, exist_ok=True)
    n_lib = compile_dir(mqjs, LIB, lib_staging)
    print(f"app/lib: compiled {n_lib} files")
    print(f"done. staged .bc under {staging}")


if __name__ == "__main__":
    main()
