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

def add_run():
    on_run(run)

def get_prefix():
    return prefix_env

add_buildin('add_flags',add_flags)
add_buildin('add_run',add_run)
add_buildin('get_prefix',get_prefix)

prefix_env='export DYLD_FRAMEWORK_PATH=../libs && export DYLD_LIBRARY_PATH="../libs" && '

includes("./src/ya.py")
includes("./lib/ya.py")

includes("./app/ya.py")

includes("./tests/ya.py")
