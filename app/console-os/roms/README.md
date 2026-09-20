# ROM 目录

把 ROM 文件按模拟器放到对应子目录，桌面与游戏库会在下次扫描时自动收录：

```
roms/
├── nes/        →  emulators/nes/nes.json    ("romDir": "roms/nes")
├── snes/
├── gb/
├── gba/
├── md/
├── n64/
├── arcade/
├── ps1/
├── psp/
├── nds/
├── saturn/
└── dos/
```

规则：

- 目录名 = `emulators/<id>/<id>.json` 里的 `romDir` 字段，可自定义。
- 支持的后缀写在模拟器定义的 `exts` 里（如 `["nes","fds","unf"]`）。
- 不支持的后缀也会列出，只是标记为本地文件。
- 目录为空时，界面展示模拟器定义里内置的演示游戏条目（`roms` 数组），
  用于在未放入 ROM 时预览界面。

新增一个模拟器：在 `emulators/` 下建目录并放入 `<id>.json`，桌面图标会自动出现。
