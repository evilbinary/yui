/*
 * YUI ESP32 宿主壳（示例入口）
 *
 * 构建步骤（前置：先编译 yui 核心库）：
 *   cd e:/workspace/yui
 *   ya -p esp32 -a esp32c3          # 生成 build/esp32/esp32c3/None/libyui.a 等
 *   cd platform/esp32
 *   idf.py set-target esp32c3
 *   idf.py build
 *   idf.py -p COM3 flash monitor
 *
 * 默认板级配置：ST7789 240x240 SPI + CST816S I2C 触摸（C3 常用小板）。
 * 按实际硬件用 backend_esp32_set_config / backend_esp32_set_spi_pins /
 * backend_esp32_set_touch 调整。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "backend.h"
#include "layer.h"
#include "layout.h"
#include "render.h"
#include "popup_manager.h"
#include "cJSON.h"
#include "esp_spiffs.h"
#include "esp_heap_caps.h"

/* ESP32 后端扩展接口（backend_esp32.c 定义，不在通用 backend.h 中） */
void backend_esp32_set_config(int width, int height, int spi_host,
                              int cs, int dc, int rst, int bl, int freq_hz);
void backend_esp32_set_spi_pins(int mosi, int sclk);
void backend_esp32_set_touch(int i2c_host, int sda, int scl, int addr, int int_pin);
void backend_esp32_set_hw_display(int on);
DFont* backend_esp32_load_font_from_flash(const char* partition_label, int size);

/* 触摸芯片创建钩子：覆盖 backend_esp32.c 中的弱符号，实现为 CST816S。
 * 换触摸芯片时改这里即可，backend 层无需改动。 */
#include "esp_lcd_touch.h"
#include "esp_lcd_touch_cst816s.h"
esp_lcd_touch_handle_t yui_esp32_touch_create(esp_lcd_panel_io_handle_t io,
                                              const esp_lcd_touch_config_t* cfg) {
    esp_lcd_touch_handle_t t = NULL;
    if (esp_lcd_touch_new_i2c_cst816s(io, cfg, &t) == ESP_OK) return t;
    return NULL;
}

/* 简单 UI 描述（也可改为从 Flash/SPIFFS 加载 JSON 文件） */
static const char s_ui_json[] =
    "{"
    "  \"type\": \"View\","
    "  \"text\": \"YUI ESP32\","
    "  \"layout\": \"vertical\","
    "  \"rect\": {\"x\": 0, \"y\": 0, \"w\": 240, \"h\": 240},"
    "  \"style\": {\"bgColor\": \"#102040\", \"color\": \"#ffffff\", \"fontSize\": 20},"
    "  \"children\": ["
    "    {\"type\": \"Label\", \"text\": \"Hello YUI\","
    "     \"style\": {\"color\": \"#ffd700\", \"fontSize\": 24}}"
    "  ]"
    "}";

