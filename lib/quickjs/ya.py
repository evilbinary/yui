# coding:utf-8
# *******************************************************************
# * Copyright 2025-present evilbinary
# * 作者: evilbinary on 01/20/2025
# * 邮箱: rootntsd@gmail.com
# ********************************************************************
target("quickjs")
set_kind("static")

add_files(
    'quickjs.c',
    'libregexp.c',
    'libunicode.c',
    'cutils.c',
    'quickjs-libc.c',
    'libbf.c'
) 
add_includedirs('.')
add_cflags(' -UCONFIG_PRINTF_RNDN -D_GNU_SOURCE -DUSE_FILE32API  -I. -DCONFIG_BIGNUM')
