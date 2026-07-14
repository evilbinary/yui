# coding:utf-8

target("lvglmodule")
set_kind("static")
add_deps("lvgl", "lvgl_extra", "cjson")
add_includedirs("../src", "../src/components", "../cjson", "../lvgl", "../lvgl/src", public=True)
add_cflags("-DLV_CONF_INCLUDE_SIMPLE")
add_files("*.c")
