# coding:utf-8

target("lvglmodule")
set_kind("static")
add_deps("lvgl", "lvgl_extra", "cjson")
add_includedirs("../src", "../src/components", "../cjson", "../lvgl", "../lvgl/src", public=True)
add_cflags("-DLV_CONF_INCLUDE_SIMPLE")
if get_plat() in ("esp32", "stm32"):
    # 嵌入式模式：ytype.h 使用 YuiTexture/YuiFont，不依赖 SDL
    add_cflags('-DYUI_BACKEND_EMBEDDED')
add_files("*.c")