void app_main(void) {
    cJSON* json;
    Layer* ui_root;
    DFont* font;

    printf("YUI ESP32 starting...\n");

    /* 0. 板级配置：分辨率/SPI 主机/CS/DC/RST/BL/SPI 时钟 */
    backend_esp32_set_config(240, 240, 2, 5, 16, -1, -1, 40 * 1000 * 1000);
    /* SPI 总线引脚（MOSI/SCK） */
    backend_esp32_set_spi_pins(23, 18);
#if defined(YUI_ESP32_QEMU)
    /* QEMU 无 ST7789/CST816S：跳过 SPI LCD 与 I2C 触摸。
     * QEMU 构建会自动创建虚拟 RGB 面板（esp_lcd_qemu_rgb），YUI 渲染像素
     * 直写该面板，由 -display sdl 窗口实时显示；无 --graphics 时为无头模式。 */
    backend_esp32_set_hw_display(0);
    printf("YUI: QEMU mode (virtual RGB panel)\n");
    /* 冒烟测试：渲染固定帧数后自动退出，便于无头验证完整主循环。
     * framebuffer 由编译期宏 YUI_ESP32_LCD_BUFFER 控制，默认关闭（直写像素）。 */
    // backend_set_auto_frames(100);
#else
    /* 触摸（I2C CST816S；int_pin=-1 禁用轮询中断） */
    backend_esp32_set_touch(0, 4, 5, 0x15, 6);
#endif

    /* 1. 初始化后端 + 弹层管理。
     *    必须在 SPIFFS 挂载之前：framebuffer calloc(115KB) 需要尽量干净的堆，
     *    SPIFFS 的缓存/工作缓冲会先从 132KB 主堆区切走几 KB，导致分配失败。 */
    printf("YUI: backend_init... free=%u largest=%u\n",
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT),
           (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    if (backend_init() != 0) {
        printf("YUI: backend_init failed\n");
        return;
    }
    popup_manager_init();
    printf("YUI: backend ready\n");

    /* 2. 挂载 SPIFFS 分区（watch-os JS/JSON 等资源）。
     *    ESP-IDF 的 chdir() 是桩函数（永远返回 ENOSYS），不能依赖 cwd。
     *    资源路径一律用绝对路径 /spiffs/...（见 SPIFFS base_path）。 */
    esp_vfs_spiffs_conf_t spiffs_conf = {
        .base_path = "/spiffs",
        .partition_label = "spiffs",
        .max_files = 16,
        .format_if_mount_failed = false,
    };
    esp_err_t spiffs_err = esp_vfs_spiffs_register(&spiffs_conf);
    if (spiffs_err != ESP_OK) {
        printf("YUI: SPIFFS mount failed (%s), running without app resources\n",
               esp_err_to_name(spiffs_err));
    } else {
        size_t total = 0, used = 0;
        if (esp_spiffs_info("spiffs", &total, &used) == ESP_OK) {
            printf("YUI: SPIFFS mounted at /spiffs (%u/%u bytes used)\n",
                   (unsigned)used, (unsigned)total);
        } else {
            printf("YUI: SPIFFS mounted at /spiffs\n");
        }
    }

    /* 3. 加载字体：优先 Flash 映射（零 RAM），回退 RAM 加载。
     *    Flash 分区方案见 partitions.csv 的 "font" 分区与 README。
     *    QEMU 环境：无 font 分区，跳过字体加载（纯图形测试）。 */
    font = backend_esp32_load_font_from_flash("font", 16);
    if (!font) {
        font = backend_load_font("Roboto-Regular.ttf", 16);
    }
    if (!font) {
        printf("YUI: No font loaded, running in headless mode\n");
    }

    /* 4. 构建 UI */
    json = cJSON_Parse(s_ui_json);
    ui_root = layer_create_from_json(json, NULL);
    cJSON_Delete(json);
    if (!ui_root) {
        printf("YUI: failed to create UI\n");
        backend_quit();
        return;
    }

    /* 预置字体：写到解析时已创建/被子层共享的 Font 对象上，避免替换指针导致
     * 子层仍持有 default_font=NULL 的旧对象。 */
    if (font) {
        if (!ui_root->font) {
            ui_root->font = (Font*)malloc(sizeof(Font));
            memset(ui_root->font, 0, sizeof(Font));
            strcpy(ui_root->font->path, "Roboto-Regular.ttf");
            ui_root->font->size = 16;
            strcpy(ui_root->font->weight, "normal");
        }
        ui_root->font->default_font = font;
    }
    if (!ui_root->assets) {
        ui_root->assets = (Assets*)malloc(sizeof(Assets));
        memset(ui_root->assets, 0, sizeof(Assets));
    }
    /* 字体在 flash 分区，不在 SPIFFS assets 下 */
    ui_root->assets->path[0] = '\0';

    if (ui_root->rect.w <= 0 || ui_root->rect.h <= 0) {
        ui_root->rect.w = 240;
        ui_root->rect.h = 240;
    }
    load_all_fonts(ui_root);
    load_textures(ui_root);
    layout_layer(ui_root);

    /* 5. 主循环（内部不返回） */
    backend_run(ui_root);
    backend_quit();
}
