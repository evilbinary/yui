all: playground  # main mqjs
mqjs:
	ya -b mqjs
	ya -r mqjs

main:
	ya -b main
	ya -r main

playground:
	ya -b playground
	ya -r playground

build-playground:
	ya -b playground
build-db:
	ya -b db
	
gdb: build-db
	gdb -x  gdb.gdb ./build/None/None/None/db.exe  app/db/db.json

build-lvgl:
	ya -b lvgl-sdl -p lvgl


gdb-lvgl: build-lvgl
	gdb -x  gdb.gdb  ./build/lvgl/None/None/lvgl-sdl.exe app/lvgl/calc.json

db:  
	ya -b db
	./build/None/None/None/db.exe  app/db/db.json

menu: build-playground
	./build/None/None/None/playground.exe  app/tests/test-menu.json

gdb-menu: build-playground
	gdb -x  gdb.gdb ./build/None/None/None/playground.exe  app/tests/test-menu.json

run: main
	ya -r main

clean:
	rm -rf build