# coding:utf-8

target("yui-pc")
(
    add_deps("yui", "cjson", "yaml2json", "quickjs", "jsmodule-quickjs", "socket"),
    add_rules("mode.debug", "mode.release"),
    set_kind("binary"),
    add_flags(),
    add_cflags("-Iplatform/common", "-Isrc", "-Ilib/cjson", "-Ilib/yaml2json", "-Ilib/jsmodule", "-DHAS_JS_MODULE"),
    add_files("pc/main.c", "common/yui_boot.c"),
    add_run()
)

if get_plat() == "android":
    target("yui-android-prebuilt")
    (
        add_deps("yui", "cjson", "yaml", "yaml2json", "quickjs", "jsmodule-quickjs", "socket"),
        add_rules("mode.debug", "mode.release"),
    )

    target("yui-android-prebuilt-mqjs")
    (
        add_deps("yui", "cjson", "yaml", "yaml2json", "mquickjs", "jsmodule", "socket"),
        add_rules("mode.debug", "mode.release"),
    )

    target("yui-android-jni-quickjs")
    (
        add_deps("yui", "cjson", "yaml", "yaml2json", "quickjs", "jsmodule-quickjs", "socket"),
        add_rules("mode.debug", "mode.release"),
        set_kind("shared"),
        add_flags(),
        add_cflags("-Iplatform/common", "-Isrc", "-Ilib/cjson", "-Ilib/yaml2json", "-Ilib/jsmodule", "-DHAS_JS_MODULE"),
        add_files("android/jni/yui_bridge.cpp", "common/yui_boot.c"),
    )

    target("yui-android-jni-mqjs")
    (
        add_deps("yui", "cjson", "yaml", "yaml2json", "mquickjs", "jsmodule", "socket"),
        add_rules("mode.debug", "mode.release"),
        set_kind("shared"),
        add_flags(),
        add_cflags("-Iplatform/common", "-Isrc", "-Ilib/cjson", "-Ilib/yaml2json", "-Ilib/jsmodule", "-DHAS_JS_MODULE"),
        add_files("android/jni/yui_bridge.cpp", "common/yui_boot.c"),
    )

if get_plat() == "ios":
    target("yui-ios-prebuilt")
    (
        add_deps("yui", "cjson", "yaml", "yaml2json", "quickjs", "jsmodule-quickjs", "socket"),
        add_rules("mode.debug", "mode.release"),
    )

    target("yui-ios-prebuilt-mqjs")
    (
        add_deps("yui", "cjson", "yaml", "yaml2json", "mquickjs", "jsmodule", "socket"),
        add_rules("mode.debug", "mode.release"),
    )

target("yui-web.js")
(
    add_deps("socket", "yui", "quickjs", "jsmodule-quickjs", "yaml2json"),
    add_rules("mode.debug", "mode.release", "web.artifacts"),
    set_kind("binary"),
    add_flags(),
    add_cflags(
        "-Iplatform/common",
        "-Isrc",
        "-Ilib/cjson",
        "-Ilib/yaml2json",
        "-Ilib/jsmodule",
        "-DHAS_JS_MODULE",
    ),
    add_files("web/vanilla/main.c", "common/yui_boot.c"),
    add_ldflags(
        "-sMODULARIZE=1",
        "-sEXPORT_NAME=YuiModule",
        "-sEXPORTED_RUNTIME_METHODS=['callMain','FS']",
        "-sENVIRONMENT=web",
    ),
)
