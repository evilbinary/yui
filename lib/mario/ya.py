# coding:utf-8
# *******************************************************************
# * Copyright 2025-present evilbinary
# * 作者: evilbinary on 01/20/2025
# * 邮箱: rootntsd@gmail.com
# ********************************************************************

target("mario")
add_deps("socket")
set_kind("static")
add_files(
     'builtin/*.c',
    'builtin/*/*.c',
    'demo/compiler.c',
    'mario.c',
) 
add_includedirs(
    '.',
    '../include',
    public = true
)
add_cflags(' -I./ -Wall -DNO_MAIN -g -MMD -D_GNU_SOURCE -fno-math-errno -fno-trapping-math ')
