import platform

project("yui",
    version='0.0.1',
    desc='yui is an gui for ai mean agui, project page: https://github.com/evilbinary/yui',
    targets=[
        'yui',
        'test_blur_cache',
        'test_content_size',
        'test_text_render',
        'test_text_simple'
    ]
)




def add_flags():

    checkmem=True
    if platform.system()=='Windows':
        checkmem=False
        
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
        tool['cc']='D:\\app\\msys2\\mingw64\\bin\\gcc.exe'
        tool['cxx']='D:\\app\\msys2\\mingw64\\bin\\g++.exe'
        tool['ld']='D:\\app\\msys2\\mingw64\\bin\\gcc.exe'
        tool['ar']='D:\\app\\msys2\\mingw64\\bin\\ar.exe'

        add_cflags(
            '-g',
            '-F../libs/',
            '-ID:\\app\\msys2\\mingw64\\include\\SDL2',
            '-I.',
            '-I./src/components',
            '-I./src/'
            )
        add_ldflags(
            '-LD:/app/msys2/mingw64/lib',
            '-lSDL2main',
            '-lSDL2',
            '-lSDL2_ttf',
            '-lSDL2_image',
            '-lm',
            '-lws2_32'
            )

def run(target):
    targetfile = target.targetfile()
    sourcefiles = target.sourcefiles()
    arch=target.get_arch()
    arch_type= target.get_arch_type()
    mode =target.get_config('mode')
    plat=target.plat()
    if platform.system()=='Windows':
        yui=targetfile.replace('/','\\')
    else:
        yui=prefix_env+" && ./"+targetfile

    print('run '+yui,yui)
    os.shell(yui)

def add_run():
    on_run(run)

def get_prefix():
    return prefix_env

add_buildin('add_flags',add_flags)
add_buildin('add_run',add_run)
add_buildin('get_prefix',get_prefix)

prefix_env=''

if platform.system()=='Windows':
    prefix_env=''
elif platform.system()=='Darwin':
    prefix_env='export DYLD_FRAMEWORK_PATH=../libs && export DYLD_LIBRARY_PATH="../libs" && '
elif platform.system()=='Linux':
    prefix_env='export LD_LIBRARY_PATH="../libs" && '

includes("./src/ya.py")
includes("./lib/ya.py")

includes("./app/ya.py")

includes("./tests/ya.py")
