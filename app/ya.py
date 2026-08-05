# coding:utf-8
# *******************************************************************
# * Copyright 2025-present evilbinary
# * 作者: evilbinary on 01/20/2025
# * 邮箱: rootntsd@gmail.com
# ********************************************************************

if is_host_plat():
    # 嵌入式平台跳过宿主 demo（main.c 依赖 SDL 头文件，由 ytype.h 引入）
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


if is_host_plat():
    # 嵌入式平台跳过依赖 jsmodule-quickjs 的 demo（jsmodule-quickjs 需 POSIX socket/lwip，由 ESP-IDF 工程编译）
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



if is_host_plat():
    # 嵌入式平台跳过依赖 mario/mquickjs/jsmodule-mqjs 的 demo（这些库在嵌入式平台已跳过）
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
        add_deps("jsmodule-mquickjs","yui", "mquickjs","yaml2json"),
        add_rules("mode.debug", "mode.release"),
        set_kind("binary"),
        add_flags(),
        add_files("playground/main.c"),
        add_run()
    )


    target("mqjs") 
    (
        add_deps( "jsmodule-mquickjs","yui", "mquickjs",),
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

if is_host_plat():
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


if is_host_plat():
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
        add_deps( "jsmodule-mquickjs","yui", "mquickjs",),
        add_rules("mode.debug", "mode.release"),
        set_kind("binary"),
        add_flags(),
        add_files("network/main.c"),
        add_run()
    )



if is_host_plat():
    # 嵌入式平台跳过依赖 jsmodule-quickjs 的 demo（jsmodule-quickjs 由 ESP-IDF 工程编译）
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

    target("watch-os") 
    (
        add_deps("socket","yui","quickjs","jsmodule-quickjs","yaml2json"),
        add_rules("mode.debug", "mode.release"),
        set_kind("binary"),
        add_flags(),
        add_files("watch-os/main.c"),
        add_run()
    )


    target("watch-os.html") 
    (
        add_deps("socket","yui","quickjs","jsmodule-quickjs","yaml2json"),
        add_rules("mode.debug", "mode.release"),
        set_kind("binary"),
        add_flags(),
        add_files("watch-os/main.c"),
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

    target("photo")
    (
        add_deps("socket","yui","quickjs","jsmodule-quickjs","yaml2json"),
        add_rules("mode.debug", "mode.release"),
        set_kind("binary"),
        add_flags(),
        add_files("photo/main.c"),
        add_run()
    )

if is_host_plat():
    # LVGL backend demo (YUI_PLAT=lvgl or -p lvgl) — 嵌入式平台跳过（lvglmodule 已跳过）
    target("lvgl-sdl")
    (
        add_deps("yui", "cjson", "lvglmodule", "lvgl", "lvgl_extra", "quickjs", "jsmodule-quickjs", "yaml2json"),
        add_rules("mode.debug", "mode.release"),
        set_kind("binary"),
        add_flags(),
        add_files("lvgl/main.c"),
        add_run()
    )


    # LVGL backend demo (YUI_PLAT=lvgl or -p lvgl)
    target("lvgl-stm32")
    (
        add_deps("yui", "cjson", "lvglmodule", "lvgl", "lvgl_extra"),
        add_rules("mode.debug", "mode.release"),
        set_kind("binary"),
        add_flags(),
        add_files("lvgl/main.c"),
        add_run()
    )