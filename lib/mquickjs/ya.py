# coding:utf-8
# *******************************************************************
# * Copyright 2025-present evilbinary
# * 作者: evilbinary on 01/20/2025
# * 邮箱: rootntsd@gmail.com
# ********************************************************************

target("mquickjs")
set_kind("static")
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
