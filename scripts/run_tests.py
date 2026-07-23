#!/usr/bin/env python3
# coding:utf-8
"""YUI test runner — unit (cmocka) + integration (playground + YTest).

Usage:
  python scripts/run_tests.py
  python scripts/run_tests.py --unit
  python scripts/run_tests.py --integration
  python scripts/run_tests.py --filter layer
"""
from __future__ import print_function

import argparse
import glob
import json
import os
import subprocess
import sys
import time

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
UNIT_DIR = os.path.join(ROOT, "tests", "unit")
INTEGRATION_DIR = os.path.join(ROOT, "tests", "integration")


def _mingw_env():
    env = os.environ.copy()
    mingw = env.get("MINGW64") or env.get("MINGW64_HOME")
    candidates = [
        mingw,
        r"E:\soft\msys2\mingw64",
        r"C:\msys64\mingw64",
    ]
    for root in candidates:
        if not root:
            continue
        bin_dir = os.path.join(root, "bin")
        if os.path.isdir(bin_dir):
            env["PATH"] = bin_dir + os.pathsep + env.get("PATH", "")
            env["MINGW64"] = root
            break
    env["YUI_AUTO_TEST"] = "1"
    if "YUI_AUTO_FRAMES" not in env:
        env["YUI_AUTO_FRAMES"] = "120"
    return env


def _find_bin(name):
    """Locate built binary under build/."""
    patterns = [
        os.path.join(ROOT, "build", "*", "*", "*", name + ".exe"),
        os.path.join(ROOT, "build", "*", "*", "*", name),
        os.path.join(ROOT, "build", name + ".exe"),
        os.path.join(ROOT, "build", name),
    ]
    matches = []
    for pat in patterns:
        matches.extend(glob.glob(pat))
    if not matches:
        return None
    matches.sort(key=lambda p: os.path.getmtime(p), reverse=True)
    return matches[0]


def _run(cmd, env, timeout=120, cwd=None):
    t0 = time.time()
    try:
        p = subprocess.run(
            cmd,
            env=env,
            cwd=cwd or ROOT,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            timeout=timeout,
            universal_newlines=True,
            encoding="utf-8",
            errors="replace",
        )
        out = p.stdout or ""
        return p.returncode, out, time.time() - t0
    except subprocess.TimeoutExpired as e:
        out = e.stdout or ""
        if isinstance(out, bytes):
            out = out.decode("utf-8", errors="replace")
        return 124, out + "\n[runner] timeout\n", time.time() - t0


def _ya_build(targets, env):
    cmd = ["ya", "-b"] + list(targets)
    code, out, _ = _run(cmd, env, timeout=600)
    if code != 0:
        print(out)
        print("[runner] build failed:", " ".join(targets))
    return code == 0


def discover_unit():
    names = []
    for path in sorted(glob.glob(os.path.join(UNIT_DIR, "test_*.c"))):
        names.append(os.path.splitext(os.path.basename(path))[0])
    return names


def discover_integration():
    cases = []
    for path in sorted(glob.glob(os.path.join(INTEGRATION_DIR, "*.json"))):
        try:
            with open(path, "r", encoding="utf-8") as f:
                data = json.load(f)
        except Exception:
            continue
        if data.get("autoTest"):
            cases.append(path)
    return cases


def run_unit(env, filt):
    names = [n for n in discover_unit() if not filt or filt in n]
    if not names:
        print("[unit] no tests")
        return []

    print("[unit] building", len(names), "target(s)...")
    if not _ya_build(names, env):
        return [("BUILD", n, False, 0.0, "build failed") for n in names]

    results = []
    for name in names:
        bin_path = _find_bin(name)
        if not bin_path:
            results.append(("unit", name, False, 0.0, "binary not found"))
            print("[unit] FAIL", name, "(binary not found)")
            continue
        code, out, dt = _run([bin_path], env, timeout=60)
        ok = code == 0
        status = "PASS" if ok else "FAIL"
        print("[unit] %s %s (%.2fs, exit=%d)" % (status, name, dt, code))
        if not ok and out:
            # show last lines
            lines = out.strip().splitlines()
            for line in lines[-20:]:
                print("   ", line)
        results.append(("unit", name, ok, dt, out if not ok else ""))
    return results


def run_integration(env, filt):
    cases = [p for p in discover_integration() if not filt or filt in os.path.basename(p)]
    if not cases:
        print("[integration] no autoTest JSON")
        return []

    print("[integration] building playground...")
    if not _ya_build(["playground"], env):
        return [("integration", "playground", False, 0.0, "build failed")]

    playground = _find_bin("playground")
    if not playground:
        print("[integration] FAIL playground binary not found")
        return [("integration", "playground", False, 0.0, "binary not found")]

    results = []
    for path in cases:
        rel = os.path.relpath(path, ROOT).replace("\\", "/")
        name = os.path.basename(path)
        code, out, dt = _run(
            [playground, "--auto", "--frames=120", rel],
            env,
            timeout=180,
        )
        ok = code == 0
        for line in out.splitlines():
            if "YTEST_RESULT" not in line:
                continue
            for token in line.split():
                if token.startswith("failed="):
                    try:
                        if int(token.split("=", 1)[1]) > 0:
                            ok = False
                    except ValueError:
                        pass
        status = "PASS" if ok else "FAIL"
        print("[integration] %s %s (%.2fs, exit=%d)" % (status, name, dt, code))
        if not ok and out:
            for line in out.strip().splitlines()[-30:]:
                print("   ", line)
        results.append(("integration", name, ok, dt, out if not ok else ""))
    return results


def main():
    ap = argparse.ArgumentParser(description="YUI unit + integration test runner")
    ap.add_argument("--unit", action="store_true", help="run C/cmocka unit tests only")
    ap.add_argument("--integration", action="store_true", help="run JS integration tests only")
    ap.add_argument("--filter", default="", help="substring filter on test name")
    args = ap.parse_args()

    run_u = args.unit or (not args.unit and not args.integration)
    run_i = args.integration or (not args.unit and not args.integration)

    env = _mingw_env()
    os.chdir(ROOT)

    results = []
    if run_u:
        results.extend(run_unit(env, args.filter))
    if run_i:
        results.extend(run_integration(env, args.filter))

    passed = sum(1 for r in results if r[2])
    failed = sum(1 for r in results if not r[2])
    print("--------")
    print("%d passed, %d failed, %d total" % (passed, failed, len(results)))
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
