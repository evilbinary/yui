# coding:utf-8

target("lvgl")
set_kind("static")

add_files(
    'src/core/*.c',
    'src/draw/*.c',
    'src/draw/sw/*.c',
    'src/font/*.c',
    'src/hal/*.c',
    'src/misc/*.c',
    'src/widgets/*.c',
    'src/extra/*.c',
    'src/extra/**/*.c',
)

add_cflags('-DLV_CONF_INCLUDE_SIMPLE')

add_includedirs('.', './src', public=True)

if get_plat() == "stm32":
    add_files('port_yiyiya/*.c')
    add_cflags('-DYUI_LVGL_PORT_STM32')
else:
    add_files('port_sdl/*.c')
    add_cflags('-DYUI_LVGL_PORT_SDL')
