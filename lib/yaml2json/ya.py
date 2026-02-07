# coding:utf-8
# *******************************************************************
# * Copyright 2025-present evilbinary
# * 作者: evilbinary on 01/20/2025
# * 邮箱: rootntsd@gmail.com
# ********************************************************************
target("yaml2json")
set_kind("static")
add_deps("yaml","cjson")
add_files(
    '*.c',
) 
add_includedirs('.')
add_cflags(' -I.')

