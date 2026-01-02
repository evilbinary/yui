# coding:utf-8
# *******************************************************************
# * Copyright 2025-present evilbinary
# * 作者: evilbinary on 01/20/2025
# * 邮箱: rootntsd@gmail.com
# ********************************************************************

target("jsmodule-mario")
add_deps("mario","cjson","yui")
add_cflags(' -DBUILD_NO_MAIN=1 -DHAS_JS_MODULE -DCONFIG_CLASS_YUI  -I. -I../mquickjs -g ')
add_flags()

set_kind("static")
add_files(
    'js_module.c',
    '../jsmodule/js_module.c'
) 
add_includedirs('.', public=true)