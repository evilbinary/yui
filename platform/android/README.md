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

需先设置 `ANDROID_NDK_HOME`。

## 2. 打 APK

```bash
cd platform/android
# 创建 local.properties: sdk.dir=C\:\\Android\\sdk
./gradlew :app:assembleDebug
```

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
