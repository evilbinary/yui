# coding:utf-8
# *******************************************************************
# * Copyright 2025-present evilbinary
# * 作者: evilbinary on 01/20/2025
# * 邮箱: rootntsd@gmail.com
# ********************************************************************

target("main") 
(
    add_deps("yui","cjson"),
    add_rules("mode.debug", "mode.release"),
    set_kind("binary"),
    add_flags(),
    add_files("app/main.c"),
    add_run()
)


target("playground") 
(
    add_deps("yui"),
    add_rules("mode.debug", "mode.release"),
    set_kind("binary"),
    add_flags(),
    add_files("app/playground/main.c"),
    add_run()
)


target("mqjs") 
(
    add_deps( "jsmodule","yui", "mquickjs",),
    add_rules("mode.debug", "mode.release"),
    set_kind("binary"),
    add_flags(),
    add_files("app/mquickjs/main.c"),
    add_run()
)