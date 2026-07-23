# coding:utf-8
# *******************************************************************
# * Copyright 2025-present evilbinary
# * 作者: evilbinary on 01/20/2025
# * 邮箱: rootntsd@gmail.com
# ********************************************************************
#
# Unit tests: tests/unit/test_*.c (cmocka)
#   ya -r test_layer_json_dump
#
# Integration / full suite:
#   python scripts/run_tests.py

import os
import glob

def add_yui_unit_test(name):
    target(name)
    (
        add_deps("yui", "cjson", "cmocka"),
        add_rules("mode.debug", "mode.release"),
        set_kind("binary"),
        add_flags(),
        add_files("unit/" + name + ".c"),
        add_includedirs(".", "../src", "../lib/cmocka/include"),
        add_run()
    )

_unit_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "unit")
for _path in sorted(glob.glob(os.path.join(_unit_dir, "test_*.c"))):
    add_yui_unit_test(os.path.splitext(os.path.basename(_path))[0])
