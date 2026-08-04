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
#include "js_module.h"
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

/* 简单 UI 描述（SPIFFS 里没有 app.json 时的回退） */
static const char s_fallback_ui_json[] =
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

/* 读整个文件到堆缓冲（调用方负责 free）。失败返回 NULL。 */
static char* read_file_alloc(const char* path, size_t max_len) {
    FILE* f = fopen(path, "rb");
    char* buf = NULL;
    long sz;
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0 || (size_t)sz > max_len) {
        fclose(f);
        return NULL;
    }
    buf = (char*)malloc((size_t)sz + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        free(buf);
        fclose(f);
        return NULL;
    }
    buf[sz] = '\0';
    fclose(f);
    return buf;
}

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

    /* 2. 挂载 SPIFFS 分区到 "/spiffs"（watch-os JS/JSON 等资源）。
     *    IDF vfs 不允许挂载根 "/"（base_path 至少 2 字符），故挂 /spiffs；
     *    设置 JS 文件系统根为 "/spiffs"，使 ../lib 上跳解析 clamp 在挂载根，
     *    共享代码零平台差异。 */
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
        js_module_set_fs_root("/spiffs");
    }
    printf("YUI: after spiffs free=%u largest=%u\n",
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT),
           (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));

    /* 3. 构建 UI：优先加载 /spiffs/app.json（watch-os 启动入口）。
     *    读取失败/解析失败时回退到内置的 s_fallback_ui_json。
     *    注意：字体加载（stb_truetype 解析吃堆）放到 JS 引擎初始化之后，
     *    否则最大连续块 < 64KB，JS 内存池 malloc 失败。 */
    {
        char* ui_buf = read_file_alloc("/spiffs/app.json", 64 * 1024);
        if (ui_buf) {
            json = cJSON_Parse(ui_buf);
            free(ui_buf);
            if (!json) {
                printf("YUI: /spiffs/app.json parse failed, using fallback UI\n");
            }
        } else {
            json = NULL;
        }
        if (!json) {
            json = cJSON_Parse(s_fallback_ui_json);
        }
        ui_root = layer_create_from_json(json, NULL);
    }
    if (!ui_root) {
        printf("YUI: failed to create UI\n");
        cJSON_Delete(json);
        backend_quit();
        return;
    }
    printf("YUI: after ui build free=%u largest=%u\n",
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT),
           (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));

    /* 5. 两阶段 JS 加载：
     *    a) 先绑定图层树（g_layer_root，事件注册需按 layer id 查找图层，不依赖 JS 引擎）。
     *    b) cJSON 树还活着时，收集 JS 文件路径 + 注册事件。
     *    c) 释放 cJSON 树（cJSON 解析 3.3KB JSON 需 ~20-40KB 堆）。
     *    d) 再初始化 JS 引擎 64KB 池 —— 否则堆碎片化导致 malloc 失败（QEMU 实测）。 */
    js_module_init_layer(ui_root);

    if (json) {
        js_module_collect_from_json(json, "/spiffs/app.json", 0);
        cJSON_Delete(json);
        json = NULL;
    }

    printf("YUI: before js_module_init free=%u largest=%u\n",
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT),
           (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));

    if (js_module_init() != 0) {
        printf("YUI: JS engine init failed, continuing without JS\n");
    }
    /* 6. 加载字体（放在 JS 引擎初始化之后，避免 stb_truetype 解析抢占堆）：
     *    优先 Flash 映射（零 RAM），回退 RAM 加载。
     *    Flash 分区方案见 partitions.csv 的 "font" 分区与 README。
     *    QEMU 环境：无 font 分区，跳过字体加载（纯图形测试）。 */
    font = backend_esp32_load_font_from_flash("font", 16);
    if (!font) {
        font = backend_load_font("Roboto-Regular.ttf", 16);
    }
    if (!font) {
        printf("YUI: No font loaded, running in headless mode\n");
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

    load_all_fonts(ui_root);

    /* 加载并执行 JS（阶段2：加载阶段1收集的路径）
     * onLoad 等生命周期事件由 layer_lifecycle 在脚本就绪后触发 */
    {
        int js_count = js_module_load_collected();
        printf("YUI: loaded %d JS file(s)\n", js_count);
        print_registered_events();
    }

    /* 屏幕尺寸优先：app.json 里的 size（如 watch-os 的 420x420）
     * 以实际 LCD 分辨率（240x240）为准 */
    ui_root->rect.w = 240;
    ui_root->rect.h = 240;
    load_textures(ui_root);
    layout_layer(ui_root);

    /* 7. 主循环（内部不返回） */
    backend_run(ui_root);
    js_module_cleanup();  // 清理 JS 引擎
    backend_quit();
}
