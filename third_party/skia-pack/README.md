# skia-pack 预编译库（POC）

首版使用 [JetBrains/skia-pack](https://github.com/JetBrains/skia-pack) 预编译 Skia，避免 GN 自编。

## 目录布局（拉取后）

```text
third_party/skia-pack/
  android/arm64-v8a/     libskia.a + include/
  android/armeabi-v7a/   libskia.a + include/
  ios/                   Skia.xcframework 或 libskia.a（device + simulator）
```

## 获取

按 skia-pack 仓库 Release / 构建说明下载对应平台产物，解压到上述路径。

稳定跑通 POC 后再考虑 GN 自编裁剪体积（见 `docs/platform-design.md` P4）。
