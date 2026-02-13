import platform
import os

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

# 判断是否为指定平台
def is_plat(plat_name):
    plat = get_plat()
    return plat == plat_name

prefix_env=''

if platform.system()=='Windows':
    prefix_env=''
elif platform.system()=='Darwin': 
    os.environ['DYLD_FRAMEWORK_PATH'] = '../libs'
    os.environ['DYLD_LIBRARY_PATH'] = '../libs'
elif platform.system()=='Linux':
    # 设置环境变量，不通过shell方式
    os.environ['LD_LIBRARY_PATH'] = '../libs'
elif is_plat("stm32"):
    # STM32平台不需要设置环境变量
    prefix_env=''

def add_flags():
    checkmem=True
    if platform.system()=='Windows':
        checkmem=False
        
    if checkmem and not is_plat("stm32"):
        tool=get_toolchain_node()
        tool['ld']='gcc'
        add_cflags(
            '-fsanitize=address',
        )
        add_ldflags(
            '-fsanitize=address',
        )

    if get_plat() in['emscripten','em']:
        set_toolchain('emscripten')
        tool=get_toolchain_node()
        tool['cc']='emcc'
        tool['cxx']='emcc'
        tool['ld']='emcc'
        tool['ar']='emar'
        if platform.system()=='Windows':
            mingw64='E:\\soft\\msys2\\mingw64'
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
            mingw64='../libs/'

            add_cflags(
            # '--use-port=sdl2 ',
            '-g',
            '-Wno-incompatible-pointer-types -Wno-implicit-function-declaration',
            '-F../libs/',
            '-I'+mingw64+'\\include\\SDL2',
            '-I.',
            '-I./src/components',
            '-I./src/',
            '-s USE_SDL=2',
            '-s USE_SDL_IMAGE=2',
            '-s USE_SDL_TTF=2',
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
               
                '-lSDL2',
                '-lSDL2_ttf',
                '-lSDL2_image',
                '-lm'
                )
    elif is_plat("stm32"):
        # STM32平台特定的编译选项
        add_cflags(
            '-g',
            '-mcpu=cortex-m7',
            '-mthumb',
            '-mfpu=fpv5-d16',
            '-mfloat-abi=hard',
            '-DSTM32F7xx',
            '-DUSE_HAL_DRIVER',
            '-Isrc',
            '-Ilib',
            '-IInc',  # STM32 HAL头文件
            '-I../Drivers/STM32F7xx_HAL_Driver/Inc',
            '-I../Drivers/STM32F7xx_HAL_Driver/Inc/Legacy',
            '-I../Drivers/CMSIS/Device/ST/STM32F7xx/Include',
            '-I../Drivers/CMSIS/Include',
            '-I../Middlewares/Third_Party/FreeRTOS/Source/include',
            '-I../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS',
            '-I../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM7/r0p1'
            ),
        add_ldflags(
            '-mcpu=cortex-m7',
            '-mthumb',
            '-mfpu=fpv5-d16',
            '-mfloat-abi=hard',
            '-specs=nano.specs',
            '-TSTM32F746NGHx_FLASH.ld',
            '-Wl,--gc-sections',
            '-static',
            '--specs=nosys.specs',
            '-specs=nano.specs',
            '-Wl,--start-group',
            '-lc',
            '-lm',
            '-lstdc++',
            '-lsupc++',
            '-Wl,--end-group'
            ),
    elif platform.system()=='Darwin':
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
        os.system(yui)
    elif is_plat("stm32"):
        # STM32平台通过调试器运行
        print("STM32 target, run through debugger (e.g., ST-Link)")
        print("Binary file:", targetfile)
        # 这里可以添加通过ST-Link或其他调试器运行的代码
    else:
        # 直接使用 Python 的 subprocess 来运行，确保环境变量正确传递
        import subprocess
        cmd = ["./" + targetfile]
        
        # 复制当前环境变量并确保关键变量存在
        env = os.environ.copy()
        if platform.system()=='Darwin':
            env['DYLD_FRAMEWORK_PATH'] = '../libs'
            env['DYLD_LIBRARY_PATH'] = '../libs'
        elif platform.system()=='Linux':
            env['LD_LIBRARY_PATH'] = '../libs'
        
        print('run', ' '.join(cmd))
        subprocess.run(cmd, env=env)

def add_run():
    on_run(run)

def get_prefix():
    return prefix_env

add_buildin('add_flags',add_flags)
add_buildin('add_run',add_run)
add_buildin('get_prefix',get_prefix)


includes("./src/ya.py")
includes("./lib/ya.py")

includes("./app/ya.py")

includes("./tests/ya.py")