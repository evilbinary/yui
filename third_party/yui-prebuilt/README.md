# yui-prebuilt（Android）

`ya -p android` 构建完成后，通过 **`after_build` hook** 自动把 `.a` 拷到本目录。

## 一键构建（QuickJS）

```bash
ya -p android -a arm64-v8a -m release -b yui-android-prebuilt
ya -p android -a armeabi-v7a -m release -b yui-android-prebuilt
```

## mquickjs 变体

```bash
ya -p android -a arm64-v8a -m release -b yui-android-prebuilt-mqjs
ya -p android -a armeabi-v7a -m release -b yui-android-prebuilt-mqjs
```

## 产物布局

```text
third_party/yui-prebuilt/android/
  arm64-v8a/
    libyui.a
    libcjson.a
    libyaml2json.a
    libquickjs.a
    libjsmodule-quickjs.a   # 或 libjsmodule.a（mqjs）
    libsocket.a
  armeabi-v7a/
    （同上）
```

## 说明

- 需设置 `ANDROID_NDK_HOME`
- hook 定义在根目录 `ya.py`：`after_build_android_prebuilt`
- `scripts/build-android-libs.ps1` 仅为上述 `ya` 命令的薄包装（可选）
