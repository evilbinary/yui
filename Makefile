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

# ESP32/ESP32-C3 targets
# 跨平台 wrapper：使用 Python 脚本激活 ESP-IDF 环境
ESP32_PORT ?= COM3
ESP32_IDF_WRAPPER := python scripts/run_esp32_idf.py

esp32-build:
	ya -p esp32 -a esp32c3
	$(ESP32_IDF_WRAPPER) build

# 生成子集字体（供 font 分区烧录 / QEMU 镜像合并）
esp32-font:
	python scripts/subset_font.py --input app/assets/Roboto-Regular.ttf \
		--output platform/esp32/build/font-subset.ttf --scan app/

# 生成 SPIFFS 镜像（app/watch-os + app/assets，供 spiffs 分区烧录 / QEMU 镜像合并）
esp32-spiffs:
	$(ESP32_IDF_WRAPPER) make-spiffs

esp32-flash: esp32-build esp32-font esp32-spiffs
	$(ESP32_IDF_WRAPPER) -p $(ESP32_PORT) flash
	$(ESP32_IDF_WRAPPER) write-font
	$(ESP32_IDF_WRAPPER) write-spiffs

esp32-monitor:
	$(ESP32_IDF_WRAPPER) -p $(ESP32_PORT) monitor

esp32-flash-monitor: esp32-flash
	$(ESP32_IDF_WRAPPER) -p $(ESP32_PORT) monitor

esp32-qemu: esp32-build esp32-font esp32-spiffs
	$(ESP32_IDF_WRAPPER) qemu --graphics monitor

esp32-qemu-headless: esp32-build esp32-font esp32-spiffs
	$(ESP32_IDF_WRAPPER) qemu monitor

esp32-menuconfig:
	$(ESP32_IDF_WRAPPER) menuconfig

esp32-size:
	$(ESP32_IDF_WRAPPER) size

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