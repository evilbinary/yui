# coding:utf-8

import os

_LVGL_DIR = os.path.dirname(os.path.abspath(__file__))


def _extra_widget_sources():
    """List extra widget .c files (ya glob does not match nested paths on Windows)."""
    root = os.path.join(_LVGL_DIR, 'src', 'extra', 'widgets')
    sources = []
    if not os.path.isdir(root):
        return sources
    for sub in sorted(os.listdir(root)):
        subdir = os.path.join(root, sub)
        if not os.path.isdir(subdir):
            continue
        for name in sorted(os.listdir(subdir)):
            if name.endswith('.c'):
                sources.append('src/extra/widgets/%s/%s' % (sub, name))
    return sources


# layouts/themes + lv_extra.c — required when LV_USE_FLEX/GRID/THEME_DEFAULT are enabled
_EXTRA_CORE = [
    'src/extra/lv_extra.c',
    'src/extra/layouts/flex/lv_flex.c',
    'src/extra/layouts/grid/lv_grid.c',
    'src/extra/themes/default/lv_theme_default.c',
    'src/extra/themes/basic/lv_theme_basic.c',
    'src/extra/themes/mono/lv_theme_mono.c',
]

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
)
add_files(*_EXTRA_CORE)

add_cflags('-DLV_CONF_INCLUDE_SIMPLE')

add_includedirs('.', './src', public=True)

if get_plat() == "yiyiya":
    add_files('port_yiyiya/*.c')
    add_cflags('-DYUI_LVGL_PORT_YIYIYA', public=True)
else:
    add_files('port_sdl/*.c')
    add_cflags('-DYUI_LVGL_PORT_SDL', public=True)

target("lvgl_extra")
set_kind("static")
add_deps("lvgl")
add_files(*_extra_widget_sources())
add_cflags('-DLV_CONF_INCLUDE_SIMPLE')
add_includedirs('.', './src', public=True)
