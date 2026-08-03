# YUI ESP32 原生后端使用指南

> 状态：已实现（基础版）
> 适用：ESP32 / ESP32-C3 / ESP32-S3
> 特点：不依赖 LVGL，合并 LCD + 触摸 + 软件渲染，ya 构建

## 目录

- [架构概述](#架构概述)
- [文件结构](#文件结构)
- [构建方式](#构建方式)
- [字体方案](#字体方案)
- [配置 API](#配置-api)
- [分区表与烧录](#分区表与烧录)
- [示例应用](#示例应用)
- [限制与注意事项](#限制与注意事项)

---

## 架构概述

ESP32 后端不引入 LVGL，直接操作 RGB565 framebuffer，通过 ESP-IDF 的 `esp_lcd` 组件驱动 SPI LCD，`esp_lcd_touch` 驱动 I2C 触摸。渲染由 CPU 软件完成（fill_rect / line / 文本 blit / 圆角等），脏区域局部刷新。

```
应用层 (main.c)
    │
    ├── backend_esp32_set_config()   配置板级引脚
    ├── backend_init()               初始化 LCD + 触摸 + framebuffer
    ├── backend_load_font()          加载字体（RAM）/ backend_esp32_load_font_from_flash()（Flash 映射）
    └── backend_run(ui_root)         主循环：触摸轮询 → 渲染 → 刷新脏区域
```

类型系统复用 `YUI_BACKEND_MOBILE`（`YuiFont{size,priv}` / `YuiTexture{w,h,priv}`），不依赖 SDL。

---

## 文件结构

| 文件 | 说明 |
|------|------|
| `src/backend/backend_esp32.c` | ESP32 后端：LCD 初始化、触摸轮询、RGB565 软件渲染、主循环 |
| `src/backend/backend_embed_font.c` | 通用字体模块（ESP32/STM32 共用）：stb_truetype + LRU 纹理缓存 |
| `src/backend/backend_embed_font.h` | 通用字体接口 |
| `scripts/subset_font.py` | 字体子集化工具 |
| `src/ya.py` | yui 库构建配置，`esp32` 平台分支 |
| `ya.py` | 顶层构建配置，ESP32 工具链与编译选项 |

---

## 构建方式

### 环境准备

1. 安装 ESP-IDF（推荐 v5.x），激活环境变量（`export.bat` 或 `idf.py`）：
   ```
   set IDF_PATH=E:\soft\Espressif\framework\esp-idf-v5.5.5
   set ESP_IDF_TOOLS_PATH=E:\soft\Espressif\tools
   ```
   ya.py 会自动搜索 `E:\soft\Espressif` 下的工具链与 `IDF_PATH`。

2. 安装 fonttools（字体子集化用）：
   ```
   pip install fonttools
   ```

### 编译

#### 分工说明

ESP-IDF 的组件 include 路径由 CMake/`idf.py` 生成（如 lwip 的 `#include_next`），ya 无法完整覆盖。因此构建分工如下：

| 层 | 构建工具 | 产物 |
|----|---------|------|
| yui 核心库（layer/render/components + backend_embed_font） | **ya** | `yui.a` |
| ESP32 后端（backend_esp32.c，依赖 esp_lcd/lwip/freertos） | **idf.py** | `backend_esp32.o` |
| 应用固件（app + 链接 yui.a + backend_esp32.o） | **idf.py** | `firmware.bin` |

#### 步骤 1：ya 编译 yui 核心库

```bash
# ESP32-C3（RISC-V）
ya -p esp32 -a esp32c3

# ESP32（Xtensa）
ya -p esp32 -a esp32

# ESP32-S3（Xtensa）
ya -p esp32 -a esp32s3
```

ya.py 自动识别架构：
- `esp32c*` / `esp32h*` → RISC-V（`-march=rv32imc -mabi=ilp32`）
- `esp32` / `esp32s2` / `esp32s3` → Xtensa（`-mlongcalls`）

产物：`build/esp32/esp32c3/yui.a`（含 yui 核心组件 + 通用字体，**不含** backend_esp32.c）。

#### 步骤 2：ESP-IDF 工程编译后端 + 固件

在 ESP-IDF 工程中（`app/esp32/`），`CMakeLists.txt` 编译 `backend_esp32.c` 并链接 ya 产出的 `yui.a`：

```cmake
# app/esp32/main/CMakeLists.txt
idf_component_register(
    SRCS "main.c" "../../../src/backend/backend_esp32.c"
    INCLUDE_DIRS "../../../src" "../../../src/backend" "../../../lib/stb"
    REQUIRES esp_lcd esp_lcd_touch esp_timer driver nvs_flash
)

# 链接 ya 编译的 yui.a
target_link_libraries(${COMPONENT_LIB} PRIVATE
    ${CMAKE_SOURCE_DIR}/../../../build/esp32/esp32c3/yui.a)
```

```bash
cd app/esp32
idf.py set-target esp32c3
idf.py build
idf.py -p COM3 flash monitor
```

---

## 字体方案

ESP32 RAM 有限（ESP32-C3 仅 400KB SRAM），直接 `fopen` 加载完整中文字体（5-15MB）不可行。采用 **Flash 映射 + 子集化 + 纹理缓存** 三层方案。

### 1. 字体子集化

从大 TTF 中提取实际用到的字符，生成几 KB~几十 KB 的小字体：

```bash
python3 scripts/subset_font.py \
    --input app/assets/Roboto-Regular.ttf \
    --output build/font-subset.ttf \
    --scan app/ \
    --extra "你好世界设置连接"
```

字符来源（取并集）：
- 扫描 `app/` 下所有 `.json` 的 `text/label/title/message/placeholder` 等字段
- 常用中文字符集（脚本内置，可按需扩充）
- `--extra` 指定的额外字符
- ASCII 可见字符（0x20-0x7E）

### 2. Flash 映射（零 RAM）

子集化后的 TTF 烧到 SPI Flash 数据分区，用 `esp_partition_mmap` 映射到地址空间，stb_truetype 直接只读访问 Flash，**不占 RAM**：

```c
#include "backend_embed_font.h"

DFont* font = backend_esp32_load_font_from_flash("font", 16);
if (!font) {
    // 回退到文件系统加载（占 RAM）
    font = backend_load_font("spiffs/font.ttf", 16);
}
```

### 3. 纹理缓存（避免每帧重算）

`embed_font_render` 内置 LRU 纹理缓存（64 槽）：
- 同一段文字（`font + text + color`）只栅格化一次
- 组件每帧 `backend_render_texture` + `backend_render_text_destroy` 模式下，命中缓存直接返回
- `backend_render_text_destroy` 仅引用计数 -1，LRU 仅淘汰 `refcount==0` 的槽

```
内存占用估算（ESP32-C3）：
  子集化 TTF（Flash，不占 RAM）: ~20-50KB
  framebuffer（RGB565 240x240） : 115KB
  纹理缓存（64 槽，每槽~1KB）   : ~64KB
  字体 stbtt_fontinfo           : ~200B
```

---

## 配置 API

应用层在 `backend_init()` 前配置板级引脚：

```c
/* LCD 配置：分辨率、SPI 主机、CS/DC/RST/BL 引脚、SPI 时钟 */
backend_esp32_set_config(240, 240,       /* width, height */
                         2,              /* spi_host: FSPI_HOST */
                         5, 16, -1, -1,  /* cs, dc, rst, bl */
                         40000000);      /* spi freq_hz */

/* SPI 总线引脚（MOSI/SCK） */
backend_esp32_set_spi_pins(23, 18);

/* 触摸配置（I2C CST816S，-1 禁用） */
backend_esp32_set_touch(0,               /* i2c_host */
                        4, 5,            /* sda, scl */
                        0x15,            /* addr */
                        6);              /* int pin */
```

默认配置（240x240 ST7789 SPI）：
| 参数 | 默认值 |
|------|--------|
| 分辨率 | 240x240 |
| SPI 主机 | FSPI_HOST(2) |
| MOSI / SCK | 23 / 18 |
| CS / DC | 5 / 16 |
| SPI 时钟 | 40MHz |
| 触摸地址 | 0x15 (CST816S) |

---

## 分区表与烧录

### 分区表 `partitions.csv`

```
nvs,      data, nvs,     0x9000,  0x6000,
phy_init, data, phy,     0xf000,  0x1000,
factory,  app,  factory, 0x10000, 1M,
font,     data, 0x40,    ,        0x80000,
```

`font` 分区（512KB）存放子集化 TTF。`0x40` 是自定义子类型。

### 烧录字体到分区

```bash
esptool.py --port COM3 --baud 921600 write_flash 0x110000 build/font-subset.ttf
```
（`0x110000` = `factory` 起始 `0x10000` + `1M`，即 `font` 分区起始地址）

---

## 示例应用

```c
#include "backend.h"
#include "layer.h"

int app_main(void) {
    /* 板级配置 */
    backend_esp32_set_config(240, 240, 2, 5, 16, -1, -1, 40000000);
    backend_esp32_set_spi_pins(23, 18);
    backend_esp32_set_touch(0, 4, 5, 0x15, 6);

    /* 初始化后端 */
    backend_init();

    /* 从 Flash 分区加载字体（零 RAM） */
    DFont* font = backend_esp32_load_font_from_flash("font", 16);

    /* 构建 UI（JSON 或代码创建 Layer 树） */
    Layer* root = layer_create_from_json_file("spiffs/ui.json", font);

    /* 主循环 */
    backend_run(root);

    backend_quit();
    return 0;
}
```

---

## 限制与注意事项

1. **LCD 驱动**：当前内置 ST7789 SPI 驱动。其他屏（ILI9341/SSD1306/GC9A01）需在 `esp32_lcd_init` 中替换 `esp_lcd_new_panel_st7789`。

2. **触摸驱动**：内置 CST816S。其他触摸芯片（FT6336/ST7123）需替换 `esp_lcd_new_touch_cst816s`（函数名按实际 ESP-IDF 版本调整）。

3. **渲染性能**：CPU 软件渲染，复杂阴影/模糊（box-shadow blur）在 ESP32-C3 上较慢。建议：
   - 避免大半径 blur 阴影
   - 用 `radius` 圆角替代阴影
   - 静态内容预渲染到 framebuffer

4. **纹理缓存大小**：默认 64 槽。若界面文字种类多导致缓存抖动，用 `embed_font_texture_cache_set_max` 调整（需改 `EMBED_TEXT_CACHE_DEFAULT` 编译时常量）。

5. **固件链接**：ya 编译 yui 为目标文件/静态库。完整固件链接（含 ESP-IDF 组件）建议在 ESP-IDF 工程中链接 `yui.a`，或后续为 ya 配置完整 ldflags。

6. **字体回退**：当前不支持 fallback 字体（emoji 等）。如需，可加载第二个子集字体，在渲染时按码点切换（参考 `mobile_text.c` 的 fallback 机制）。

7. **QuickJS**：ESP32-C3 RAM 紧张，QuickJS 内存占用较大（~200KB+）。建议 ESP32-C3 上用纯 JSON 配置（不启用 JS），或使用精简 JS 引擎。ESP32（含 PSRAM）可运行 QuickJS。
