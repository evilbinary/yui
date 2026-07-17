# iOS platform

- **真机 + 模拟器**：`ya -p ios` 分别编译，产物在 `third_party/yui-prebuilt/ios/<sdk>/<arch>/`
- **绘制**：`backend_mobile.c` + `ios_metal_glue.mm`（Metal POC；Skia 待接）
- **JS**：QuickJS（`jsmodule-quickjs`），与 Android 一致

## 1. 编译 native 静态库（macOS + Xcode）

```bash
# 真机 arm64
IOS_SDK=iphoneos ya -p ios -a arm64 -m release -b yui-ios-prebuilt

# 模拟器 arm64（Apple Silicon Mac）
IOS_SDK=iphonesimulator ya -p ios -a arm64 -m release -b yui-ios-prebuilt

# 模拟器 x86_64（Intel Mac）
IOS_SDK=iphonesimulator ya -p ios -a x86_64 -m release -b yui-ios-prebuilt
```

或 Makefile：

```bash
make ios-device
make ios-simulator
```

## 2. 打 .app（CMake + Xcode）

`ya` 编完预编译库后，直接 CMake 打包（资源从 `app/` 自动打进 bundle，与 Android Gradle 的 `copyYuiAssets` 同理）：

```bash
# 生成 Xcode 工程并编译（真机）
cmake -G Xcode -S platform/ios -B build/ios-xcode \
  -DIOS_SDK=iphoneos -DIOS_ARCH=arm64
cmake --build build/ios-xcode --config Debug

# 模拟器
cmake -G Xcode -S platform/ios -B build/ios-sim-xcode \
  -DIOS_SDK=iphonesimulator -DIOS_ARCH=arm64
cmake --build build/ios-sim-xcode --config Debug
```

产物：`.app` 在 `build/ios-xcode/Debug/` 或 `build/ios-sim-xcode/Debug/`。

## 3. 目录

```text
platform/ios/
  YuiApp/              Swift UI（MTKView + CADisplayLink）
  YuiBridge.mm         ObjC++ 桥接 → yui_boot
  ios_metal_glue.mm    Metal 绘制 POC
  CMakeLists.txt       Xcode 工程生成
```

## 4. 桥接 API

与 Android JNI 对齐：

| Android | iOS |
|---------|-----|
| `nativeInit(surface, ...)` | `YuiBridge.initWithLayer(...)` |
| `nativeTick()` | `YuiBridge.tick` |
| `nativeOnTouch(...)` | `YuiBridge.onTouchWithPointerId(...)` |
| `Choreographer` | `CADisplayLink` |
| `ANativeWindow` | `CAMetalLayer` |

## 5. Skia（后续）

接入 `third_party/skia-pack/ios/Skia.xcframework` 后定义 `YUI_HAS_SKIA`，替换 Metal POC 绘制路径。
