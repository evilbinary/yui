# Android platform

- **minSdk** 21，**ABI** `arm64-v8a` + `armeabi-v7a`（Universal APK）
- **绘制**：`backend_mobile.c` + EGL/GLES2（POC；文字/图片待 Skia）
- **JS**：QuickJS（`jsmodule-quickjs`），与 `yui-pc` 一致

## 1. 编译 native 静态库

```bash
# 构建 + after_build 自动拷贝到 third_party/yui-prebuilt/android/<abi>/
ya -p android -a arm64-v8a -m release -b yui-android-prebuilt
ya -p android -a armeabi-v7a -m release -b yui-android-prebuilt
```

或 Makefile：

```bash
make android-arm64
make android-armv7
make android          # 两个 ABI 都打
```

若 JNI 链接报 `undefined symbol: handle_window_event`（prebuilt 落后于 `src/event.c`），可先快速修补：

```bash
# 需 ANDROID_NDK_HOME
python scripts/patch_android_prebuilt.py
# 或 make android-patch-prebuilt
```

再重新 `./gradlew :app:assembleDebug`。

需先设置 `ANDROID_NDK_HOME`（或 `ANDROID_NDK_ROOT`）。`ya` 与 Gradle 共用该变量，无需在 `local.properties` 写 `ndk.dir`。

`ANDROID_HOME` / `ANDROID_SDK_ROOT` 用于定位 Android SDK；`local.properties` 会在路径无效时自动同步，无需手写 `cmake.dir`（由 `app/build.gradle.kts` 的 `cmake { version = "3.22.1" }` 解析）。

## 2. 打 APK

```bash
cd platform/android
# local.properties: sdk.dir=...（Android SDK，由 Android Studio 自动生成，勿提交 git）
```

设置 `JAVA_HOME` 指向 Android Studio 的 `jbr` 目录，并确保 `ANDROID_NDK_HOME` 已设置，然后：

```bash
./gradlew :app:assembleDebug
```

成功后在（`build/` 被 gitignore，IDE 里可能不显示）：

```text
platform/android/app/build/outputs/apk/debug/app-debug.apk
```

安装到已连接设备：`./gradlew :app:installDebug`

`app/build.gradle.kts` 会在构建前把 `app/tests/login.json` 和 `app/assets/` 拷入 `assets/`。

## 3. 目录

```text
platform/android/
  app/src/main/java/com/yui/   YuiActivity, YuiView
  app/src/main/cpp/CMakeLists.txt
  jni/yui_bridge.cpp
```

## 4. Skia（后续）

POC 使用 GLES 画纯色矩形。接入 [skia-pack](https://github.com/JetBrains/skia-pack) 后定义 `YUI_HAS_SKIA` 替换绘制路径。

## 5. mquickjs 变体

```bash
ya -p android -a arm64-v8a -b yui cjson yaml2json mquickjs jsmodule socket
```

CMake 链 `libjsmodule.a` + `libmquickjs.a`（需改 `CMakeLists.txt` 或增加 flavor）。
