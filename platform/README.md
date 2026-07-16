# platform/

YUI 各端**宿主壳**目录。引擎在 `src/`（`libyui.a`），业务在 `app/`，此处只负责启动、输入、打包。

| 目录 | 说明 | 状态 |
|------|------|------|
| [common/](common/) | `yui_boot.h` / `yui_boot.c` 统一启动 API | 骨架 |
| [pc/](pc/) | 桌面壳，目标 `yui-pc` | 骨架 |
| [android/](android/) | Gradle + JNI | 占位 |
| [ios/](ios/) | Xcode + Metal 桥接 | 占位 |
| [web/](web/) | WASM 宿主（vanilla 等） | 待整理 |

设计文档：[docs/platform-design.md](../docs/platform-design.md)

## 构建

```bash
# PC 壳（SDL 后端，验证 yui_boot）
ya -b yui-pc

# 移动端 libyui（需 NDK / Xcode 工具链，待完善）
ya -p android -a arm64-v8a -m release
ya -p android -a armeabi-v7a -m release
ya -p ios -a arm64 -m release
```
