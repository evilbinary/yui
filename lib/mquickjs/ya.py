# coding:utf-8
# *******************************************************************
# * Copyright 2025-present evilbinary
# * 作者: evilbinary on 01/20/2025
# * 邮箱: rootntsd@gmail.com
# ********************************************************************

target("mquickjs")
set_kind("static")
add_flags()
if get_plat() == "esp32":
    # ESP32 (RISC-V/ilp32)：无 JS_PTR64，使用 32 位 stdlib ROM 表
    #（mqjs_stdlib_32.h），避免 RV32 GCC 无法编译 64 位自引用表
    add_files(
        'dtoa.c',
        'libm.c',
        'cutils.c',
        'readline.c',
        'mquickjs.c',
        'mqjs_std.c',
    )
else:
    add_files(
        'readline_tty.c',
        'readline.c',
        'mquickjs.c',
        'dtoa.c',
        'libm.c',
        'cutils.c',
        'mqjs_std.c'
    ) 
add_includedirs(
    '.',
    '../include',
    public = true
)
add_includedirs('./mquickjs/')
add_cflags(' -Wall -DNO_MAIN -g -MMD -D_GNU_SOURCE -fno-math-errno -fno-trapping-math ')
