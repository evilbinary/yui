# coding:utf-8
# *******************************************************************
# * Copyright 2025-present evilbinary
# * 作者: evilbinary on 01/20/2025
# * 邮箱: rootntsd@gmail.com
# ********************************************************************

target('yui')
add_deps("cjson")
add_flags()
set_kind('static')
add_files("*.c")
add_files("components/*.c")
add_files("backend/backend_common.c")

if get_plat() == "lvgl":
    add_files("backend/backend_lvgl.c")
    add_cflags("-DYUI_BACKEND_LVGL")
    add_cflags("-DYUI_HAS_LVGLMODULE")
    add_deps("lvgl", "lvglmodule")
elif get_plat() == "stm32":
    add_files("backend/backend_stm32.c")
    add_cflags("-DSTM32_PLATFORM")
else:
    add_files("backend/backend_sdl.c")
    add_cflags("-DYUI_BACKEND_SDL")
