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
    if get_plat() in ("esp32", "stm32"):
        return  # 单元测试为宿主 cmocka 二进制，嵌入式平台跳过
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

def add_js_unit_test(name, engine):
    """JS 引擎单元测试：engine 为 "mqjs" 或 "qjs"，决定链接的适配器与头文件。"""
    if get_plat() in ("esp32", "stm32"):
        return  # JS 引擎测试为宿主二进制，嵌入式平台跳过
    if engine == "mqjs":
        _deps = ("yui", "cjson", "cmocka", "jsmodule-mquickjs")
        _incs = (
            ".",
            "../src",
            "../lib/cmocka/include",
            "../lib/jsmodule",
            "../lib/jsmodule-mquickjs",
            "../lib/mquickjs",
        )
    else:  # qjs
        _deps = ("yui", "cjson", "cmocka", "jsmodule-quickjs")
        _incs = (
            ".",
            "../src",
            "../lib/cmocka/include",
            "../lib/jsmodule",
            "../lib/jsmodule-quickjs",
        )
    target(name)
    (
        add_deps(*_deps),
        add_rules("mode.debug", "mode.release"),
        set_kind("binary"),
        add_flags(),
        add_files("unit/" + name + ".c"),
        add_includedirs(*_incs),
        add_run()
    )

_unit_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "unit")
for _path in sorted(glob.glob(os.path.join(_unit_dir, "test_js_*.c"))):
    _name = os.path.splitext(os.path.basename(_path))[0]
    if _name.endswith("_qjs"):
        add_js_unit_test(_name, "qjs")
    elif _name.endswith("_mqjs"):
        add_js_unit_test(_name, "mqjs")
    else:
        # 无后缀的 JS 测试默认使用 mquickjs（保持向后兼容）
        add_js_unit_test(_name, "mqjs")
for _path in sorted(glob.glob(os.path.join(_unit_dir, "test_*.c"))):
    _name = os.path.splitext(os.path.basename(_path))[0]
    if _name.startswith("test_js_"):
        continue
    add_yui_unit_test(_name)
