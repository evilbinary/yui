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
