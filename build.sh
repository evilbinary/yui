#gcc main.c -I/usr/local/Cellar/cjson/1.7.17/include/cjson -lcjson -I/opt/local/include/ -L/opt/local/lib -lSDL2 -lSDL2_ttf -lSDL_image  -o main

gcc -g main.c -I/usr/local/Cellar/cjson/1.7.17/include/cjson -lcjson \
-F../libs/ -framework SDL2  -framework SDL2_ttf  -framework SDL2_image  \
 -I../libs/SDL2.framework/Headers \
 -I../libs/SDL2_ttf.framework/Headers \
 -I../libs/SDL2_image.framework/Headers \
-o main