# YUI STM32 宿主壳

STM32 宿主壳入口（F7 系列，LTDC + DMA2D + SDRAM）。后端 `backend_stm32.c` 已由 ya 编译进 `libyui.a`，此处仅需入口 `main.c` 与 HAL/BSP 代码。

## 构建分工

| 层 | 构建工具 | 产物 |
|----|---------|------|
| yui 核心库（含 backend_stm32.c + backend_embed_font.c） | **ya** | `build/stm32/None/libyui.a` |
| HAL/BSP（CubeMX 生成：ltdc/dma2d/sdram/touch + SystemClock_Config） | 你的 STM32 工程 | 用户提供 |
| 宿主壳入口（本目录 main.c） | 你的 STM32 工程 或 `ya -p stm32 -b yui-stm32` | firmware |

## 集成步骤（CubeIDE / Keil）

1. 用 **STM32CubeMX** 生成工程：使能 **LTDC**、**DMA2D**、**SDRAM**、**触摸**（touch.c 提供 `touch_init/touch_update/touch_get_position`），生成 `SystemClock_Config()` 与外设初始化。
2. 编译 yui 核心库：
   ```bash
   ya -p stm32
   ```
3. 把本目录 `main.c` 加入工程，链接：
   - `build/stm32/None/libyui.a`、`libcjson.a`、`libtsm.a`
   - STM32 HAL 库（`stm32f7xx_hal_ltdc/dma2d/sdram` 等）+ BSP（ltdc.c、dma2d.c、sdram.c、touch.c）

`main.c` 只做：HAL 初始化 → `backend_init()`（framebuffer + 触摸）→ 加载字体 → 构建 UI（JSON）→ `backend_run()` 主循环。

## 依赖说明

`backend_stm32.c` 引用的外部符号（由你的 BSP 提供）：

| 符号 | 来源 |
|------|------|
| `hltdc` / `hdma2d` | CubeMX 生成的 LTDC/DMA2D 句柄 |
| `touch_init` / `touch_update` / `touch_get_position` | touch.c |
| `SDRAM_BANK_ADDR` | sdram.c / 链接脚本 |
| `LCD_WIDTH` / `LCD_HEIGHT` | 平台宏（backend_stm32.c 内使用） |

## 试构建（可选）

```bash
ya -p stm32 -b yui-stm32
```

`platform/ya.py` 中 `yui-stm32` target 只编译本 main.c，HAL/BSP 库需在你的工程 include/link 路径中提供。
