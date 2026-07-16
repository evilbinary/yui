# coding:utf-8

target("yui-pc")
(
    add_deps("yui", "cjson", "yaml2json"),
    add_rules("mode.debug", "mode.release"),
    set_kind("binary"),
    add_flags(),
    add_cflags("-Iplatform/common", "-Isrc", "-Ilib/cjson", "-Ilib/yaml2json"),
    add_files("pc/main.c", "common/yui_boot.c"),
    add_run()
)
