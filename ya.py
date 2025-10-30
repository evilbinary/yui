project("yui",
    version='0.0.1',
    desc='yui is an gui for ai mean agui, project page: https://github.com/evilbinary/yui',
    targets=[
        'yui',
        'test_blur_cache'
    ]
)



target("yui") 
def run(target):
    targetfile = target.targetfile()
    sourcefiles = target.sourcefiles()
    arch=target.get_arch()
    arch_type= target.get_arch_type()
    mode =target.get_config('mode')
    plat=target.plat()

    yui='export DYLD_FRAMEWORK_PATH=../libs && export DYLD_LIBRARY_PATH="../libs" '
    yui+="&& ./build/"+str(plat)+"/"+str(arch)+"/"+str(mode)+"/yui"

    print('run '+str(plat)+' yui',yui)
    os.shell(yui)
(
    add_rules("mode.debug", "mode.release"),
    set_kind("binary"),
    add_cflags(
        '-g',
        '-I/usr/local/Cellar/cjson/1.7.17/include/cjson',
        '-F../libs/',
        '-I../libs/SDL2.framework/Headers',
        '-I../libs/SDL2_ttf.framework/Headers',
        '-I../libs/SDL2_image.framework/Headers'
        ),
    add_ldflags(
        '-F../libs/',
       '-framework SDL2',
        '-framework SDL2_ttf',
        '-framework SDL2_image ',
        '-lcjson ',
        '-F../libs/'
        ),
    add_files("src/*.c"),
    add_files("src/components/*.c"),
    on_run(run)
)

target("test_blur_cache") 
def run_test(target):
    targetfile = target.targetfile()
    sourcefiles = target.sourcefiles()
    arch=target.get_arch()
    arch_type= target.get_arch_type()
    mode =target.get_config('mode')
    plat=target.plat()

    test_cmd='export DYLD_FRAMEWORK_PATH=../libs && export DYLD_LIBRARY_PATH="../libs" '
    test_cmd+="&& ./build/"+str(plat)+"/"+str(arch)+"/"+str(mode)+"/test_blur_cache"

    print('run '+str(plat)+' test_blur_cache',test_cmd)
    os.shell(test_cmd)
(
    add_rules("mode.debug", "mode.release"),
    set_kind("binary"),
    add_cflags(
        '-g',
        '-I/usr/local/Cellar/cjson/1.7.17/include/cjson',
        '-F../libs/',
        '-I../libs/SDL2.framework/Headers',
        '-I../libs/SDL2_ttf.framework/Headers',
        '-I../libs/SDL2_image.framework/Headers'
        ),
    add_ldflags(
        '-F../libs/',
       '-framework SDL2',
        '-framework SDL2_ttf',
        '-framework SDL2_image ',
        '-lcjson ',
        '-F../libs/'
        ),
    add_files("src/animate.c"),
    add_files("src/backend_sdl.c"),
    add_files("src/event.c"),
    add_files("src/layer.c"),
    add_files("src/layout.c"),
    add_files("src/render.c"),
    add_files("src/util.c"),
    add_files("src/components/*.c"),
    add_files("tests/test_blur_cache.c"),
    on_run(run_test)
)