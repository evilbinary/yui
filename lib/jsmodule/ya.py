# coding:utf-8
# *******************************************************************
# * Copyright 2025-present evilbinary
# * 作者: evilbinary on 01/20/2025
# * 邮箱: rootntsd@gmail.com
# ********************************************************************

target("jsmodule")
add_deps("mquickjs","cjson")

set_kind("static")
add_files(
    'js_module.c',
) 
add_cflags(' -I. -I../mquickjs -g -DHAS_JS_MODULE')
add_includedirs('.', public=true)