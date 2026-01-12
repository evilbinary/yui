# coding:utf-8
# *******************************************************************
# * Copyright 2025-present evilbinary
# * 作者: evilbinary on 01/20/2025
# * 邮箱: rootntsd@gmail.com
# ********************************************************************

target("jsmodule-quickjs")
add_deps("quickjs","cjson","yui")
add_cflags(' -DBUILD_NO_MAIN=1  -I. -I../../lib/quickjs -g ')
add_flags()

set_kind("static")
add_files(
    'js_module.c',
    'js_socket.c',
    '../jsmodule/js_common.c'
) 
add_includedirs('.', public=true)