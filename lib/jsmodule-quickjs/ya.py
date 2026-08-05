# coding:utf-8
# *******************************************************************
# * Copyright 2025-present evilbinary
# * 作者: evilbinary on 01/20/2025
# * 邮箱: rootntsd@gmail.com
# ********************************************************************

target("jsmodule-quickjs")
add_deps("quickjs","cjson","yui","socket")
add_cflags('-DBUILD_NO_MAIN=1', '-I.', '-I../../lib/quickjs', '-g')
if get_plat() in ("esp32", "stm32"):
    # 嵌入式模式：ytype.h 使用 YuiTexture/YuiFont，不依赖 SDL
    add_cflags('-DYUI_BACKEND_EMBEDDED')
    # YUI_MAX_PATH / YUI_PATH_MAX / MAX_JS_EVENTS / MAX_C_EVENT_HANDLERS 已统一
    # 收敛到根 ya.py 的 add_flags()，此处不再重复。
add_flags()

set_kind("static")
add_files(
    'js_module.c',
    'js_socket.c',
    'js_timer.c',
    'js_perf.c',
    'js_game.c',
    '../jsmodule/js_common.c'
) 
add_includedirs('.', '../..', '../jsmodule', public=true)