# coding:utf-8
# *******************************************************************
# * Copyright 2025-present evilbinary
# * 作者: evilbinary on 01/20/2025
# * 邮箱: rootntsd@gmail.com
# ********************************************************************

target("jsmodule-mario")
add_deps("mario","cjson","yui")
add_cflags(' -DBUILD_NO_MAIN=1  -I. -I../../lib/mario -g ')
if get_plat() in ("esp32", "stm32"):
    # 嵌入式模式：ytype.h 使用 YuiTexture/YuiFont，不依赖 SDL
    add_cflags('-DYUI_BACKEND_EMBEDDED')
    # YUI_MAX_PATH / YUI_PATH_MAX / MAX_JS_EVENTS / MAX_C_EVENT_HANDLERS 已统一
    # 收敛到根 ya.py 的 add_flags()，此处不再重复。
add_flags()

set_kind("static")
add_files(
    'js_module.c',
    '../jsmodule/js_common.c'
) 
add_includedirs('.', public=true)