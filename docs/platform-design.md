# YUI Platform 层设计

> 版本：草案 v0.2  
> 日期：2026-07-16  
> 状态：已评审，待实现

## 目录

- [背景与目标](#背景与目标)
- [总体架构](#总体架构)
- [三层职责划分](#三层职责划分)
- [目录结构](#目录结构)
- [统一启动 API](#统一启动-api)
- [libyui 构建与交付](#libyui-构建与交付)
- [platform 如何消费 libyui](#platform-如何消费-libyui)
- [移动端 Backend（Skia）](#移动端-backendskia)
- [各 Platform 设计](#各-platform-设计)
- [GPU 方案对比](#gpu-方案对比)
- [构建系统](#构建系统)
- [资源与路径约定](#资源与路径约定)
- [分阶段实施计划](#分阶段实施计划)
- [风险与对策](#风险与对策)
- [Android 双 ABI 打包说明](#android-双-abi-打包说明)
- [已确认决策](#已确认决策)

---

## 背景与目标

YUI 当前桌面端通过 `app/main.c` + `backend_sdl.c` 直接运行；Web 端通过 Emscripten 编译为 WASM。移动端（Android / iOS）尚无实现。

本设计引入 **`platform/` 宿主层**，将「引擎」与「平台壳」分离，使同一套 `app/` 业务（JSON + JS）能在 PC、移动、Web 上复用。

### 设计目标

1. **引擎与壳分离**：`src/` 产出 `libyui.a`（或 WASM），`platform/` 只负责窗口、输入、打包
2. **业务复用**：`app/*.json`、`app/*.js`、`assets/` 各端共用，仅资源根路径由 platform 传入
3. **统一启动流程**：PC / Android / iOS / Web 共用 `yui_boot` 初始化逻辑（从 `main.c` 抽取）
4. **移动端首选 Skia**：Android 用 GLES，iOS 用 Metal；绘制逻辑集中在**单文件** `backend_mobile.c`
5. **Web 可选框架壳**：vanilla（纯 HTML）为默认；React / Vue / Angular 仅作 Canvas 外包装
6. **JS 引擎双栈**：移动端与桌面一致，QuickJS 与 mquickjs **均可构建、可切换**

### 非目标（首版不做）

- 将 YUI UI 迁移为 React / Vue 组件树（Web 框架不替代 Canvas 内 JSON UI）
- Vulkan 统一 GPU API（见 [GPU 方案对比](#gpu-方案对比)）
- 运行时动态加载 `libyui.so` 插件化 SDK（首版以静态链接为主）
- 首版自编译 Skia（POC 使用 [skia-pack](https://github.com/JetBrains/skia-pack) 预编译；稳定后再考虑 GN 自编）

---

## 总体架构

```
                         app/  （业务层，各端共用）
                    app.json / *.js / assets / fonts
                              │
                              ▼
              ┌───────────────────────────────┐
              │   platform/common/yui_boot.c   │  ← 从 main.c 抽取的启动逻辑
              │   yui_init / yui_tick / ...    │
              └───────────────┬───────────────┘
                              │
         ┌────────────────────┼────────────────────┐
         ▼                    ▼                    ▼
   platform/pc          platform/android      platform/ios
   SDL / 安装包          JNI + Surface         Swift + Metal
         │                    │                    │
         └────────────────────┼────────────────────┘
                              ▼
              ┌───────────────────────────────┐
              │   libyui.a（src/ 静态库）      │
              │   Layer / render / components  │
              │   backend_* / perf / event     │
              └───────────────┬───────────────┘
                              ▼
              ┌───────────────────────────────┐
              │   第三方库（按平台链接）         │
              │   cjson / quickjs / skia / ... │
              └───────────────────────────────┘

   platform/web/vanilla|react|vue|angular
         │
         ▼
   yui.wasm + Emscripten glue  →  同一套 src/ + app/
```

### 与现有后端的关系

| 后端 | 状态 | 位置 |
|------|------|------|
| SDL | 已实现 | `src/backend/backend_sdl.c` |
| LVGL | 设计/部分实现 | 见 [lvgl-backend-design.md](lvgl-backend-design.md) |
| STM32 | 草案 | `src/backend/backend_stm32.c` |
| **Mobile (Skia)** | **本设计新增** | `src/backend/backend_mobile.c` |

`platform/` 不实现渲染，只调用 `backend.h` 公开 API 和 `yui_boot` 入口。

---

## 三层职责划分

| 层级 | 路径 | 职责 | 产物 |
|------|------|------|------|
| **引擎** | `src/`、`lib/` | UI 核心、组件、渲染管线、JS 绑定 | `libyui.a`、`libcjson.a`、`libquickjs.a` 等 |
| **业务** | `app/` | JSON 界面、JS 逻辑、字体图片 | 资源文件（非库） |
| **宿主** | `platform/*` | 窗口/Surface、触摸/键盘、资源路径、打包 | `.exe`、`.apk`、`.app`、静态网站 |

**原则**：`platform/` 代码应保持「薄」——每个平台通常只有桥接层（JNI / ObjC++ / HTML 加载器）+ 打包配置，不含 UI 业务逻辑。

---

## 目录结构

```
yui/
├── src/
│   ├── ya.py
│   ├── backend/
│   │   ├── backend_common.c
│   │   ├── backend_sdl.c          # 现有桌面 / Web
│   │   └── backend_mobile.c       # 新增：Skia 移动端
│   └── ...
├── app/                           # 业务（各端共用）
│   ├── app.json
│   ├── tests/login.json
│   └── ...
├── platform/
│   ├── common/
│   │   ├── yui_boot.h
│   │   └── yui_boot.c             # 从 app/main.c 抽取
│   ├── pc/
│   │   ├── common/
│   │   ├── win/
│   │   ├── mac/
│   │   └── linux/
│   ├── android/
│   │   ├── app/                   # Gradle 工程
│   │   └── jni/
│   │       ├── CMakeLists.txt
│   │       └── yui_bridge.cpp
│   ├── ios/
│   │   ├── YuiApp/                # Xcode 工程
│   │   └── YuiBridge.mm
│   └── web/
│       ├── vanilla/               # 纯 HTML + WASM（默认）
│       ├── react/                 # 可选：React Canvas 宿主
│       ├── vue/                   # 可选
│       └── angular/               # 可选
└── third_party/
    ├── skia-pack/                 # JetBrains/skia-pack 预编译（POC 首选）
    │   ├── android/arm64-v8a/
    │   ├── android/armeabi-v7a/
    │   └── ios/                   # Skia.xcframework（真机 + 模拟器）
    └── yui-prebuilt/              # ya 交叉编译产物汇总
        ├── include/
        └── lib/
            ├── android/arm64-v8a/
            ├── android/armeabi-v7a/
            └── ios/arm64/
```

---

## 统一启动 API

从 `app/main.c` 抽取公共流程，供各 platform 调用。桌面可继续用 `backend_run()` 阻塞循环；移动端和 Web 使用逐帧 `yui_tick()`。

### 头文件：`platform/common/yui_boot.h`

```c
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* 初始化：解析 JSON、创建 Layer 树、加载字体/纹理、layout。
 * json_path  - UI 描述文件绝对或相对路径
 * assets_dir - 资源根目录（字体、图片）
 * 返回 0 成功，负值失败 */
int yui_init(const char* json_path, const char* assets_dir);

/* 窗口/Surface 尺寸变化 */
void yui_resize(int width, int height);

/* 每帧调用：处理 update 回调 → render → present */
void yui_tick(void);

/* 触摸事件（phase: 0=down, 1=move, 2=up, 3=cancel） */
void yui_on_touch(int pointer_id, int x, int y, int phase);

/* 释放资源 */
void yui_shutdown(void);

/* 获取根图层（供调试或平台扩展） */
struct Layer* yui_get_root(void);

#ifdef __cplusplus
}
#endif
```

### 与 `app/main.c` 的对应关系

| main.c 步骤 | yui_boot 封装 |
|-------------|---------------|
| `backend_init()` | `yui_init` 内部（platform 须先设置 Surface/MetalLayer） |
| `popup_manager_init()` | `yui_init` 内部 |
| `parse_yaml_json_file` + `layer_create_from_json` | `yui_init` 内部 |
| `load_all_fonts` / `load_textures` / `layout_layer` | `yui_init` 内部 |
| `backend_run(ui_root)` | 桌面：`backend_run`；移动/Web：`yui_tick` 循环 |
| `popup_manager_cleanup` + `backend_quit` | `yui_shutdown` |

### 主循环模式

| Platform | 驱动方式 | 说明 |
|----------|----------|------|
| PC (SDL) | `backend_run()` 内 `while` | 可暂不改为 tick，或逐步统一 |
| Android | `Choreographer` → `nativeTick()` | 不可阻塞 UI 线程 |
| iOS | `CADisplayLink` → `YuiTick()` | 同上 |
| Web (Emscripten) | `emscripten_set_main_loop` 或 `requestAnimationFrame` | 已有 `backend_main_loop` 可参考 |

---

## libyui 构建与交付

### 现有构建

`src/ya.py` 已将 `yui` 设为静态库：

```python
target('yui')
set_kind('static')
```

桌面 `app/ya.py` 的 `main` / `playground` 等为 `binary`，链接 `yui`。

### 移动端交叉编译（计划新增）

```bash
# Android（首版双 ABI）
ya -p android -a arm64-v8a -m release
ya -p android -a armeabi-v7a -m release
# → build/android/{arm64-v8a,armeabi-v7a}/release/libyui.a

# Android + QuickJS
ya -p android -a arm64-v8a -m release
# target: yui-mobile-quickjs → libyui.a + libquickjs.a + libjsmodule-quickjs.a

# Android + mquickjs
ya -p android -a arm64-v8a -m release
# target: yui-mobile-mqjs → libyui.a + libmquickjs.a + libjsmodule.a

# iOS
ya -p ios -a arm64 -m release
# → build/ios/arm64/release/libyui.a

# Web（已有）
ya -p em -m release
# → build/emscripten/.../playground.wasm + .js
```

### JS 引擎切换（与桌面一致）

移动端提供**两套可链接产物**，构建时选择，运行时不再动态切换引擎：

| 构建 target | JS 引擎 | 依赖库 |
|-------------|---------|--------|
| `yui-mobile-quickjs` | QuickJS | `libquickjs.a` + `libjsmodule-quickjs.a` |
| `yui-mobile-mqjs` | mquickjs | `libmquickjs.a` + `libjsmodule.a` |

`platform/android` Gradle 通过 `productFlavor` 或 CMake 变量选择链接哪一套；与桌面 `playground` / `playground-mqjs` 命名对齐。

### 交付物清单

每个平台需要链接的静态库（示例）：

| 库 | 说明 |
|----|------|
| `libyui.a` | YUI 引擎 |
| `libcjson.a` | JSON 解析 |
| `libquickjs.a` + `libjsmodule-quickjs.a` | QuickJS 变体 |
| `libmquickjs.a` + `libjsmodule.a` | mquickjs 变体 |
| `libskia.a`（skia-pack 预编译） | 移动端 GPU 绘制 |
| 系统库 | `libc++`、`libm`、`libz`；iOS 另加 Metal/UIKit 等 |

**注意**：静态库不会自动传递依赖，platform 工程必须显式链接全部 `.a`，并注意链接顺序。

### 预编译分发目录（可选）

为方便 platform 工程引用，可将构建产物整理到 `third_party/yui-prebuilt/`：

```
third_party/yui-prebuilt/
  include/          # src/*.h, cJSON.h 等
  lib/
    android/arm64-v8a/*.a
    android/armeabi-v7a/*.a
    ios/arm64/*.a
```

---

## platform 如何消费 libyui

`libyui.a` 是**编译期静态链接**，不是运行时动态加载（Android 上也可全静态链入 `libyui_jni.so`）。

### 集成三步

1. **配置路径**：Header Search Paths + Library Search Paths  
2. **编写桥接**：`yui_bridge.cpp`（JNI）或 `YuiBridge.mm`（ObjC++）+ `yui_boot.c`  
3. **链接所有 `.a`** → 产出 APK / .app / .exe  

### Android（CMake 概念）

```cmake
add_library(yui STATIC IMPORTED)
set_target_properties(yui PROPERTIES
    IMPORTED_LOCATION ${YUI_ROOT}/lib/android/${ANDROID_ABI}/libyui.a)

add_library(yui_jni SHARED yui_bridge.cpp)
target_link_libraries(yui_jni yui cjson quickjs jsmodule skia android log EGL GLESv3)
```

Kotlin 侧 `System.loadLibrary("yui_jni")`，通过 JNI 调用 `yui_init` / `yui_tick`。

### iOS（Xcode）

- Library Search Paths → `libyui.a` 所在目录  
- Link Binary With Libraries → `libyui.a`、Skia、系统 Framework  
- `YuiBridge.mm` 在 `backend_init` 前设置 `CAMetalLayer`  

### 数据流

```
ya 编译 → libyui.a (+ 依赖 .a)
              ↓
platform 工程链接 + yui_boot.c + 薄桥
              ↓
运行时：创建 Surface → yui_init(json, assets) → 每帧 yui_tick() → yui_shutdown()
```

---

## 移动端 Backend（Skia）

新增 **单文件** `src/backend/backend_mobile.c`，实现 `backend.h` 全部绘制与平台接口，内部使用 **Skia `SkCanvas`**。Android（EGL/GLES）与 iOS（Metal）的差异通过 `#if defined(__ANDROID__)` / `TARGET_OS_IPHONE` 等同文件内分支处理，**不拆** `backend_mobile_android.cpp` 等独立文件。

### 平台 GPU 选型

| 平台 | GPU API | Surface |
|------|---------|---------|
| Android | OpenGL ES + EGL | `ANativeWindow` |
| iOS | Metal | `CAMetalLayer` |

### Skia 来源（已确认：首版 skia-pack）

POC 阶段使用 **[JetBrains/skia-pack](https://github.com/JetBrains/skia-pack)** 预编译产物，放入 `third_party/skia-pack/`：

| 平台 | skia-pack 产物 | 放入路径 |
|------|----------------|----------|
| Android arm64-v8a | `libskia.a` | `third_party/skia-pack/android/arm64-v8a/` |
| Android armeabi-v7a | `libskia.a` | `third_party/skia-pack/android/armeabi-v7a/` |
| iOS device | `libskia.a` 或 xcframework | `third_party/skia-pack/ios/` |

稳定跑通 `login.json` 后，再评估 GN 自编以进一步裁剪体积。

<details>
<summary>备选：GN 自编译参数（后期）</summary>

**Android：**

```gn
target_os = "android"
target_cpu = "arm64"   # 或 "arm" 对应 armeabi-v7a
ndk_api = 24
skia_use_gl = true
is_official_build = true
```

**iOS：**

```gn
target_os = "ios"
target_cpu = "arm64"
skia_use_metal = true
is_official_build = true
```

</details>

### backend_mobile 需在 platform 初始化前收到的信息

| 参数 | Android | iOS |
|------|---------|-----|
| 绘图表面 | `ANativeWindow*` | `CAMetalLayer*` |
| 尺寸 | Surface 宽高 | layer.bounds × scale |
| DPI | `densityDpi / 160` | `UIScreen.main.scale` |
| 资源根路径 | `assets` 或 filesDir | `Bundle.main.resourcePath` |

### src/ya.py 扩展（计划）

```python
elif get_plat() == "android":
    add_files("backend/backend_mobile.c")
    add_cflags("-DYUI_BACKEND_MOBILE")
    # Skia 头文件/库路径 → third_party/skia-pack/android/${ANDROID_ABI}
elif get_plat() == "ios":
    add_files("backend/backend_mobile.c")
    add_cflags("-DYUI_BACKEND_MOBILE")
    # Skia → third_party/skia-pack/ios
```

JS 变体由 `app/ya.py` 或独立 target 控制 `add_deps("quickjs","jsmodule-quickjs")` 与 `add_deps("mquickjs","jsmodule")` 二选一。

---

## 各 Platform 设计

### platform/pc（与移动端并行）

**现状**：`app/main.c` + SDL 已满足日常开发。

**首版策略**：`platform/pc` 与 Android/iOS **并行推进**——在抽取 `yui_boot.c` 时同步整理 PC 打包，不等到移动端 POC 完成再做。

**platform/pc 价值**：安装包（NSIS / DMG）、系统托盘、原生文件对话框、与移动端统一的 `yui_boot` API。

```
platform/pc/
  common/     yui_boot.c（与 mobile / web 共用）
  win/        WinMain 包装、.rc 资源
  mac/        Info.plist、.app 结构
  linux/      .desktop、AppImage 脚本
```

现有 `app/main` 目标保持可用；`platform/pc` 作为统一入口的增量路径，不强制一次性迁移。

---

### platform/android

```
platform/android/
  app/
    build.gradle
    src/main/
      java/com/yui/YuiActivity.kt
      java/com/yui/YuiView.kt          # SurfaceView / TextureView
      assets/  → 软链或拷贝 app/ 资源
      cpp/
        CMakeLists.txt
        yui_bridge.cpp
  README.md
```

**职责：**

- `Surface` / `SurfaceHolder` 生命周期
- JNI：`nativeInit(surface, assetsPath)`、`nativeTick()`、`nativeTouch(...)`
- `Choreographer` 驱动帧循环
- `InputConnection` → `backend_start_text_input` / IME
- APK 打包：**Universal APK**（`arm64-v8a` + `armeabi-v7a` 双 `.so` 同包）
- `minSdkVersion 21`

---

### platform/ios

```
platform/ios/
  YuiApp/
    YuiApp.swift
    YuiViewController.swift
    YuiBridge.mm
    Info.plist
  YuiApp.xcodeproj
  README.md
```

**职责：**

- `CAMetalLayer` + `CADisplayLink`
- **真机（iphoneos arm64）与模拟器（iossimulator arm64/x64）均需可运行**；Skia / `libyui.a` 通过 **XCFramework** 或分目录链入
- Safe Area insets 传给 layout
- 键盘弹出时 `backend_set_text_input_rect`
- `UITextInput` 协议对接 IME
- `applicationWillResignActive` 暂停帧循环

---

### platform/web

YUI Web 端 = **Emscripten 将 `src/` 编译为 WASM**，UI 仍在 **Canvas** 内绘制，不是 DOM 组件。

#### vanilla（默认，推荐首版）

**Vanilla** = 纯 HTML + CSS + 原生 JavaScript，不使用 React/Vue/Angular。

```
platform/web/vanilla/
  index.html              # <canvas> + 加载脚本
  public/
    app/                  # 拷贝或软链 app/ 业务资源
    yui/
      playground.js       # Emscripten 产出
      playground.wasm
```

与现有 `app/ya.py` 中 `playground.html` 目标一致，仅将 HTML/加载逻辑收到 `platform/web/vanilla/`。

#### react / vue / angular（可选）

前端框架 **只作 Canvas 宿主**，不替代 YUI JSON UI：

| 框架职责 | 不做 |
|----------|------|
| 加载 WASM | 用 React 重写 Button、Form |
| 提供 `<YuiCanvas />` 组件 | 将 Layer 树映射为 DOM |
| 页面级路由、登录外壳（HTML） | 替代 `render.c` |
| 对接浏览器 `fetch` / `localStorage` | |

```
platform/web/react/
  src/components/YuiCanvas.tsx
  package.json
```

**一个团队选一个框架即可**，不必三套都做。

---

## GPU 方案对比

| 方案 | Android | iOS | 推荐度 |
|------|---------|-----|--------|
| **Skia GLES + Metal** | GLES/EGL | 原生 Metal | ✅ 首选 |
| Skia Vulkan + MoltenVK | Vulkan | MoltenVK → Metal | ⚠️ iOS 多一层翻译，Skia 主推 Metal |
| 自研 Vulkan 画 UI | 工作量大 | 需 MoltenVK | ❌ 不推荐 |

若未来需要 Android + Linux 桌面共用 Vulkan 设备管理，可再评估 Android 侧 `skia_use_vulkan`；iOS 仍建议 Metal backend。

---

## 构建系统

### ya 平台矩阵（目标）

| `get_plat()` | backend | JS 变体 | platform 产物 |
|--------------|---------|---------|---------------|
| 默认 / `sdl` | `backend_sdl.c` | quickjs / mqjs（现有 target） | `app/main` 可执行文件 |
| `em` / `emscripten` | `backend_sdl.c` | quickjs | WASM + JS |
| `android` | `backend_mobile.c` | quickjs **或** mqjs（构建时选） | `libyui.a` × 2 ABI → APK |
| `ios` | `backend_mobile.c` | quickjs **或** mqjs | `libyui.a` → .app |
| `lvgl` | `backend_lvgl.c` | — | 见 LVGL 设计文档 |

### platform 工程不负责编译 YUI 核心

- **YUI 库**：`ya` 在仓库根目录交叉编译  
- **platform 工程**：Gradle / Xcode / npm（web）只负责链库、打包、签名  

---

## 资源与路径约定

| 端 | 资源位置 | 传入 `yui_init` 的 `assets_dir` |
|----|----------|----------------------------------|
| PC | 工作目录相对路径 | `"assets"` 或 CLI 参数 |
| Android | `assets/` 或解压到 `filesDir` | `context.filesDir` 绝对路径 |
| iOS | App Bundle | `Bundle.main.resourcePath` |
| Web | `public/app/` | `"/app"` 或 Emscripten 虚拟 FS 路径 |

**JSON 路径**示例：`{assets_dir}/tests/login.json` 或独立传入 `json_path`。

字体、图片路径在 JSON 中保持相对 `assets` 的写法，与桌面一致。

---

## 分阶段实施计划

### P0 — 设计与骨架（当前阶段）

- [x] 本文档评审
- [x] 关键决策确认（见 [已确认决策](#已确认决策)）
- [x] 创建 `platform/common/yui_boot.h` + `yui_boot.c`
- [x] 创建 `platform/` 目录 README
- [x] `third_party/skia-pack/README.md` 占位说明
- [ ] 拉取 skia-pack 预编译至 `third_party/skia-pack/`（需手动下载）

### P1 — 移动端 + PC 并行 POC

**Android**

- [x] 单文件 `backend_mobile.c` 桩实现（Skia TODO）
- [ ] `ya -p android` 交叉编译 `libyui.a`（`arm64-v8a` + `armeabi-v7a`）
- [ ] QuickJS / mquickjs 两套 target 可链
- [x] `platform/android/jni/yui_bridge.cpp` 桩
- [ ] `platform/android` Gradle：显示 `login.json` 一屏

**PC（并行）**

- [x] `platform/common/yui_boot.c` 从 `main.c` 抽取首版
- [x] `platform/pc/main.c` + `ya -b yui-pc` target

### P2 — iOS + 输入

- [ ] skia-pack iOS + `platform/ios`
- [ ] 触摸 → `handle_touch_event`
- [ ] 软键盘 / `backend_set_text_input_rect`

### P3 — 统一与 Web 整理

- [ ] `yui_boot.c` 完善（与 PC / 移动共用）
- [ ] `platform/web/vanilla` 收拢现有 Emscripten HTML
- [ ] 移动端验证 `app/tests/test-perf.js`（quickjs / mqjs 各跑一遍）

### P4 — 可选增强

- [ ] `platform/pc` 安装包（NSIS / DMG / AppImage）
- [ ] `platform/web/react` 宿主组件
- [ ] `Yui.xcframework` / Android AAR 分发
- [ ] Skia GN 自编替换 skia-pack（体积裁剪）

---

## 风险与对策

| 风险 | 对策 |
|------|------|
| Skia 体积大（10MB+ 级） | `is_official_build`、关闭 PDF/Skottie/SVG；Release strip |
| 静态库链接遗漏 / 顺序错误 | `third_party/yui-prebuilt` 统一清单；CMake `IMPORTED` 模板 |
| `backend_run` 阻塞不适合移动 | 统一 `yui_tick` + 参考 Emscripten `backend_main_loop` |
| iOS 模拟器 vs 真机 Skia 双份 | skia-pack 拉 **XCFramework**；`ya -p ios` 分别编 device / simulator 后合并 |
| Web 框架与 Canvas 职责混淆 | 文档明确：框架只包宿主，UI 仍在 WASM |
| 跨盘符 `ya` 构建失败（Windows） | 文档注明工作目录需与仓库同盘符 |

---

## Android 双 ABI 打包说明

Android 上 Native 代码编译成 **`.so` 动态库**，按 CPU 架构分目录存放。你们首版有两套架构：

| ABI | 典型设备 | 目录 |
|-----|----------|------|
| `arm64-v8a` | 近年手机（64 位 ARM） | `lib/arm64-v8a/libyui_jni.so` |
| `armeabi-v7a` | 较老 32 位 ARM 手机 | `lib/armeabi-v7a/libyui_jni.so` |

安装时系统**只会加载与 CPU 匹配的那一个** `.so`，不会两套同时跑。

### 方式 A：单 APK（Universal / Fat APK）

**一个 APK 里同时打包两套 `.so`。**

```text
app-release.apk
├── lib/
│   ├── arm64-v8a/
│   │   └── libyui_jni.so    ← 约 15–25 MB（含 Skia）
│   └── armeabi-v7a/
│       └── libyui_jni.so    ← 约 12–20 MB
├── classes.dex
└── assets/ ...
```

| 优点 | 缺点 |
|------|------|
| 构建、 sideload、内测分发**最简单**（一个文件到处装） | **下载体积 ≈ 两套 native 之和**（Skia 大时很明显） |
| 不依赖 Google Play 拆包 | 上架 Play 时仍建议再转 AAB（见方式 B） |

Gradle 默认即是如此（`splits.abi.enable = false`）。

### 方式 B：ABI Split / App Bundle（AAB）

**按架构拆成多个 APK，或上传 AAB 由 Play 商店按机型下发。**

```text
# 本地 abi split 产出示例
app-arm64-v8a-release.apk      ← 只有 arm64 的 .so，体积小
app-armeabi-v7a-release.apk    ← 只有 v7a 的 .so

# 或上传 Google Play
app-release.aab                ← 商店为用户生成「只含一种 ABI」的 APK
```

| 优点 | 缺点 |
|------|------|
| 用户**只下载自己 CPU 对应的一份** native 库 | 本地要多编/多产物，或必须走 AAB |
| 适合正式上架 Play | 直接发多个 APK 内测稍麻烦 |

Gradle 示例：

```groovy
android {
    bundle {
        abi { enableSplit = true }
    }
    // 或 APK splits:
    splits {
        abi {
            enable true
            universalApk false  // true 则额外再打一个 fat APK
        }
    }
}
```

### 和 iOS 的对比（帮助理解）

| Android | iOS |
|---------|-----|
| 单 APK 可塞多个 ABI 的 `.so` | 真机 arm64 与模拟器 x64/arm64 **不能**塞进同一个二进制 |
| Play 可用 AAB 自动拆 | 用 **XCFramework** 把 device + simulator 包在一起给 Xcode 选 |

### 建议（YUI 首版）— 已确认

| 场景 | 决策 |
|------|------|
| 开发、内测、sideload、分发 | **Universal APK（单 APK 双 .so）** ✅ |
| Google Play 上架（若需要） | 后续可再开 AAB + abi split，不阻塞首版 |

Gradle 保持 `splits.abi.enable = false`（默认），`ndk.abiFilters "arm64-v8a", "armeabi-v7a"`。

---

## 已确认决策

| 项 | 决策 | 说明 |
|----|------|------|
| **移动端 JS** | QuickJS **与** mquickjs **均支持** | 与桌面一致，**构建时**选择链接哪套（`yui-mobile-quickjs` / `yui-mobile-mqjs`），非运行时切换 |
| **Android ABI** | `arm64-v8a` **+** `armeabi-v7a` | 两套 native 库打进同一个 APK |
| **Android 发包** | **Universal APK（单 APK 双 .so）** | 一个 APK 含两套 ABI；`splits.abi.enable = false` |
| **Android 最低 API** | **21**（Android 5.0） | `minSdkVersion 21`；Skia GLES 在 API 21+ 可用 |
| **iOS 目标** | **真机 + 模拟器** | skia-pack / `libyui.a` 需含 device + simulator slice（推荐 `xcframework`） |
| **platform/pc** | **与移动端并行** | P1 即开始 `yui_boot` + `platform/pc` 骨架，安装包细化放 P4 |
| **Skia 来源** | **skia-pack 预编译**（POC） | `third_party/skia-pack/`；GN 自编延后至 P4 |
| **backend_mobile** | **单文件** | 仅 `backend_mobile.c`，平台差异用 `#if` 分支，不拆多文件 |

---

## 相关文档

- [架构设计文档](architecture.md)
- [LVGL Backend 设计](lvgl-backend-design.md)
- [C API 参考](c-api-reference.md)
- [性能监控 perf.md](perf.md)
- [开发者指南](developer-guide.md)
