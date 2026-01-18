# coding:utf-8
# *******************************************************************
# * Copyright 2025-present evilbinary
# * 作者: evilbinary on 01/20/2025
# * 邮箱: rootntsd@gmail.com
# ********************************************************************

target("test_blur_cache") 
(
    add_deps("cjson"),
    add_rules("mode.debug", "mode.release"),
    set_kind("binary"),
    add_flags(),
    add_files("../src/animate.c"),
    add_files("../src/backend_sdl.c"),
    add_files("../src/event.c"),
    add_files("../src/layer.c"),
    add_files("../src/layer_update.c"),
    add_files("../src/layer_properties.c"),
    add_files("../src/layout.c"),
    add_files("../src/render.c"),
    add_files("../src/popup_manager.c"),
    add_files("../src/util.c"),
    add_files("../src/theme.c"),
    add_files("../src/theme_manager.c"),
    add_files("../src/components/*.c"),
    add_files("test_blur_cache.c"),
    add_run()
)

target("test_content_size") 
(
    add_deps("cjson"),
    add_rules("mode.debug", "mode.release"),
    add_flags(),
    add_files("../src/layout.c"),
    add_files("../src/backend_sdl.c"),
    add_files("../src/animate.c"),
    add_files("../src/event.c"),
    add_files("../src/layer.c"),
    add_files("../src/layer_update.c"),
    add_files("../src/layer_properties.c"),
    add_files("../src/render.c"),
    add_files("../src/popup_manager.c"),
    add_files("../src/components/*.c"),
    add_files("../src/util.c"),
    add_files("../src/theme.c"),
    add_files("../src/theme_manager.c"),
    add_files("test_content_size.c"),
    add_run()
)

target("test_treeview_scroll") 
(
    add_deps("cjson"),
    add_rules("mode.debug", "mode.release"),
    set_kind("binary"),
    add_flags(),
    add_files("../src/animate.c"),
    add_files("../src/backend_sdl.c"),
    add_files("../src/event.c"),
    add_files("../src/layer.c"),
    add_files("../src/layer_update.c"),
    add_files("../src/layer_properties.c"),
    add_files("../src/layout.c"),
    add_files("../src/render.c"),
    add_files("../src/util.c"),
    add_files("../src/popup_manager.c"),
    add_files("../src/components/*.c"),
    add_files("../src/theme.c"),
    add_files("../src/theme_manager.c"),
    add_files("test_treeview_scroll.c"),
    add_run()
)


target("test_simple_scroll") 
(
    add_deps("cjson"),
    add_rules("mode.debug", "mode.release"),
    set_kind("binary"),
    add_flags(),
    add_files("../src/animate.c"),
    add_files("../src/backend_sdl.c"),
    add_files("../src/event.c"),
    add_files("../src/layer.c"),
    add_files("../src/layer_update.c"),
    add_files("../src/layer_properties.c"),
    add_files("../src/layout.c"),
    add_files("../src/render.c"),
    add_files("../src/util.c"),
    add_files("../src/popup_manager.c"),
    add_files("../src/components/*.c"),
    add_files("../src/theme.c"),
    add_files("../src/theme_manager.c"),
    add_files("test_simple_scroll.c"),
    add_run()
)

target("test_text_render") 
(
    add_deps("cjson"),
    add_rules("mode.debug", "mode.release"),
    set_kind("binary"),
    add_flags(),
    add_files("../src/animate.c"),
    add_files("../src/backend_sdl.c"),
    add_files("../src/event.c"),
    add_files("../src/layer.c"),
    add_files("../src/layer_update.c"),
    add_files("../src/layer_properties.c"),
    add_files("../src/layout.c"),
    add_files("../src/render.c"),
    add_files("../src/util.c"),
    add_files("../src/popup_manager.c"),
    add_files("../src/components/*.c"),
    add_files("../src/theme.c"),
    add_files("../src/theme_manager.c"),
    add_files("test_text_render.c"),
    add_run()
)

target("test_text_simple") 
(
    add_deps("cjson"),
    add_rules("mode.debug", "mode.release"),
    set_kind("binary"),
    add_flags(),
    add_files("../src/animate.c"),
    add_files("../src/backend_sdl.c"),
    add_files("../src/event.c"),
    add_files("../src/layer.c"),
    add_files("../src/layer_update.c"),
    add_files("../src/layer_properties.c"),
    add_files("../src/layout.c"),
    add_files("../src/render.c"),
    add_files("../src/util.c"),
    add_files("../src/popup_manager.c"),
    add_files("../src/components/*.c"),
    add_files("../src/theme.c"),
    add_files("../src/theme_manager.c"),
    add_files("test_text_simple.c"),
    add_run()
)