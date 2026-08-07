#!/usr/bin/env python3
"""
Pre-compile watch-os JS files to mquickjs 32-bit bytecode (.bc).

For every `xxx.js` under app/watch-os and app/lib, runs the bc-gen host tool
(compiled with the YUI stdlib table via ymake) with `-m32` so the bytecode
carries the same YUI/Socket atoms as the ESP32 runtime. The embedded runtime
loads bytecode via JS_LoadBytecode() instead of parsing source (saves peak
JS-pool memory).

Usage:
  python3 scripts/precompile_js.py [--bcgen PATH] [--out DIR]
"""
import argparse
import os
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WATCH = os.path.join(ROOT, "app", "watch-os")
LIB = os.path.join(ROOT, "app", "lib")

# bc-gen needs the MINGW64 toolchain on PATH (gcc subprocess loads its DLLs).


def find_bcgen():
    candidates = [
        os.environ.get("BCGEN"),
        # 32 位独立可执行（JSW=4 直接产出 32 位字节码，atom 与 ESP32 匹配）
        os.path.join(ROOT, "scripts", "mqjs32", "bc-gen.exe"),
        os.path.join(ROOT, "build", "pc", "None", "None", "bc-gen.exe"),
        os.path.join(ROOT, "build", "pc", "None", "None", "bc-gen"),
        "bc-gen",
    ]
    for c in candidates:
        if not c:
            continue
        # bc-gen.exe 是 Windows 宿主工具：仅 Windows 上可用，
        # Linux/macOS 上直接执行会报 "Exec format error"，必须跳过。
        if sys.platform != "win32" and c.lower().endswith(".exe"):
            continue
        if shutil.which(c):
            return os.path.abspath(shutil.which(c))
        if os.path.isfile(c):
            return os.path.abspath(c)
    return None


def msys_path(p):
    """Convert a Windows path to an MSYS2-style path (E:/x -> /e/x)."""
    p = p.replace("\\", "/")
    drive, rest = "", p
    if len(p) >= 2 and p[1] == ":":
        drive, rest = p[0], p[2:]
    if drive:
        return f"/{drive.lower()}{rest}"
    return p


def compile_dir(bcgen, src_dir, out_dir):
    """Compile every .js under src_dir into out_dir/<rel>.bc (32-bit)."""
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
            # bc-gen.exe 是 32 位工具（JSW=4），直接产出 32 位字节码，无 -m32。
            try:
                res = subprocess.run([bcgen, "-o", dst, src],
                                     capture_output=True, text=True)
            except OSError as e:
                print(f"WARN: cannot run bc-gen ({e}), skipping {rel}")
                continue
            ok = res.returncode == 0
            if not ok and os.path.isfile(dst) and os.path.getsize(dst) > 0:
                ok = True
            if not ok:
                print(f"WARN: compile failed {rel}: rc={res.returncode}")
                print(f"  out: {res.stdout.strip()[:300]}")
                print(f"  err: {res.stderr.strip()[:300]}")
                continue
            n += 1
    return n


def main():
    ap = argparse.ArgumentParser(description="precompile JS to mquickjs bytecode")
    ap.add_argument("--bcgen", help="path to bc-gen host tool")
    ap.add_argument("--out", help="output staging dir (default build/spiffs-staging)")
    args = ap.parse_args()

    bcgen = args.bcgen or find_bcgen()
    if bcgen:
        bcgen = os.path.abspath(bcgen)
        # ymake emits bc-gen.exe on Windows (no extension when cross-build)
        if sys.platform == "win32" and not bcgen.endswith(".exe") and \
           os.path.exists(bcgen + ".exe"):
            bcgen = bcgen + ".exe"
    if not bcgen:
        print("ERROR: bc-gen host tool not found (build with: ya -p pc -b bc-gen)",
              file=sys.stderr)
        sys.exit(1)
    print(f"Using bc-gen: {bcgen}")

    staging = args.out or os.path.join(ROOT, "build", "esp32", "esp32c3", "spiffs-staging")
    os.makedirs(staging, exist_ok=True)

    n_watch = compile_dir(bcgen, WATCH, staging)
    print(f"watch-os: compiled {n_watch} files")

    lib_staging = os.path.join(staging, "lib")
    os.makedirs(lib_staging, exist_ok=True)
    n_lib = compile_dir(bcgen, LIB, lib_staging)
    print(f"app/lib: compiled {n_lib} files")
    print(f"done. staged .bc under {staging}")


if __name__ == "__main__":
    main()
