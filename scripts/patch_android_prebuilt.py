#!/usr/bin/env python3
"""Patch third_party/yui-prebuilt/android libyui.a with updated event.c + input/state.c.

Use when ya -p android full prebuilt rebuild is unavailable but JNI link fails on
missing handle_window_event / input_state_* symbols.
"""
from __future__ import annotations

import os
import shutil
import subprocess
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
NDK = os.environ.get("ANDROID_NDK_HOME") or os.environ.get("ANDROID_NDK_ROOT") or ""
HOST = "windows-x86_64" if os.name == "nt" else "linux-x86_64"
ABIS = {
    "arm64-v8a": "aarch64-linux-android21",
    "armeabi-v7a": "armv7a-linux-androideabi21",
}
SOURCES = [
    ("src/event.c", "event.o"),
    ("src/input/state.c", "state.o"),
]
CFLAGS = [
    "-fPIC",
    "-g",
    "-D__ANDROID__",
    "-DYUI_BACKEND_MOBILE",
    "-DYUI_WITH_GAME=1",
    "-DYUI_WITH_GAME_AUDIO=1",
    f"-I{os.path.join(ROOT, 'src')}",
    f"-I{os.path.join(ROOT, 'src', 'components')}",
    f"-I{os.path.join(ROOT, 'lib', 'cjson')}",
    f"-I{os.path.join(ROOT, 'lib', 'miniaudio')}",
]


def tool(name: str) -> str:
    if not NDK:
        raise SystemExit("ANDROID_NDK_HOME or ANDROID_NDK_ROOT is required")
    return os.path.join(NDK, "toolchains", "llvm", "prebuilt", HOST, "bin", name)


def patch_abi(abi: str, triple: str) -> None:
    clang = tool("clang")
    ar = tool("llvm-ar")
    work = os.path.join(ROOT, "build", "android-patch", abi)
    lib = os.path.join(ROOT, "third_party", "yui-prebuilt", "android", abi, "libyui.a")
    if not os.path.isfile(lib):
        print(f"[skip] missing {lib}")
        return
    os.makedirs(work, exist_ok=True)
    backup = os.path.join(work, "libyui.a.bak")
    if not os.path.isfile(backup):
        shutil.copy2(lib, backup)
    objects = []
    for src_rel, obj_name in SOURCES:
        src = os.path.join(ROOT, src_rel)
        obj = os.path.join(work, obj_name)
        cmd = [clang, f"--target={triple}", *CFLAGS, "-c", src, "-o", obj]
        print("[compile]", " ".join(cmd))
        subprocess.check_call(cmd)
        objects.append(obj)
    for obj_name, _ in SOURCES:
        subprocess.call([ar, "d", lib, obj_name], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.check_call([ar, "r", lib, *objects])
    print(f"[patched] {lib}")


def main() -> int:
    abis = sys.argv[1:] or list(ABIS.keys())
    for abi in abis:
        triple = ABIS.get(abi)
        if not triple:
            print(f"unknown abi: {abi}", file=sys.stderr)
            return 1
        patch_abi(abi, triple)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
