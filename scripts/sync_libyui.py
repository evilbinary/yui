#!/usr/bin/env python3
# coding:utf-8
"""把本仓库的 .c / .h 同步到 YiYiYa eggs/libyui。

只写入目标里已经存在的目录，不创建新文件夹。
默认源：src/、lib/（相对本仓库根）。

Usage:
  python3 scripts/sync_libyui.py
  python3 scripts/sync_libyui.py --dry-run
  python3 scripts/sync_libyui.py --dest /path/to/libyui
"""
from __future__ import print_function

import argparse
import os
import shutil
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
DEFAULT_DEST = "/media/evil/d/dev/YiYiYa/eggs/libyui"
DEFAULT_TREES = ("src", "lib")
SKIP_DIR_NAMES = {
    ".git",
    "__pycache__",
    "build",
    ".cache",
}


def iter_c_h(src_root):
    for dirpath, dirnames, filenames in os.walk(src_root):
        dirnames[:] = [d for d in dirnames if d not in SKIP_DIR_NAMES]
        for name in filenames:
            if name.endswith(".c") or name.endswith(".h"):
                yield os.path.join(dirpath, name)


def main():
    parser = argparse.ArgumentParser(description="Sync .c/.h into existing libyui dirs only")
    parser.add_argument("--dest", default=DEFAULT_DEST, help="libyui 根目录")
    parser.add_argument(
        "--tree",
        action="append",
        dest="trees",
        help="相对仓库根的源目录，可重复；默认 src 和 lib",
    )
    parser.add_argument("--dry-run", action="store_true", help="只打印，不拷贝")
    args = parser.parse_args()

    dest_root = os.path.abspath(args.dest)
    if not os.path.isdir(dest_root):
        print("dest not found: %s" % dest_root, file=sys.stderr)
        return 1

    trees = args.trees if args.trees else list(DEFAULT_TREES)
    copied = 0
    skipped_no_dir = 0
    skipped_missing_src = 0

    for tree in trees:
        src_tree = os.path.join(ROOT, tree)
        if not os.path.isdir(src_tree):
            print("skip missing source tree: %s" % src_tree)
            skipped_missing_src += 1
            continue
        for src in iter_c_h(src_tree):
            rel = os.path.relpath(src, ROOT)
            dest = os.path.join(dest_root, rel)
            dest_dir = os.path.dirname(dest)
            if not os.path.isdir(dest_dir):
                skipped_no_dir += 1
                continue
            if args.dry_run:
                print("copy %s" % rel)
            else:
                shutil.copy2(src, dest)
            copied += 1

    print(
        "sync %s -> %s  copied=%d  skip_no_dir=%d%s"
        % (
            ROOT,
            dest_root,
            copied,
            skipped_no_dir,
            "  (dry-run)" if args.dry_run else "",
        )
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
