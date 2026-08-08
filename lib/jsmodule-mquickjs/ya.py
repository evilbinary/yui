# coding:utf-8
# *******************************************************************
# * Copyright 2025-present evilbinary
# * 作者: evilbinary on 01/20/2025
# * 邮箱: rootntsd@gmail.com
# ********************************************************************
import os
import sys
if is_host_plat():
    # 宿主工具，生成 yui_stdlib.h / yui_stdlib_32.h（嵌入式平台编译产物无法在宿主运行）
    target("yui-stdlib-host")
    add_deps("mquickjs","cjson","socket")
    set_kind("binary")
    add_flags()
    add_files('yui_stdlib_stubs.c',
              'yui_stdlib_build.c',
              '../mquickjs/mquickjs_build.c'
              )

    add_includedirs('.', '..', '../jsmodule', public=true)
    add_cflags(' -Isrc/ -I../jsmodule -DCONFIG_CLASS_SOCKET -DCONFIG_CLASS_YUI -DSTDLIB_BUILD -DYUI_BACKEND_EMBEDDED -DYUI_WITH_GAME=1 ')
    def after_build_host(target):
        # target.on_run(target)
        import subprocess
        import os
        targetfile = target.targetfile()
        print('gen yui_stdlib.h by exec',target.name())
        exe=get_prefix()+"./"+targetfile
        # 生成器在 ASan 下运行会因 LeakSanitizer 检测到泄漏以非零码退出，
        # 且退出时不冲刷 stdout，导致生成的表文件被截断。关闭泄漏检测。
        env = dict(os.environ)
        env['ASAN_OPTIONS'] = (env.get('ASAN_OPTIONS', '') + ' detect_leaks=0').strip()
        # 使用 subprocess 运行并捕获输出

        with open('lib/jsmodule-mquickjs/yui_stdlib.h', 'w') as f:
            result = subprocess.run([exe], stdout=f, stderr=subprocess.PIPE, text=True, env=env)
            if result.returncode != 0:
                print('Error generating yui_stdlib.h:', result.stderr)
        # 同时生成 32 位表（esp32/stm32 等嵌入式平台使用）
        with open('lib/jsmodule-mquickjs/yui_stdlib_32.h', 'w') as f:
            result = subprocess.run([exe, '-m32'], stdout=f, stderr=subprocess.PIPE, text=True, env=env)
            if result.returncode != 0:
                print('Error generating yui_stdlib_32.h:', result.stderr)

    after_build(after_build_host)

    # PC 端字节码编译工具：用 js_yuistdlib 表（含 YUI/Socket atom）把 JS 编译
    # 成 mquickjs 字节码，保证编译期解析的全局标识符与 ESP32 运行时一致。
    # 必须用 32 位工具链（JSW=4）直接产出 32 位字节码：64 位主机 + -m32 的
    # JS_PrepareBytecode64to32 会因 64/32 位表的 atom 顺序不同而错位，运行时
    # 找不到 YUI 等全局对象。yui_stdlib_32.h 是 32 位表，需 32 位 gcc 编译。
    def configure_bcgen_mingw32(target=None):
        import os
        mingw32 = r"E:\soft\msys2\mingw32"
        if not os.path.isfile(os.path.join(mingw32, 'bin', 'gcc.exe')):
            print("WARN: mingw32 toolchain not found at " + mingw32)
            return
        tool = get_toolchain_node()
        if tool is None:
            print("WARN: toolchain node not found, skip mingw32 override")
            return
        tool['cc'] = os.path.join(mingw32, 'bin', 'gcc.exe')
        tool['cxx'] = os.path.join(mingw32, 'bin', 'g++.exe')
        tool['ld'] = os.path.join(mingw32, 'bin', 'gcc.exe')
        tool['ar'] = os.path.join(mingw32, 'bin', 'ar.exe')
        os.environ['PATH'] = os.path.join(mingw32, 'bin') + os.pathsep + os.environ.get('PATH', '')

    # 全局 buildin add_flags()（lib/ya.py 顶层调用）会把 pc 后端的 SDL2/ASan 链接参数
    # 挂到 lib 祖先节点上，经 node_get_parent_all 泄漏进 bc-gen 的链接命令（bc-gen 是
    # lib 子树下唯一的宿主 binary）。这里在链接前从祖先节点剔除这些 pc 后端专属参数，
    # 不影响其它目标（lib 下其余 target 均为 .a 静态库，不参与链接）。
    def strip_bcgen_leaked_ldflags(target=None):
        _junk_c = ('-fsanitize',)
        _junk_l = ('-lSDL', '-fsanitize', '-framework', '-Wl,--no-as-needed')
        n = target
        while n is not None:
            n = n.get('parent') if hasattr(n, 'get') else None
            if not n:
                break
            if n.get('cflags'):
                n['cflags'] = [f for f in n['cflags']
                               if not f.startswith(_junk_c)]
            if n.get('ldflags'):
                n['ldflags'] = [f for f in n['ldflags']
                                if not f.startswith(_junk_l)]

    target("bc-gen")
    set_kind("binary")
    set_toolchain('gcc')
    # 隔离 32 位(-m32)对象到独立目录：bc-gen 与 64 位 mquickjs 静态库共用
    # cutils.c/dtoa.c/libm.c，默认同一对象路径会互相覆盖（-m32 编译产物污染
    # 64 位库，导致 PC 端 yui-stdlib-host / 测试链接报 i386 与 x86-64 不匹配）。
    def bcgen_objdir(target=None):
        if target is not None:
            target['build-obj-dir'] = 'build/{plat}/{arch}/{mode}/objs32/'
    on_config(bcgen_objdir)
    # 独立宿主工具，不调用全局 add_flags()（那会引入 SDL2/ASan 等 pc 后端依赖，
    # 且与 -m32 冲突）。bc-gen 只需引擎 + socket.c 胶水，下面显式给出全部编译选项。
    # 自包含 32 位引擎：不依赖已编译的 64 位 PC 库，直接编译引擎源 + bc_gen.c。
    # 必须用 32 位字宽（JSW=4 / 无 JS_PTR64）产出 32 位字节码，atom 与 ESP32 的
    # yui_stdlib_32.h 严格一致（YUI/Socket 运行时可见）。Windows 走 mingw32（已是
    # 32 位）；Linux/macOS 用宿主 gcc 的 -m32（ILP32），保证指值 32 位、字节码位宽
    # 与 ESP32 一致（64 位主机直接 JSW=4 会把堆指针截断进 32 位 JSValue，产出错位
    # 字节码，故必须 -m32）。
    add_files('bc_gen.c',
              '../mquickjs/cutils.c',
              '../mquickjs/dtoa.c',
              '../mquickjs/libm.c',
              '../mquickjs/mquickjs.c',
              '../socket/socket.c',
              '../cjson/cJSON.c')
    add_includedirs('.', '..', '../jsmodule', 'src', '../cjson', '../../src/perf', public=true)
    add_cflags(' -DCONFIG_CLASS_SOCKET -DCONFIG_CLASS_YUI -DSTDLIB_BUILD -DYUI_BACKEND_EMBEDDED -DNO_MAIN -UJS_PTR64 ')
    if sys.platform != 'win32':
        # Linux/macOS：非 mingw 工具链，用宿主 gcc 的 -m32 产出 32 位 ELF（ILP32）
        add_cflags(' -m32 ')
        add_ldflags(' -m32 ')
    before_build(configure_bcgen_mingw32)
    before_build(strip_bcgen_leaked_ldflags)

