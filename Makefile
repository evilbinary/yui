all: playground  # main mqjs
mqjs:
	ya -b mqjs
	ya -r mqjs

main:
	ya -b main
	ya -r main

playground:
	ya -r playground

build-playground:
	ya -b playground
	
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

gallery:
	ya -r playground -- app/gallery/app.json

photo:
	ya -b photo
	ya -r photo -- app/photo/app.json

menu: build-playground
	ya -r playground -- app/tests/test-menu.json

game: build-playground
	ya -r playground -- app/game/demo.json

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

# 快速修补 prebuilt（event.c + input/state.c）；完整重建仍用 android-arm64/android-armv7
android-patch-prebuilt:
	python scripts/patch_android_prebuilt.py

android: android-arm64 android-armv7
	cd platform/android && ./gradlew :app:assembleDebug
	@echo "APK: platform/android/app/build/outputs/apk/debug/app-debug.apk"

android-install: android
	cd platform/android && ./gradlew :app:installDebug

ios-device:
	IOS_SDK=iphoneos ya -p ios -a arm64 -m release -b yui-ios-prebuilt

ios-simulator:
	IOS_SDK=iphonesimulator ya -p ios -a arm64 -m release -b yui-ios-prebuilt

ios-simulator-x64:
	IOS_SDK=iphonesimulator ya -p ios -a x86_64 -m release -b yui-ios-prebuilt

ios-app:
	cmake -G Xcode -S platform/ios -B build/ios-xcode -DIOS_SDK=iphoneos -DIOS_ARCH=arm64
	cmake --build build/ios-xcode --config Debug
	@echo "App: build/ios-xcode/Debug/YuiApp.app"

web:
	ya -p em -m release -b yui-web.js
	@echo "WASM: platform/web/vanilla/yui/yui-web.js"

web-lvgl:
	ya -p em-lvgl -m release -b yui-web-lvgl.js
	@echo "WASM: platform/web/vanilla/yui/yui-web-lvgl.js"

web-serve: web
	cd platform/web/vanilla && python -m http.server 8080

web-serve-lvgl: web-lvgl
	cd platform/web/vanilla && python -m http.server 8080

run: main
	ya -r main

PYTHON ?= python3

test:
	$(PYTHON) scripts/run_tests.py

test-unit:
	$(PYTHON) scripts/run_tests.py --unit

test-integration:
	$(PYTHON) scripts/run_tests.py --integration

test-perf:
	$(PYTHON) scripts/run_tests.py --perf

test-e2e:
	$(PYTHON) scripts/run_tests.py --e2e

test-visual:
	$(PYTHON) scripts/run_tests.py --visual

test-visual-update:
	$(PYTHON) scripts/run_tests.py --visual --update-baselines

test-all:
	$(PYTHON) scripts/run_tests.py --all

clean:
	rm -rf build