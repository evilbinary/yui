# Android platform

- **minSdk** 21，**ABI** `arm64-v8a` + `armeabi-v7a`（Universal APK）
- **Skia**：`third_party/skia-pack/android/<abi>/`
- **JS**：构建时选 `yui-mobile-quickjs` 或 `yui-mobile-mqjs`（待加 target）

## 下一步

1. 拉取 [skia-pack](https://github.com/JetBrains/skia-pack) 到 `third_party/skia-pack/`
2. 完善 `app/build.gradle` + `jni/CMakeLists.txt` 链 `libyui.a`
3. `YuiView.kt` → JNI `nativeInit` / `nativeTick`

见 `jni/yui_bridge.cpp` 桩文件。
