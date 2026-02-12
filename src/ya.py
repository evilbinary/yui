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

if is_plat("stm32"):
    add_files("backend/backend_stm32.c")
    # 添加STM32特定的编译标志
    add_cflags("-DSTM32_PLATFORM")
    # 添加STM32特定的依赖
    # add_deps("stm32_hal")  # 如果需要的话
else:
    # 默认使用SDL后端
    add_files("backend/backend_sdl.c")
