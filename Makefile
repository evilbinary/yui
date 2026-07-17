all: playground  # main mqjs
mqjs:
	ya -b mqjs
	ya -r mqjs

main:
	ya -b main
	ya -r main

playground:
	ya -r playground

build-db:
	ya -b db

build-watch-os:
	ya -b watch-os

watch-os: build-watch-os
	ya -r watch-os --  app/watch-os/app.json


build-lvgl:
	ya -b lvgl-sdl -p lvgl

lvgl-sdl: build-lvgl
	ya -p lvgl  -r lvgl-sdl #-- app/lvgl/calc.json
db:  
	ya -r db -- app/db/db.json

menu: build-playground
	ya -r playground -- app/tests/test-menu.json



gdb: build-db
	gdb -x  gdb.gdb ./build/None/None/None/db.exe  app/db/db.json

gdb-watch: build-db
	gdb -x  gdb.gdb ./build/None/None/None/db.exe  app/watch-os/app.json

gdb-lvgl: build-lvgl
	gdb -x  gdb.gdb  ./build/lvgl/None/None/lvgl-sdl.exe app/lvgl/calc.json

gdb-menu: build-playground
	gdb -x  gdb.gdb ./build/None/None/None/playground.exe  app/tests/test-menu.json



android-arm64:
	ya -p android -a arm64-v8a -m release -b yui-android-prebuilt

android-armv7:
	ya -p android -a armeabi-v7a -m release -b yui-android-prebuilt

android: android-arm64 android-armv7
	cd platform/android && ./gradlew :app:assembleDebug
	@echo "APK: platform/android/app/build/outputs/apk/debug/app-debug.apk"

android-install: android
	cd platform/android && ./gradlew :app:installDebug 

run: main
	ya -r main

clean:
	rm -rf build