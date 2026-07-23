#!/usr/bin/env python3
# coding:utf-8
"""YUI test runner — unit / integration / perf / visual / e2e.

Usage:
  python scripts/run_tests.py
  python scripts/run_tests.py --unit
  python scripts/run_tests.py --integration
  python scripts/run_tests.py --perf
  python scripts/run_tests.py --visual
  python scripts/run_tests.py --visual --update-baselines
  python scripts/run_tests.py --e2e
  python scripts/run_tests.py --filter layer
"""
from __future__ import print_function

import argparse
import glob
import json
import os
import struct
import subprocess
import sys
import time
import zlib

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
UNIT_DIR = os.path.join(ROOT, "tests", "unit")
INTEGRATION_DIR = os.path.join(ROOT, "tests", "integration")
PERF_DIR = os.path.join(ROOT, "tests", "perf")
E2E_DIR = os.path.join(ROOT, "tests", "e2e")
VISUAL_CASES_DIR = os.path.join(ROOT, "tests", "visual", "cases")
VISUAL_BASELINE_DIR = os.path.join(ROOT, "tests", "visual", "baselines")
VISUAL_OUTPUT_DIR = os.path.join(ROOT, "tests", "visual", "output")

# Soft pixel tolerance for font/AA drift across machines (solid-color scenes stay near 0).
VISUAL_MAX_DIFF_RATIO = 0.02
VISUAL_MAX_CHANNEL_DELTA = 8


def _safe_print_indent(line):
    try:
        print("   ", line)
    except UnicodeEncodeError:
        print("   ", line.encode("ascii", errors="replace").decode("ascii"))


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
    env["YUI_HEADLESS"] = "1"
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


def _ytest_failed(out):
    ok = True
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
    return not ok


def _discover_auto_json(directory):
    cases = []
    for path in sorted(glob.glob(os.path.join(directory, "*.json"))):
        try:
            with open(path, "r", encoding="utf-8") as f:
                data = json.load(f)
        except Exception:
            continue
        if data.get("autoTest"):
            cases.append(path)
    return cases


def discover_unit():
    names = []
    for path in sorted(glob.glob(os.path.join(UNIT_DIR, "test_*.c"))):
        names.append(os.path.splitext(os.path.basename(path))[0])
    return names


def discover_integration():
    return _discover_auto_json(INTEGRATION_DIR)


def discover_perf():
    return _discover_auto_json(PERF_DIR)


def discover_e2e():
    return _discover_auto_json(E2E_DIR)


def discover_visual():
    return _discover_auto_json(VISUAL_CASES_DIR)


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
            lines = out.strip().splitlines()
            for line in lines[-20:]:
                _safe_print_indent(line)
        results.append(("unit", name, ok, dt, out if not ok else ""))
    return results


def _run_playground_cases(label, cases, env, filt, frames=120, timeout=180):
    cases = [p for p in cases if not filt or filt in os.path.basename(p)]
    if not cases:
        print("[%s] no autoTest JSON" % label)
        return []

    print("[%s] building playground..." % label)
    if not _ya_build(["playground"], env):
        return [(label, "playground", False, 0.0, "build failed")]

    playground = _find_bin("playground")
    if not playground:
        print("[%s] FAIL playground binary not found" % label)
        return [(label, "playground", False, 0.0, "binary not found")]

    results = []
    for path in cases:
        rel = os.path.relpath(path, ROOT).replace("\\", "/")
        name = os.path.basename(path)
        code, out, dt = _run(
            [playground, "--auto", "--frames=%d" % frames, rel],
            env,
            timeout=timeout,
        )
        ok = code == 0 and not _ytest_failed(out)
        status = "PASS" if ok else "FAIL"
        print("[%s] %s %s (%.2fs, exit=%d)" % (label, status, name, dt, code))
        if not ok and out:
            for line in out.strip().splitlines()[-30:]:
                _safe_print_indent(line)
        results.append((label, name, ok, dt, out if not ok else ""))
    return results


def run_integration(env, filt):
    return _run_playground_cases("integration", discover_integration(), env, filt)


def run_perf(env, filt):
    return _run_playground_cases("perf", discover_perf(), env, filt, frames=180)


