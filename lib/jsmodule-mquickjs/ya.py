# coding:utf-8
# *******************************************************************
# * Copyright 2025-present evilbinary
# * 作者: evilbinary on 01/20/2025
# * 邮箱: rootntsd@gmail.com
# ********************************************************************
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
    add_cflags(' -Isrc/ -I../jsmodule -DCONFIG_CLASS_SOCKET -DCONFIG_CLASS_YUI -DSTDLIB_BUILD -DYUI_BACKEND_EMBEDDED ')
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
            result = subprocess.run([exe], stdout=f, stderr=subprocess.PIPE, text=True)
            if result.returncode != 0:
                print('Error generating yui_stdlib.h:', result.stderr)
        # 同时生成 32 位表（esp32/stm32 等嵌入式平台使用）
        with open('lib/jsmodule-mquickjs/yui_stdlib_32.h', 'w') as f:
            result = subprocess.run([exe, '-m32'], stdout=f, stderr=subprocess.PIPE, text=True)
            if result.returncode != 0:
                print('Error generating yui_stdlib_32.h:', result.stderr)

    after_build(after_build_host)


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
