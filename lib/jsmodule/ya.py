# coding:utf-8
# *******************************************************************
# * Copyright 2025-present evilbinary
# * 作者: evilbinary on 01/20/2025
# * 邮箱: rootntsd@gmail.com
# ********************************************************************
target("yui-stdlib-host")
add_deps("mquickjs","cjson","yui","jsmodule")
set_kind("binary")
add_flags()
add_files('yui_stdlib_build.c',
          '../mquickjs/mquickjs_build.c'
          )
add_cflags(' -Isrc/ ')
def after_build_host(target):
    # target.on_run(target)

    targetfile = target.targetfile()
    print('gen yui_stdlib.h by exec',target.name())
    exe=get_prefix()+"./"+targetfile

    os.shell(exe+' > lib/jsmodule/yui_stdlib.h')

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
    'yui_stdlib_link.c',
    'js_socket.c'
) 

add_includedirs('.', public=true)