def run_e2e(env, filt):
    return _run_playground_cases("e2e", discover_e2e(), env, filt, frames=180)


# --- PNG baseline compare (stdlib only) ------------------------------------


def _png_paeth(a, b, c):
    p = a + b - c
    pa = abs(p - a)
    pb = abs(p - b)
    pc = abs(p - c)
    if pa <= pb and pa <= pc:
        return a
    if pb <= pc:
        return b
    return c


def read_png_rgba(path):
    """Decode 8-bit RGB/RGBA non-interlaced PNG → (w, h, bytearray RGBA)."""
    with open(path, "rb") as f:
        data = f.read()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("not a PNG: %s" % path)

    pos = 8
    width = height = None
    bit_depth = color_type = None
    idat = []
    while pos + 8 <= len(data):
        length = struct.unpack(">I", data[pos : pos + 4])[0]
        ctype = data[pos + 4 : pos + 8]
        chunk = data[pos + 8 : pos + 8 + length]
        pos += 12 + length
        if ctype == b"IHDR":
            width, height, bit_depth, color_type, comp, filt, inter = struct.unpack(
                ">IIBBBBB", chunk
            )
            if bit_depth != 8 or inter != 0 or comp != 0:
                raise ValueError("unsupported PNG (need 8-bit non-interlaced): %s" % path)
            if color_type not in (2, 6):
                raise ValueError("unsupported color type %d in %s" % (color_type, path))
        elif ctype == b"IDAT":
            idat.append(chunk)
        elif ctype == b"IEND":
            break

    if width is None:
        raise ValueError("missing IHDR: %s" % path)

    raw = zlib.decompress(b"".join(idat))
    channels = 3 if color_type == 2 else 4
    stride = width * channels
    expected = (stride + 1) * height
    if len(raw) < expected:
        raise ValueError("truncated IDAT in %s" % path)

    out = bytearray(width * height * 4)
    prev = bytearray(stride)
    offset = 0
    for y in range(height):
        ftype = raw[offset]
        offset += 1
        row = bytearray(raw[offset : offset + stride])
        offset += stride
        for i in range(stride):
            left = row[i - channels] if i >= channels else 0
            up = prev[i]
            up_left = prev[i - channels] if i >= channels else 0
            x = row[i]
            if ftype == 0:
                val = x
            elif ftype == 1:
                val = (x + left) & 255
            elif ftype == 2:
                val = (x + up) & 255
            elif ftype == 3:
                val = (x + ((left + up) >> 1)) & 255
            elif ftype == 4:
                val = (x + _png_paeth(left, up, up_left)) & 255
            else:
                raise ValueError("bad filter %d in %s" % (ftype, path))
            row[i] = val
        prev = row
        for x in range(width):
            si = x * channels
            di = (y * width + x) * 4
            out[di] = row[si]
            out[di + 1] = row[si + 1]
            out[di + 2] = row[si + 2]
            out[di + 3] = 255 if channels == 3 else row[si + 3]
    return width, height, out


def compare_png(baseline, actual, max_ratio=VISUAL_MAX_DIFF_RATIO, max_delta=VISUAL_MAX_CHANNEL_DELTA):
    bw, bh, bpx = read_png_rgba(baseline)
    aw, ah, apx = read_png_rgba(actual)
    if bw != aw or bh != ah:
        return False, "size mismatch baseline=%dx%d actual=%dx%d" % (bw, bh, aw, ah)

    total = bw * bh
    diff = 0
    for i in range(0, len(bpx), 4):
        if (
            abs(bpx[i] - apx[i]) > max_delta
            or abs(bpx[i + 1] - apx[i + 1]) > max_delta
            or abs(bpx[i + 2] - apx[i + 2]) > max_delta
        ):
            diff += 1
    ratio = float(diff) / float(total) if total else 1.0
    if ratio > max_ratio:
        return False, "diff pixels=%d/%d (%.2f%% > %.2f%%)" % (
            diff,
            total,
            ratio * 100.0,
            max_ratio * 100.0,
        )
    return True, "diff pixels=%d/%d (%.2f%%)" % (diff, total, ratio * 100.0)


