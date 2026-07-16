# iOS platform

- **真机 + 模拟器**：Skia / `libyui` 用 XCFramework（`third_party/skia-pack/ios/`）
- **桥接**：`YuiBridge.mm` → `yui_set_native_surface` / `yui_init` / `yui_tick`

## 下一步

1. 创建 `YuiApp.xcodeproj`
2. 链 `libyui.a`、skia-pack、系统 Framework（Metal、UIKit）
3. `CADisplayLink` 驱动 `yui_tick`
