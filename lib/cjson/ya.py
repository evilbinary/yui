# coding:utf-8
# *******************************************************************
# * Copyright 2025-present evilbinary
# * 作者: evilbinary on 01/20/2025
# * 邮箱: rootntsd@gmail.com
# ********************************************************************
target("cjson")
set_kind("static")
add_files(
    'cJSON_Utils.c',
    'cJSON.h.c',
    'cJSON.c',
    # 'cJSON.h'
)
add_includedirs('.')
add_cflags(' -pedantic -Wall -Werror -Wstrict-prototypes -Wwrite-strings -Wshadow -Winit-self -Wcast-align -Wformat=2 -Wmissing-prototypes -Wstrict-overflow=2 -Wcast-qual -Wc++-compat -Wundef -Wswitch-default -Wconversion  ')