def _visual_names(json_path):
    base = os.path.splitext(os.path.basename(json_path))[0]
    # test-blocks.json → blocks.png (matches JS VISUAL_OUT convention)
    if base.startswith("test-"):
        stem = base[5:]
    else:
        stem = base
    return stem + ".png"


def run_visual(env, filt, update_baselines=False):
    cases = [p for p in discover_visual() if not filt or filt in os.path.basename(p)]
    if not cases:
        print("[visual] no autoTest JSON")
        return []

    os.makedirs(VISUAL_OUTPUT_DIR, exist_ok=True)
    os.makedirs(VISUAL_BASELINE_DIR, exist_ok=True)

    print("[visual] building playground...")
    if not _ya_build(["playground"], env):
        return [("visual", "playground", False, 0.0, "build failed")]

    playground = _find_bin("playground")
    if not playground:
        print("[visual] FAIL playground binary not found")
        return [("visual", "playground", False, 0.0, "binary not found")]

    results = []
    for path in cases:
        rel = os.path.relpath(path, ROOT).replace("\\", "/")
        name = os.path.basename(path)
        png_name = _visual_names(path)
        out_png = os.path.join(VISUAL_OUTPUT_DIR, png_name)
        base_png = os.path.join(VISUAL_BASELINE_DIR, png_name)

        if os.path.isfile(out_png):
            try:
                os.remove(out_png)
            except OSError:
                pass

        code, out, dt = _run(
            [playground, "--auto", "--frames=120", rel],
            env,
            timeout=180,
        )
        ok = code == 0 and not _ytest_failed(out)

        if ok and not os.path.isfile(out_png):
            ok = False
            msg = "screenshot missing: %s" % out_png
        elif ok and update_baselines:
            import shutil

            shutil.copy2(out_png, base_png)
            msg = "baseline updated: %s" % os.path.relpath(base_png, ROOT)
            print("[visual] UPDATE", name, "->", png_name)
        elif ok and not os.path.isfile(base_png):
            ok = False
            msg = "no baseline (run with --update-baselines): %s" % base_png
        elif ok:
            same, msg = compare_png(base_png, out_png)
            ok = same
        else:
            msg = "playground exit=%d" % code

        status = "PASS" if ok else "FAIL"
        print("[visual] %s %s (%.2fs) %s" % (status, name, dt, msg))
        if not ok and out:
            for line in out.strip().splitlines()[-20:]:
                _safe_print_indent(line)
        results.append(("visual", name, ok, dt, "" if ok else msg))
    return results


def main():
    ap = argparse.ArgumentParser(description="YUI test runner")
    ap.add_argument("--unit", action="store_true", help="run C/cmocka unit tests")
    ap.add_argument("--integration", action="store_true", help="run JS integration tests")
    ap.add_argument("--perf", action="store_true", help="run perf budget tests")
    ap.add_argument("--e2e", action="store_true", help="run end-to-end interaction tests")
    ap.add_argument("--visual", action="store_true", help="run visual regression tests")
    ap.add_argument(
        "--update-baselines",
        action="store_true",
        help="copy visual output PNGs into baselines/",
    )
    ap.add_argument("--filter", default="", help="substring filter on test name")
    ap.add_argument(
        "--all",
        action="store_true",
        help="run unit + integration + perf + visual + e2e",
    )
    args = ap.parse_args()

    any_flag = (
        args.unit
        or args.integration
        or args.perf
        or args.visual
        or args.e2e
        or args.all
    )
    # Default suite stays fast: unit + integration only.
    run_u = args.unit or args.all or not any_flag
    run_i = args.integration or args.all or not any_flag
    run_p = args.perf or args.all
    run_v = args.visual or args.all
    run_e = args.e2e or args.all

    env = _mingw_env()
    os.chdir(ROOT)

    results = []
    if run_u:
        results.extend(run_unit(env, args.filter))
    if run_i:
        results.extend(run_integration(env, args.filter))
    if run_e:
        results.extend(run_e2e(env, args.filter))
    if run_p:
        results.extend(run_perf(env, args.filter))
    if run_v:
        results.extend(run_visual(env, args.filter, update_baselines=args.update_baselines))

    passed = sum(1 for r in results if r[2])
    failed = sum(1 for r in results if not r[2])
    print("--------")
    print("%d passed, %d failed, %d total" % (passed, failed, len(results)))
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
