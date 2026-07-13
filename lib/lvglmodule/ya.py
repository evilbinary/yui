# coding:utf-8

target("lvglmodule")
set_kind("static")
add_deps("lvgl", "cjson")
add_includedirs("../src", "../src/components", "../cjson", public=True)
add_files("*.c")
