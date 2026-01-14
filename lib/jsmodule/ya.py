# coding:utf-8
# *******************************************************************
# * Copyright 2025-present evilbinary
# * 作者: evilbinary on 01/20/2025
# * 邮箱: rootntsd@gmail.com
# ********************************************************************
target("yui-stdlib-host")
add_deps("mquickjs","cjson","socket")
set_kind("binary")
add_flags()
add_files('yui_stdlib_stubs.c',
          'yui_stdlib_build.c',
          '../mquickjs/mquickjs_build.c'
          )
add_cflags(' -Isrc/ -DCONFIG_CLASS_SOCKET -DCONFIG_CLASS_YUI ')
def after_build_host(target):
    # target.on_run(target)
    import subprocess
    targetfile = target.targetfile()
    print('gen yui_stdlib.h by exec',target.name())
    exe=get_prefix()+"./"+targetfile
    
    # 使用 subprocess 运行并捕获输出
    with open('lib/jsmodule/yui_stdlib.h', 'w') as f:
        result = subprocess.run([exe], stdout=f, stderr=subprocess.PIPE, text=True)
        if result.returncode != 0:
            print('Error generating yui_stdlib.h:', result.stderr)

after_build(after_build_host)



target("jsmodule")
add_deps("mquickjs","cjson","yui","socket")
add_cflags(' -DBUILD_NO_MAIN=1 -DHAS_JS_MODULE -DCONFIG_CLASS_SOCKET -DCONFIG_CLASS_YUI  -I. -I../mquickjs -g ')
add_flags()

set_kind("static")
add_files(
    'js_module.c',
    'js_common.c',
    'yui_stdlib.c',
    # 'yui_stdlib_link.c',  # 不需要，yui_stdlib.c 已经包含了所有需要的东西
    # 'js_socket.c'  # 不需要，已经在 yui_stdlib.c 中通过 #include 包含了
) 

add_includedirs('.', public=true)