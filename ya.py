import platform

project("yui",
    version='0.0.1',
    desc='yui is an gui for ai mean agui, project page: https://github.com/evilbinary/yui',
    targets=[
        'yui',
        'test_blur_cache',
        'test_content_size'
    ]
)

includes("./lib/ya.py")


def add_flags():

    checkmem=True
    if checkmem:
        tool=get_toolchain_node()
        tool['ld']='gcc'
        add_cflags(
            '-fsanitize=address',
        )
        add_ldflags(
            '-fsanitize=address',
        )

    if platform.system()=='Darwin':
        add_cflags(
            '-g',
            '-F../libs/',
            '-I../libs/SDL2.framework/Headers',
            '-I../libs/SDL2_ttf.framework/Headers',
            '-I../libs/SDL2_image.framework/Headers',
            '-Isrc',
            '-Ilib',
            '-Ilib/mquickjs',
            '-DHAS_JS_MODULE'
            ),
        add_ldflags(
            '-F../libs/',
        '-framework SDL2',
            '-framework SDL2_ttf',
            '-framework SDL2_image ',
            '-F../libs/'
            ),
    elif platform.system()=='Linux':
        add_cflags(
            '-g',
            '-F../libs/',
            '-I../libs/SDL2.framework/Headers',
            '-I../libs/SDL2_ttf.framework/Headers',
            '-I../libs/SDL2_image.framework/Headers',
            '-Isrc'
            )
        add_ldflags(
            '-F../libs/',
            '-framework SDL2',
            '-framework SDL2_ttf',
            '-framework SDL2_image ',
            '-F../libs/'
            )
    else:
        tool=get_toolchain_node()
        tool['ld']='gcc'

        add_cflags(
            '-g',
            '-F../libs/',
            '-ID:\\app\\msys2\\mingw64\\include\\SDL2',
            '-I.',
            '-I./src/components',
            '-I./src/'
            )
        add_ldflags(
            '-LD:\\app\\msys2\\mingw64\\lib',
            '-L../libs/',
            '-lSDL2',
            '-lSDL2_ttf',
            '-lSDL2_image',
            '-lm',
            )

prefix_env='export DYLD_FRAMEWORK_PATH=../libs && export DYLD_LIBRARY_PATH="../libs" && '

target('yui')
add_deps("cjson")
add_flags()
set_kind('static')
add_files("src/*.c")
add_files("src/components/*.c")


def run(target):
    targetfile = target.targetfile()
    sourcefiles = target.sourcefiles()
    arch=target.get_arch()
    arch_type= target.get_arch_type()
    mode =target.get_config('mode')
    plat=target.plat()

    yui=prefix_env+"./"+targetfile

    print('run '+yui,yui)
    os.shell(yui)

target("main") 
(
    add_deps("yui","cjson"),
    add_rules("mode.debug", "mode.release"),
    set_kind("binary"),
    add_flags(),
    add_files("app/main.c"),
    on_run(run)
)


target("playground") 
(
    add_deps("yui"),
    add_rules("mode.debug", "mode.release"),
    set_kind("binary"),
    add_flags(),
    add_files("app/playground/main.c"),
    on_run(run)
)


target("mqjs") 
(
    add_deps("yui", "mquickjs", "jsmodule"),
    add_rules("mode.debug", "mode.release"),
    set_kind("binary"),
    add_flags(),
    add_files("app/mquickjs/main.c"),
    on_run(run)
)


target("test_blur_cache") 
(
    add_deps("cjson"),
    add_rules("mode.debug", "mode.release"),
    set_kind("binary"),
    add_flags(),
    add_files("src/animate.c"),
    add_files("src/backend_sdl.c"),
    add_files("src/event.c"),
    add_files("src/layer.c"),
    add_files("src/layout.c"),
    add_files("src/render.c"),
    add_files("src/popup_manager.c"),
    add_files("src/util.c"),
    add_files("src/components/*.c"),
    add_files("tests/test_blur_cache.c"),
    on_run(run)
)

target("test_content_size") 
(
    add_deps("cjson"),
    add_rules("mode.debug", "mode.release"),
    add_flags(),
    add_files("src/layout.c"),
    add_files("src/backend_sdl.c"),
    add_files("src/animate.c"),
    add_files("src/event.c"),
    add_files("src/layer.c"),
    add_files("src/render.c"),
    add_files("src/popup_manager.c"),
    add_files("src/components/*.c"),
    add_files("src/util.c"),
    add_files("tests/test_content_size.c"),
    on_run(run)
)

target("test_treeview_scroll") 
(
    add_deps("cjson"),
    add_rules("mode.debug", "mode.release"),
    set_kind("binary"),
    add_flags(),
    add_files("src/animate.c"),
    add_files("src/backend_sdl.c"),
    add_files("src/event.c"),
    add_files("src/layer.c"),
    add_files("src/layout.c"),
    add_files("src/render.c"),
    add_files("src/util.c"),
    add_files("src/popup_manager.c"),
    add_files("src/components/*.c"),
    add_files("tests/test_treeview_scroll.c"),
    on_run(run)
)


target("test_simple_scroll") 
(
    add_deps("cjson"),
    add_rules("mode.debug", "mode.release"),
    set_kind("binary"),
    add_flags(),
    add_files("src/animate.c"),
    add_files("src/backend_sdl.c"),
    add_files("src/event.c"),
    add_files("src/layer.c"),
    add_files("src/layout.c"),
    add_files("src/render.c"),
    add_files("src/util.c"),
    add_files("src/popup_manager.c"),
    add_files("src/components/*.c"),
    add_files("tests/test_simple_scroll.c"),
    on_run(run)
)