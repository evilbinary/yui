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
    elif platform.system()=='Windows':
        mingw64='E:\\soft\\msys2\\mingw64'
        if get_plat() in['emscripten','em']:
            tool=get_toolchain_node()
            tool['cc']='emcc'
            tool['cxx']='emcc'
            tool['ld']='emcc'
            tool['ar']='emar'
            add_cflags(
            # '--use-port=sdl2 ',
            '-g',
            '-Wno-incompatible-pointer-types -Wno-implicit-function-declaration',
            '-F../libs/',
            '-I'+mingw64+'\\include\\SDL2',
            '-I.',
            '-I./src/components',
            '-I./src/'
            )
            add_ldflags(
                
                '-s USE_SDL=2',
                '-s USE_SDL_IMAGE=2',
                '-s USE_SDL_TTF=2',
                '-s ALLOW_MEMORY_GROWTH=1',
                '-s ASSERTIONS=1',
                #'-s EXPORTED_FUNCTIONS=["_main"]',
                #'-s EXPORTED_RUNTIME_METHODS=["ccall","cwrap","FS","stringToUTF8","UTF8ToString"]',
                '-s EXPORT_ALL=1 ',
                '-s ASSERTIONS=2 ',
                '-s STACK_OVERFLOW_CHECK=2',
                '-g4',
                '-O0',
                '-Wbad-function-cast ',
                '-Wcast-function-type',
                '--source-map-base http://localhost:6931/',

                '-s ERROR_ON_UNDEFINED_SYMBOLS=0',
                '-s AGGRESSIVE_VARIABLE_ELIMINATION=1',
                '--preload-file app/playground/',
                '--preload-file app/assets/',
                '--preload-file app/lib/',
                '--preload-file app',
                '-L'+mingw64+'\\lib',
                '-lSDL2main',
                '-lSDL2',
                '-lSDL2_ttf',
                '-lSDL2_image',
                '-lm'
                )
        else:
            tool=get_toolchain_node()
            tool['cc']=mingw64+'\\bin\\gcc.exe'
            tool['cxx']=mingw64+'\\bin\\g++.exe'
            tool['ld']=mingw64+'\\bin\\gcc.exe'
            tool['ar']=mingw64+'\\bin\\ar.exe'

            add_cflags(
                '-g',
                '-Wno-incompatible-pointer-types -Wno-implicit-function-declaration',
                '-F../libs/',
                '-I'+mingw64+'\\include\\SDL2',
                '-I.',
                '-I./src/components',
                '-I./src/'
                )
            add_ldflags(
                '-L'+mingw64+'\\lib',
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
