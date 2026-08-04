# YUI ESP32 宿主壳

ESP32 固件工程模板：入口 `main.c` + ESP32 原生后端，链接 ya 编译的 yui 核心库。不依赖 LVGL。

## 构建

```bash
# 1. 编译 yui 核心库（产物 build/esp32/esp32c3/None/libyui.a 等）
cd e:/workspace/yui
ya -p esp32 -a esp32c3        # C3（RISC-V）；esp32/esp32s3 用 -a esp32 / -a esp32s3

# 2. 编译固件
cd platform/esp32
idf.py set-target esp32c3
idf.py build

# 3. 烧录 + 监控
idf.py -p COM3 flash monitor
```

> `main/CMakeLists.txt` 里 `YUI_BUILD_DIR` 指向 `build/esp32/esp32c3/None`，换芯片时同步改 arch 目录。

## 板级配置

`main/main.c` 中按实际硬件调用：

| API | 参数 | 默认 |
|-----|------|------|
| `backend_esp32_set_config(w,h,spi_host,cs,dc,rst,bl,freq)` | 分辨率/SPI/引脚 | 240x240, **SPI2_HOST(1)**, cs=7, dc=2（C3） |
| `backend_esp32_set_spi_pins(mosi,sclk)` | SPI 总线 | mosi=6, sclk=4（C3；勿用 GPIO>21） |
| `backend_esp32_set_touch(i2c_host,sda,scl,addr,int_pin)` | 触摸 | I2C0, sda=8, scl=9, addr=0x15（与 LCD CS 错开） |

> ESP32-C3 只有 `SPI2_HOST`（值为 1）。旧默认 `spi_host=2` / `MOSI=23` 会报 `invalid host_id`。

当前内置驱动：ST7789（SPI）、CST816S（I2C）。其他屏/触摸需替换 `backend_esp32.c` 中的 `esp32_lcd_init` / `esp32_touch_init`。

## 字体（Flash 映射，零 RAM）

1. 子集化字体：`python3 scripts/subset_font.py --input app/assets/Roboto-Regular.ttf --output build/font-subset.ttf --scan app/`
2. 烧录到 `font` 分区（partitions.csv 已定义，偏移 `0x110000` = factory 1M 之后）：
   ```bash
   esptool.py --port COM3 --baud 921600 write_flash 0x110000 build/font-subset.ttf
   ```
3. `main.c` 通过 `backend_esp32_load_font_from_flash("font", 16)` 加载。

## 注意事项

- 复杂 box-shadow 模糊在 C3 上较慢，建议用 `radius` 圆角替代。
- QuickJS 在 C3 上内存紧张（~200KB+），建议 C3 用纯 JSON 配置；ESP32（含 PSRAM）可启用 JS。
