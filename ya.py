project("yui",
    version='0.0.1',
    desc='yui is an gui for ai mean agui, project page: https://github.com/evilbinary/yui',
    targets=[
        'yui'
    ]
)



target("yui") 
(
    set_kind("binary"),
    add_cflags(
        '-g',
        '-I/usr/local/Cellar/cjson/1.7.17/include/cjson',
        '-F../libs/',
        '-I../libs/SDL2.framework/Headers',
        '-I../libs/SDL2_ttf.framework/Headers',
        '-I../libs/SDL2_image.framework/Headers'
        ),
    add_ldflags(
        '-F../libs/',
       '-framework SDL2',
        '-framework SDL2_ttf',
        '-framework SDL2_image ',
        '-lcjson ',
        '-F../libs/'
        ),
    add_files("*.c"),
)
