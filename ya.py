import platform
import os

project("yui",
    version='0.0.1',
    desc='yui is an gui for ai mean agui, project page: https://github.com/evilbinary/yui',
    targets=[
        'yui',
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

def _resolve_emscripten_tool(name):
    import shutil

    def _pick_emscripten_dir():
        emscripten = os.environ.get("EMSCRIPTEN")
        if emscripten and os.path.isdir(emscripten):
            return emscripten
        emsdk = os.environ.get("EMSDK")
        if emsdk:
            candidate = os.path.join(emsdk, "upstream", "emscripten")
            if os.path.isdir(candidate):
                return candidate
        for root in (
            r"E:\workspace\emsdk",
            r"E:\soft\emsdk",
            os.path.expanduser(r"~\emsdk"),
        ):
            candidate = os.path.join(root, "upstream", "emscripten")
            if os.path.isdir(candidate):
                return candidate
        return None

    if platform.system() == "Windows":
        for candidate in (name + ".bat", name + ".cmd", name):
            path = shutil.which(candidate)
            if path:
                return path
        emscripten = _pick_emscripten_dir()
        if emscripten:
            for ext in (".bat", ".cmd", ""):
                path = os.path.join(emscripten, name + ext)
                if os.path.isfile(path):
                    return path
    else:
        path = shutil.which(name)
        if path:
            return path
    return name

_mingw64_root_cache = None

def _resolve_mingw64():
    """Locate MSYS2 mingw64 toolchain root on Windows."""
    global _mingw64_root_cache
    if _mingw64_root_cache is not None:
        return _mingw64_root_cache

    import shutil

    def _valid_root(root):
        if not root:
            return None
        root = os.path.normpath(root)
        gcc = os.path.join(root, "bin", "gcc.exe")
        sdl_inc = os.path.join(root, "include", "SDL2")
        if os.path.isfile(gcc) and os.path.isdir(sdl_inc):
            return root
        return None

    for key in ("MINGW64", "MINGW64_HOME", "MINGW_PREFIX"):
        found = _valid_root(os.environ.get(key))
        if found:
            _mingw64_root_cache = found
            return found

    msys2 = os.environ.get("MSYS2_ROOT") or os.environ.get("MSYS2_PREFIX")
    if msys2:
        found = _valid_root(os.path.join(msys2, "mingw64"))
        if found:
            _mingw64_root_cache = found
            return found

    gcc_path = shutil.which("gcc")
    if gcc_path:
        bin_dir = os.path.dirname(os.path.normpath(gcc_path))
        found = _valid_root(os.path.dirname(bin_dir))
        if found:
            _mingw64_root_cache = found
            return found

    for root in (
        r"E:\soft\msys2\mingw64",
        r"C:\msys64\mingw64",
        r"D:\app\msys2\mingw64",
        os.path.expanduser(r"~\scoop\apps\msys2\current\mingw64"),
    ):
        found = _valid_root(root)
        if found:
            _mingw64_root_cache = found
            return found

    print("warning: mingw64 not found; set MINGW64/MSYS2_ROOT or install MSYS2")
    _mingw64_root_cache = r"C:\msys64\mingw64"
    return _mingw64_root_cache

def configure_emscripten_toolchain(target=None):
    if get_plat() not in ("em", "emscripten", "em-lvgl"):
        return
    # Debian/system emscripten ships FROZEN_CACHE=True with ports under
    # /usr/share/emscripten/cache/ports (not writable). Override so -sUSE_SDL=2
    # can fetch/build SDL2 into a user-owned cache. Empty EM_FROZEN_CACHE is
    # falsy and disables the package default (string "0" would still be truthy).
    if "EM_FROZEN_CACHE" not in os.environ:
        os.environ["EM_FROZEN_CACHE"] = ""
    if not os.environ.get("EM_CACHE"):
        cache_dir = os.path.join(os.path.expanduser("~"), ".emscripten_cache")
        os.makedirs(cache_dir, exist_ok=True)
        os.environ["EM_CACHE"] = cache_dir
    if not os.environ.get("EM_PORTS"):
        ports_dir = os.path.join(os.environ["EM_CACHE"], "ports")
        os.makedirs(ports_dir, exist_ok=True)
        os.environ["EM_PORTS"] = ports_dir
    tool = get_toolchain_node()
    if not tool:
        print("warning: emscripten toolchain not found")
        return
    cc = _resolve_emscripten_tool("emcc")
    ar = _resolve_emscripten_tool("emar")
    if platform.system() == "Windows" and (cc == "emcc" or not os.path.isfile(cc)):
        print("warning: emcc not found. Activate emsdk or set EMSDK/EMSCRIPTEN.")
    tool["cc"] = cc
    tool["cxx"] = cc
    tool["ld"] = cc
    tool["ar"] = ar

def _emscripten_sdl_flags():
    """Compile + link: emcc ports need these on both sides for SDL2 headers/libs."""
    return [
        "-sUSE_SDL=2",
        "-sUSE_SDL_IMAGE=2",
        "-sSDL2_IMAGE_FORMATS=[\"png\",\"jpg\"]",
        "-sUSE_SDL_TTF=2",
    ]

def _emscripten_link_flags():
    # ymake ldflags dedup breaks paired "-s", "FOO=1"; use -sFOO=1 single tokens.
    return [
        *_emscripten_sdl_flags(),
        "-sALLOW_MEMORY_GROWTH=1",
        "-sASSERTIONS=2",
        "-sEXPORT_ALL=1",
        "-sSTACK_OVERFLOW_CHECK=2",
        "-sERROR_ON_UNDEFINED_SYMBOLS=0",
        "-sAGGRESSIVE_VARIABLE_ELIMINATION=1",
        "--preload-file=app/playground/",
        "--preload-file=app/assets/",
        "--preload-file=app/lib/",
        "--preload-file=app",
        "-O0",
        "-lm",
    ]

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
    add_cflags(
        "-g",
        "-fPIC",
        "-D__ANDROID__",
        "-DYUI_BACKEND_MOBILE",
        "-I.",
        "-Isrc",
        "-Ilib",
    )
    add_ldflags(
        "-fPIC",
        "-landroid",
        "-llog",
        "-lEGL",
        "-lGLESv2",
        "-lm",
    )

_ios_toolchain_cache = {}

def _ios_sdk_name():
    sdk = os.environ.get("IOS_SDK", "")
    if sdk in ("iphoneos", "iphonesimulator"):
        return sdk
    arch = get_arch()
    if arch in ("x86_64", "i386"):
        return "iphonesimulator"
    return "iphoneos"

def configure_ios_toolchain(target=None):
    if get_plat() != "ios":
        return
    import subprocess
    sdk = _ios_sdk_name()
    try:
        sdk_path = subprocess.check_output(
            ["xcrun", "--sdk", sdk, "--show-sdk-path"], text=True
        ).strip()
        cc = subprocess.check_output(["xcrun", "--sdk", sdk, "-f", "clang"], text=True).strip()
        cxx = subprocess.check_output(["xcrun", "--sdk", sdk, "-f", "clang++"], text=True).strip()
        ar = subprocess.check_output(["xcrun", "--sdk", sdk, "-f", "ar"], text=True).strip()
    except (OSError, subprocess.CalledProcessError):
        print("warning: Xcode toolchain not found (need macOS + xcrun)")
        return

    arch = get_arch()
    if (not arch or arch == "None") and target is not None:
        arch = target.get_arch()
    if not arch or arch == "None":
        arch = os.environ.get("YMAKE_ARCH") or os.environ.get("ARCH") or "arm64"
    if sdk == "iphonesimulator" and arch not in ("arm64", "x86_64"):
        arch = "arm64"

    _ios_toolchain_cache["sdk"] = sdk
    _ios_toolchain_cache["sdk_path"] = sdk_path
    _ios_toolchain_cache["arch"] = arch

    tool = get_toolchain_node()
    if not tool:
        print("warning: gcc toolchain not found for ios build")
        return
    tool["cc"] = cc
    tool["cxx"] = cxx
    tool["ld"] = cc
    tool["ar"] = ar

def _add_ios_compile_flags():
    if not _ios_toolchain_cache:
        configure_ios_toolchain()
    sdk = _ios_toolchain_cache.get("sdk")
    sdk_path = _ios_toolchain_cache.get("sdk_path")
    arch = _ios_toolchain_cache.get("arch", "arm64")
    if not sdk or not sdk_path:
        return
    min_ver = os.environ.get("IOS_DEPLOYMENT_TARGET", "15.0")
    add_cflags(
        "-g",
        "-fPIC",
        "-DYUI_BACKEND_MOBILE",
        "-I.",
        "-Isrc",
        "-Ilib",
        "-Iplatform/ios",
        "-arch", arch,
        "-isysroot", sdk_path,
        "-m%s-version-min=%s" % ("ios-simulator" if sdk == "iphonesimulator" else "ios", min_ver),
    )
    add_ldflags(
        "-arch", arch,
        "-isysroot", sdk_path,
        "-m%s-version-min=%s" % ("ios-simulator" if sdk == "iphonesimulator" else "ios", min_ver),
    )

def add_flags():
    checkmem=True
    if platform.system()=='Windows':
        checkmem=False
        
    if checkmem and not is_plat("stm32") and get_plat() not in ("android", "ios", "em", "emscripten", "em-lvgl"):
        tool=get_toolchain_node()
        tool['ld']='gcc'
        add_cflags(
            '-fsanitize=address',
        )
        add_ldflags(
            '-fsanitize=address',
        )

    if get_plat() in ['emscripten', 'em', 'em-lvgl']:
        set_toolchain('emscripten')
        configure_emscripten_toolchain()
        before_build(configure_emscripten_toolchain)
        # -sUSE_SDL=2 must be on cflags too, else #include <SDL.h> resolves to SDL1
        # in emscripten sysroot (SDL_Color has no .a, no SDL_PIXELFORMAT_RGBA32, …).
        add_cflags(
            '-g',
            '-Wno-incompatible-pointer-types',
            '-Wno-implicit-function-declaration',
            '-I.',
            '-I./src/components',
            '-I./src/',
            *_emscripten_sdl_flags(),
        )
        if platform.system() != 'Windows':
            add_cflags('-gsource-map')
        add_ldflags(
            *_emscripten_link_flags(),
            '-Wbad-function-cast',
            '-Wcast-function-type',
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
        configure_ios_toolchain()
        _add_ios_compile_flags()
        before_build(configure_ios_toolchain)
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
            '-I/usr/include/SDL2',
            '-Isrc',
            '-Ilib',
            '-Ilib/mquickjs',
            '-DHAS_JS_MODULE',
            )
        add_ldflags(
            '-lSDL2',
            '-lSDL2_ttf',
            '-lSDL2_image',
            '-lm',
            '-lpthread',
            '-ldl',
            )
    elif platform.system()=='Windows':
        mingw64 = _resolve_mingw64()
        if get_plat() in ['emscripten', 'em', 'em-lvgl']:
            configure_emscripten_toolchain()
        else:
            tool=get_toolchain_node()
            tool['cc']=os.path.join(mingw64, 'bin', 'gcc.exe')
            tool['cxx']=os.path.join(mingw64, 'bin', 'g++.exe')
            tool['ld']=os.path.join(mingw64, 'bin', 'gcc.exe')
            tool['ar']=os.path.join(mingw64, 'bin', 'ar.exe')

            add_cflags(
                '-g',
                '-Wno-incompatible-pointer-types -Wno-implicit-function-declaration',
                '-F../libs/',
                '-I'+os.path.join(mingw64, 'include', 'SDL2'),
                '-I.',
                '-I./src/components',
                '-I./src/'
                )
            add_ldflags(
            '-L'+os.path.join(mingw64, 'lib'),
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
    import subprocess
    import sys
    argv = sys.argv[1:]
    extra = []
    if '--' in argv:
        extra = argv[argv.index('--') + 1:]
    if platform.system()=='Windows':
        yui=targetfile.replace('/','\\')
        cmd = [yui] + extra
        print('run', ' '.join(cmd))
        subprocess.run(cmd)
    elif is_plat("stm32"):
        # STM32平台通过调试器运行
        print("STM32 target, run through debugger (e.g., ST-Link)")
        print("Binary file:", targetfile)
        # 这里可以添加通过ST-Link或其他调试器运行的代码
    else:
        # 直接使用 Python 的 subprocess 来运行，确保环境变量正确传递
        cmd = ["./" + targetfile] + extra
        
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
    if plat == "ios":
        sdk = os.environ.get("IOS_SDK", "iphoneos")
        dest_dir = os.path.join("third_party", "yui-prebuilt", plat, sdk, arch)
    os.makedirs(dest_dir, exist_ok=True)
    dest = os.path.join(dest_dir, os.path.basename(src))
    shutil.copy2(src, dest)
    print("[prebuilt] %s -> %s" % (src, dest))

rule("mobile.prebuilt")
after_build(after_build_mobile_prebuilt)
rule_end()

if get_plat() in ("android", "ios"):
    add_rules("mobile.prebuilt")

def after_build_web_artifacts(target):
    import shutil
    plat = target.plat()
    if plat not in ("em", "emscripten", "em-lvgl"):
        return
    name = target.get("name") or ""
    if not name.startswith("yui-web"):
        return
    arch = target.get_arch()
    mode = target.get_config("mode")
    if not mode:
        return
    base = name.replace(".html", "").replace(".js", "")
    dest_dir = os.path.join("platform", "web", "vanilla", "yui")
    os.makedirs(dest_dir, exist_ok=True)
    if name.startswith("yui-web-lvgl"):
        out_prefix = "yui-web-lvgl"
    else:
        out_prefix = "yui-web"
    mapping = {
        ".js": out_prefix + ".js",
        ".wasm": out_prefix + ".wasm",
        ".data": out_prefix + ".data",
    }
    build_dirs = []
    seen = set()
    for plat_name in (plat, "em", "em-lvgl", "emscripten"):
        for arch_name in (arch, "None"):
            path = os.path.join("build", str(plat_name), str(arch_name), str(mode))
            if path not in seen:
                seen.add(path)
                build_dirs.append(path)
    for ext, out_name in mapping.items():
        copied = False
        for build_dir in build_dirs:
            candidates = [
                os.path.join(build_dir, base + ext),
                os.path.join(build_dir, name + ext),
            ]
            if ext == ".js":
                candidates.append(os.path.join(build_dir, base))
            for src in candidates:
                if os.path.isfile(src):
                    dest = os.path.join(dest_dir, out_name)
                    shutil.copy2(src, dest)
                    print("[web] %s -> %s" % (src, dest))
                    copied = True
                    break
            if copied:
                break
    for stale in ("playground.js", "playground.wasm", "playground.data"):
        stale_path = os.path.join(dest_dir, stale)
        if os.path.isfile(stale_path):
            os.remove(stale_path)
            print("[web] removed stale %s" % stale_path)

rule("web.artifacts")
after_build(after_build_web_artifacts)
rule_end()

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