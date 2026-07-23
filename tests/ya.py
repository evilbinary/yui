# coding:utf-8
# *******************************************************************
# * Copyright 2025-present evilbinary
# * 作者: evilbinary on 01/20/2025
# * 邮箱: rootntsd@gmail.com
# ********************************************************************
#
# Unit tests under tests/test_*.c are auto-registered.
# Add a new file tests/test_foo.c, then: ya -r test_foo
# No need to edit this file for each new test.

import os
import glob

def add_yui_unit_test(name):
    """Link against the full yui library — do not hand-list src/*.c."""
    target(name)
    (
        add_deps("yui", "cjson"),
        add_rules("mode.debug", "mode.release"),
        set_kind("binary"),
        add_flags(),
        add_files(name + ".c"),
        add_run()
    )

_tests_dir = os.path.dirname(os.path.abspath(__file__))
for _path in sorted(glob.glob(os.path.join(_tests_dir, "test_*.c"))):
    add_yui_unit_test(os.path.splitext(os.path.basename(_path))[0])