# ESP32/STM32：32 位表（mqjs_stdlib_32.h / yui_stdlib_32.h）由 JS_PTR64 条件选择
target("jsmodule-mquickjs")
add_deps("mquickjs","cjson","yui","socket")
add_cflags(' -DBUILD_NO_MAIN=1 -DHAS_JS_MODULE -DCONFIG_CLASS_SOCKET -DCONFIG_CLASS_YUI  -I. -I../mquickjs -g -Wno-implicit-function-declaration ')
if get_plat() in ("esp32", "stm32"):
    # 嵌入式模式：ytype.h 使用 YuiTexture/YuiFont，不依赖 SDL
    add_cflags('-DYUI_BACKEND_EMBEDDED')
    # YUI_MAX_PATH / YUI_PATH_MAX / MAX_JS_EVENTS / MAX_C_EVENT_HANDLERS 已统一
    # 收敛到根 ya.py 的 add_flags()，此处不再重复。
add_flags()

set_kind("static")
add_files(
    'js_module.c',
    '../jsmodule/js_common.c',
    'mqjs_shim.c',
    # 'yui_stdlib.c',
    'yui_stdlib_link.c',  # 不需要，yui_stdlib.c 已经包含了所有需要的东西
    # 'js_socket.c'  # 不需要，已经在 yui_stdlib.c 中通过 #include 包含了
)

add_includedirs('.', '..', '../jsmodule', public=true)
