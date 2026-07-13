# coding:utf-8

target("lvglmodule")
set_kind("static")
add_deps("lvgl")
add_includedirs("../src", "../src/components", public=True)
add_files("*.c")
