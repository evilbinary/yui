# coding:utf-8
# *******************************************************************
# * Copyright 2025-present evilbinary
# * 作者: evilbinary on 01/20/2025
# * 邮箱: rootntsd@gmail.com
# ********************************************************************

target('yui')
add_deps("cjson")
add_deps("tsm")
add_includedirs('.', 'components', '../lib/libtsm/src', public=True)
add_flags()
set_kind('static')
add_files("*.c")
add_files("components/*.c")
add_files("perf/*.c")
add_files("input/*.c")
add_files("backend/backend_common.c")

if get_plat() == "esp32":
    # ESP32 资源有限，禁用 game/audio（miniaudio 依赖 POSIX pthread/dlfcn）
    add_cflags("-DYUI_WITH_GAME=0")
    add_cflags("-DYUI_WITH_GAME_AUDIO=0")
else:
    add_files("game/*.c")
    add_cflags("-DYUI_WITH_GAME=1")
    add_cflags("-DYUI_WITH_GAME_AUDIO=1")
    add_includedirs('../lib/miniaudio')

if get_plat() in ("lvgl", "em-lvgl"):
    add_files("backend/backend_lvgl.c")
    add_cflags("-DYUI_USE_LVGL_BACKEND")
    add_cflags("-DYUI_HAS_LVGLMODULE")
    add_cflags("-DYUI_LVGL_PORT_SDL")
    add_deps("lvgl", "lvgl_extra", "lvglmodule")
elif get_plat() == "stm32":
    add_files("backend/backend_stm32.c")
    add_files("backend/backend_embed_font.c")
    add_includedirs('../lib/stb')
    add_cflags("-DSTM32_PLATFORM")
    add_cflags("-DYUI_BACKEND_EMBEDDED")
elif get_plat() == "esp32":
    # ya 只编译 yui 核心 + 通用字体（不依赖 ESP-IDF）
    # backend_esp32.c 依赖 esp_lcd/esp_lcd_touch 等 ESP-IDF 组件，
    # 其 include 路径由 idf.py 生成，故由 ESP-IDF 工程编译
    add_files("backend/backend_embed_font.c")
    add_includedirs('../lib/stb')
    add_cflags("-DYUI_BACKEND_EMBEDDED")
elif get_plat() in ("android", "ios"):
    add_files("backend/backend_mobile.c")
    add_files("backend/mobile_text.c")
    add_includedirs('../lib/stb')
    add_cflags("-DYUI_BACKEND_MOBILE")
else:
    add_files("backend/backend_sdl.c")
    add_cflags("-DYUI_USE_SDL_BACKEND")
