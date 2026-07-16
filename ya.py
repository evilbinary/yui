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

def apply_cli_arch():
    import sys

    argv = sys.argv[1:]
    if '--' in argv:
        argv = argv[:argv.index('--')]
    i = 0
    while i < len(argv):
        arg = argv[i]
        if arg in ('-a', '--a', '-arch', '--arch') and i + 1 < len(argv):
            set_arch(argv[i + 1])
            return
        i += 1

    if get_plat() in ('android', 'ios'):
        env_abi = os.environ.get('ANDROID_ABI') or os.environ.get('YMAKE_ARCH') or os.environ.get('ARCH')
        if env_abi:
            set_arch(env_abi)

apply_cli_arch()

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

def _ndk_host_dirs():
    if platform.system() == "Windows":
        return "windows-x86_64", ".cmd", ".exe"
    elif platform.system() == "Darwin":
        return "darwin-x86_64", "", ""
    else:
        return "linux-x86_64", "", ""

def _android_triple(arch):
    if arch in ("arm64-v8a", "arm64"):
        return "aarch64-linux-android21"
    if arch in ("armeabi-v7a", "arm"):
        return "armv7a-linux-androideabi21"
    return "aarch64-linux-android21"

def configure_android_toolchain(target=None):
    if get_plat() != "android":
        return
    ndk = os.environ.get("ANDROID_NDK_HOME") or os.environ.get("ANDROID_NDK_ROOT") or ""
    if not ndk:
        print("warning: ANDROID_NDK_HOME not set, android cross-build may fail")
        return
    host, clang_ext, bin_ext = _ndk_host_dirs()
    arch = get_arch()
    if (not arch or arch == "None") and target is not None:
        arch = target.get_arch()
    if not arch or arch == "None":
        arch = os.environ.get("ANDROID_ABI") or os.environ.get("YMAKE_ARCH")
    bin_dir = os.path.join(ndk, "toolchains", "llvm", "prebuilt", host, "bin")
    triple = _android_triple(arch)
    clang = os.path.join(bin_dir, triple + "-clang" + clang_ext)
    clangxx = os.path.join(bin_dir, triple + "-clang++" + clang_ext)
    ar = os.path.join(bin_dir, "llvm-ar" + bin_ext)
    if bin_ext and not os.path.isfile(ar):
        ar = os.path.join(bin_dir, "llvm-ar")
    tool = get_toolchain_node()
    if not tool:
        print("warning: gcc toolchain not found for android build")
        return
    tool["cc"] = clang
    tool["cxx"] = clangxx
    tool["ld"] = clang
    tool["ar"] = ar

def _add_android_compile_flags():
    ndk = os.environ.get("ANDROID_NDK_HOME") or os.environ.get("ANDROID_NDK_ROOT") or ""
    if not ndk:
        return
    host, _, _ = _ndk_host_dirs()
    sysroot = os.path.join(ndk, "toolchains", "llvm", "prebuilt", host, "sysroot")

    add_cflags(
        "-g",
        "-fPIC",
        "-D__ANDROID__",
        "-DYUI_BACKEND_MOBILE",
        "--sysroot",
        sysroot,
        "-I.",
        "-Isrc",
        "-Ilib",
    )
    add_ldflags(
        "-fPIC",
        "--sysroot",
        sysroot,
        "-landroid",
        "-llog",
        "-lEGL",
        "-lGLESv2",
        "-lm",
    )

def add_flags():
    checkmem=True
    if platform.system()=='Windows':
        checkmem=False
        
    if checkmem and not is_plat("stm32") and get_plat() not in ("android", "ios"):
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
            mingw64='D:\\app\\msys2\\mingw64'
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
            '-lm'
            )
        else:
            mingw64='../libs/'

            add_cflags(
            # '--use-port=sdl2 ',
            '-g',
            '-gsource-map',
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
                '-gsource-map',
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
                # '--source-map-base http://localhost:6931/',
                '-s ERROR_ON_UNDEFINED_SYMBOLS=0',
                '-s AGGRESSIVE_VARIABLE_ELIMINATION=1',
                '--preload-file app/playground/',
                '--preload-file app/assets/',
                '--preload-file app/lib/',
                '--preload-file app',
                # '-s USE_WEBGL2=1',          # 启用 WebGL2 支持（现代浏览器兼容性更好）
                # '-s FULL_ES2=1',           # 启用完整的 OpenGL ES 3 特性
                # '-s MIN_WEBGL_VERSION=2',  # 要求 WebGL 最低版本为 2
                '-s ASSERTIONS=2',          # 启用运行时断言，便于调试
                # '-s GL_ASSERTIONS=1',       # 启用 GL 断言，捕捉 WebGL 错误
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
    elif is_plat("android"):
        configure_android_toolchain()
        _add_android_compile_flags()
        before_build(configure_android_toolchain)
        if not (os.environ.get("ANDROID_NDK_HOME") or os.environ.get("ANDROID_NDK_ROOT")):
            print("warning: ANDROID_NDK_HOME not set, android cross-build may fail")
    elif is_plat("ios"):
        add_cflags(
            "-g",
            "-fPIC",
            "-DYUI_BACKEND_MOBILE",
            "-Isrc",
            "-Ilib",
        )
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
        mingw64='D:\\app\\msys2\\mingw64'
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
                # '--source-map-base http://localhost:6931/',

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
            '-lws2_32',
            '-ldwmapi'
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

def get_prefix():
    return prefix_env

def after_build_mobile_prebuilt(target):
    import shutil
    plat = target.plat()
    if plat not in ("android", "ios") or target.get("kind") != "static":
        return
    arch = target.get_arch()
    mode = target.get_config("mode")
    name = target.get("filename") or target.get("name")
    if not arch or arch == "None" or not mode or not name:
        return
    src = os.path.join("build", plat, arch, mode, "lib" + name + ".a")
    if not os.path.isfile(src):
        return
    dest_dir = os.path.join("third_party", "yui-prebuilt", plat, arch)
    os.makedirs(dest_dir, exist_ok=True)
    dest = os.path.join(dest_dir, os.path.basename(src))
    shutil.copy2(src, dest)
    print("[prebuilt] %s -> %s" % (src, dest))

rule("mobile.prebuilt")
after_build(after_build_mobile_prebuilt)
rule_end()

if get_plat() in ("android", "ios"):
    add_rules("mobile.prebuilt")

def add_run():
    on_run(run)

add_buildin('add_flags',add_flags)
add_buildin('add_run',add_run)
add_buildin('get_prefix',get_prefix)

includes("./src/ya.py")
includes("./lib/ya.py")

includes("./app/ya.py")

includes("./platform/ya.py")

includes("./tests/ya.py")