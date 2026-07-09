# coding:utf-8
# *******************************************************************
# * Copyright 2025-present evilbinary
# * 作者: evilbinary on 01/20/2025
# * 邮箱: rootntsd@gmail.com
# ********************************************************************

target("main") 
(
    add_deps("yui","cjson","yaml2json"),
    add_rules("mode.debug", "mode.release"),
    set_kind("binary"),
    add_flags(),
    add_files("main.c"),
    add_run()
)


target("main.html") 
(
    add_deps("yui","cjson","yaml2json"),
    add_rules("mode.debug", "mode.release"),
    set_kind("binary"),
    add_flags(),
    add_files("main.c"),
    add_run()
)


target("playground") 
(
    add_deps("socket","yui","quickjs","jsmodule-quickjs","yaml2json"),
    add_rules("mode.debug", "mode.release"),
    set_kind("binary"),
    add_flags(),
    add_files("playground/main.c"),
    add_run()
)

target("playground.html") 
(
    add_deps("socket","yui","quickjs","jsmodule-quickjs","yaml2json"),
    add_rules("mode.debug", "mode.release"),
    set_kind("binary"),
    add_flags(),
    add_files("playground/main.c"),
    add_run()
)



target("playground-mario") 
(
    add_deps("jsmodule-mario","yui", "mario","yaml2json"),
    add_rules("mode.debug", "mode.release"),
    set_kind("binary"),
    add_flags(),
    add_files("playground/main.c"),
    add_run()
)

target("playground-mqjs") 
(
    add_deps("jsmodule","yui", "mquickjs","yaml2json"),
    add_rules("mode.debug", "mode.release"),
    set_kind("binary"),
    add_flags(),
    add_files("playground/main.c"),
    add_run()
)


target("mqjs") 
(
    add_deps( "jsmodule","yui", "mquickjs",),
    add_rules("mode.debug", "mode.release"),
    set_kind("binary"),
    add_flags(),
    add_files("js/main.c"),
    add_run()
)

target("mariojs")
(
    add_deps( "jsmodule-mario","yui", "mario",),
    add_rules("mode.debug", "mode.release"),
    set_kind("binary"),
    add_flags(),
    add_files("js/main.c"),
    add_run()
)

target("qjs")
(
    add_deps( "jsmodule-quickjs","yui", "quickjs",),
    add_rules("mode.debug", "mode.release"),
    set_kind("binary"),
    add_flags(),
    add_files("js/main.c"),
    add_run()
)


target("network") 
(
    add_deps("yui","quickjs","jsmodule-quickjs"),
    add_rules("mode.debug", "mode.release"),
    set_kind("binary"),
    add_flags(),
    add_files("network/main.c"),
    add_run()
)


target("network-mario") 
(
    add_deps( "jsmodule-mario","yui", "mario",),
    add_rules("mode.debug", "mode.release"),
    set_kind("binary"),
    add_flags(),
    add_files("network/main.c"),
    add_run()
)

target("network-mqjs") 
(
    add_deps( "jsmodule","yui", "mquickjs",),
    add_rules("mode.debug", "mode.release"),
    set_kind("binary"),
    add_flags(),
    add_files("network/main.c"),
    add_run()
)



target("camera") 
(
    add_deps("socket","yui","quickjs","jsmodule-quickjs","yaml2json"),
    add_rules("mode.debug", "mode.release"),
    set_kind("binary"),
    add_flags(),
    add_files("camera/main.c"),
    add_run()
)

target("camera.html") 
(
    add_deps("socket","yui","quickjs","jsmodule-quickjs","yaml2json"),
    add_rules("mode.debug", "mode.release"),
    set_kind("binary"),
    add_flags(),
    add_files("camera/main.c"),
    add_run()
)


target("reader") 
(
    add_deps("socket","yui","quickjs","jsmodule-quickjs","yaml2json"),
    add_rules("mode.debug", "mode.release"),
    set_kind("binary"),
    add_flags(),
    add_files("reader/main.c"),
    add_run()
)

target("reader.html") 
(
    add_deps("socket","yui","quickjs","jsmodule-quickjs","yaml2json"),
    add_rules("mode.debug", "mode.release"),
    set_kind("binary"),
    add_flags(),
    add_files("reader/main.c"),
    add_run()
)

target("calc") 
(
    add_deps("socket","yui","quickjs","jsmodule-quickjs","yaml2json"),
    add_rules("mode.debug", "mode.release"),
    set_kind("binary"),
    add_flags(),
    add_files("calc/main.c"),
    add_run()
)


target("calc.html") 
(
    add_deps("socket","yui","quickjs","jsmodule-quickjs","yaml2json"),
    add_rules("mode.debug", "mode.release"),
    set_kind("binary"),
    add_flags(),
    add_files("calc/main.c"),
    add_run()
)


target("db")
(
    add_deps("socket","yui","quickjs","jsmodule-quickjs","yaml2json"),
    add_rules("mode.debug", "mode.release"),
    set_kind("binary"),
    add_flags(),
    add_files("db/main.c", "db/mysql_fun.c"),
    add_cflags(' -I/mingw64/include/mariadb -I/mingw64/include/'),
    add_ldflags(' -L/mingw64/lib/ -lmariadb '),
    add_run()
)