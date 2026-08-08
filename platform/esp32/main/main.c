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
 * 板级引脚/SPI 主机/时钟见下方 esp32_lcd_init() / esp32_touch_init() 内宏。
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
#include "event.h"
#include "js_module.h"
#include "cJSON.h"
#include "esp_spiffs.h"
#include "esp_heap_caps.h"
#include "driver/spi_master.h"

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_heap_caps.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "esp_timer.h"
#ifdef YUI_ESP32_QEMU
#include "esp_lcd_qemu_rgb.h"
#endif

/* 屏幕分辨率：与 app/watch-os/app.json 根节点 size:[420,420] 对齐，
 * 布局按此逻辑宽计算。修改需同时改两处引用（set_config / ui_root->rect）。 */
 #define YUI_SCREEN_WIDTH  240
 #define YUI_SCREEN_HEIGHT 240

/* framebuffer 模式（与 backend_esp32.c 保持一致）：QEMU 用虚拟面板显存，真机直写 */
#ifdef YUI_ESP32_QEMU
#ifndef YUI_ESP32_LCD_BUFFER
#define YUI_ESP32_LCD_BUFFER 1
#endif
#else
#ifndef YUI_ESP32_LCD_BUFFER
#define YUI_ESP32_LCD_BUFFER 0
#endif
#endif

#define EXAMPLE_LCD_PIXEL_CLOCK_HZ (10 * 1000 * 1000)
#define EXAMPLE_LCD_BK_LIGHT_ON_LEVEL  0
#define EXAMPLE_LCD_BK_LIGHT_OFF_LEVEL !EXAMPLE_LCD_BK_LIGHT_ON_LEVEL
#define EXAMPLE_PIN_NUM_DATA0          6  /*!< for 1-line SPI, this also refereed as MOSI */
#define EXAMPLE_PIN_NUM_PCLK           4 //SPI时钟
#define EXAMPLE_PIN_NUM_CS             -1 //CS脚
#define EXAMPLE_PIN_NUM_DC             8 //DC脚
#define EXAMPLE_PIN_NUM_RST            9 //RST脚
#define EXAMPLE_PIN_NUM_BK_LIGHT       10 //BL脚

// Using SPI2 in the example, as it also supports octal modes on some targets
#define LCD_HOST       SPI2_HOST
// To speed up transfers, every SPI transfer sends a bunch of lines. This define specifies how many.
// More means more memory use, but less overhead for setting up / finishing transfers. Make sure 240
// is dividable by this.
#define PARALLEL_LINES 16
// The number of frames to show before rotate the graph
#define ROTATE_FRAME   30


#define EXAMPLE_LCD_H_RES              YUI_SCREEN_WIDTH
#define EXAMPLE_LCD_V_RES              YUI_SCREEN_HEIGHT


/* ESP32 后端扩展接口（backend_esp32.c 定义，不在通用 backend.h 中） */
void backend_esp32_set_hw_display(int on);
DFont* backend_esp32_load_font_from_flash(const char* partition_label, int size);



/* 触摸芯片创建钩子：覆盖 backend_esp32.c 中的弱符号，实现为 CST816S。
 * 换触摸芯片时改这里即可，backend 层无需改动。 */
#include "esp_lcd_touch.h"
#include "esp_lcd_touch_cst816s.h"
esp_lcd_touch_handle_t yui_esp32_touch_create(esp_lcd_panel_io_handle_t io,
                                              const esp_lcd_touch_config_t* cfg) {
    esp_lcd_touch_handle_t t = NULL;
    // if (esp_lcd_touch_new_i2c_cst816s(io, cfg, &t) == ESP_OK) return t;
    return NULL;
}

/* ====================== 平台初始化（LCD / 触摸） ======================
 * 硬件相关初始化放在平台层（main.c），后端只负责渲染/轮询。
 * 通过 backend_esp32_set_panel / set_touch_handle 注入句柄给后端。 */

/* LCD/触摸配置（与 app_main 的 set_config 保持一致） */
#define YUI_LCD_SPI_HOST LCD_HOST
#define YUI_LCD_FREQ_HZ  EXAMPLE_LCD_PIXEL_CLOCK_HZ
#define YUI_TOUCH_I2C_HOST  0
#define YUI_TOUCH_SDA       4
#define YUI_TOUCH_SCL       5
#define YUI_TOUCH_ADDR      0x15
#define YUI_TOUCH_INT       -1

extern void backend_esp32_set_panel(esp_lcd_panel_handle_t p);
extern void backend_esp32_set_touch_handle(esp_lcd_touch_handle_t t);
extern void backend_esp32_set_framebuffer(uint16_t* fb, int w, int h);
extern int  backend_esp32_get_hw_display(void);

static esp_lcd_panel_handle_t s_lcd_panel = NULL;
static esp_lcd_touch_handle_t s_lcd_touch = NULL;

static int esp32_lcd_init(void) {
    spi_device_handle_t spi = NULL;
    esp_lcd_panel_io_handle_t io_handle = NULL;
    spi_bus_config_t buscfg;
    esp_lcd_panel_io_spi_config_t io_config;
    esp_lcd_panel_dev_config_t panel_config;
    esp_err_t ret;

#ifdef YUI_ESP32_QEMU
    {
        esp_lcd_rgb_qemu_config_t qcfg = {
            .width = YUI_SCREEN_WIDTH,
            .height = YUI_SCREEN_HEIGHT,
            .bpp = RGB_QEMU_BPP_16,
        };
        ret = esp_lcd_new_rgb_qemu(&qcfg, &s_lcd_panel);
        if (ret != ESP_OK) {
            ESP_LOGE("yui-esp32", "esp_lcd_new_rgb_qemu failed (%s)", esp_err_to_name(ret));
            return -1;
        }
        esp_lcd_panel_reset(s_lcd_panel);
        esp_lcd_panel_init(s_lcd_panel);
        printf("YUI: QEMU virtual RGB panel %dx%d RGB565 ready\n",
               YUI_SCREEN_WIDTH, YUI_SCREEN_HEIGHT);
    }
    return 0;
#else
    if (!backend_esp32_get_hw_display()) {
        ESP_LOGI("yui-esp32", "hw display off: software framebuffer only");
        return 0;
    }

    memset(&buscfg, 0, sizeof(buscfg));
    buscfg.mosi_io_num = EXAMPLE_PIN_NUM_DATA0;
    buscfg.miso_io_num = -1;
    buscfg.sclk_io_num = EXAMPLE_PIN_NUM_PCLK;
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;
    buscfg.max_transfer_sz = YUI_SCREEN_WIDTH * YUI_SCREEN_HEIGHT * 2 + 8;

    ret = spi_bus_initialize((spi_host_device_t)YUI_LCD_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGW("yui-esp32", "spi_bus_initialize failed (%s), running in headless mode", esp_err_to_name(ret));
        return 0;
    }

    memset(&io_config, 0, sizeof(io_config));
    io_config.dc_gpio_num = EXAMPLE_PIN_NUM_DC;
    io_config.cs_gpio_num = EXAMPLE_PIN_NUM_CS;
    io_config.pclk_hz = YUI_LCD_FREQ_HZ;
    io_config.lcd_cmd_bits = 8;
    io_config.lcd_param_bits = 8;
    io_config.spi_mode = 0;
    io_config.trans_queue_depth = 10;

    ret = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)YUI_LCD_SPI_HOST, &io_config, &io_handle);
    if (ret != ESP_OK) {
        ESP_LOGW("yui-esp32", "esp_lcd_new_panel_io_spi failed (%s), running in headless mode", esp_err_to_name(ret));
        return 0;
    }

    memset(&panel_config, 0, sizeof(panel_config));
    panel_config.reset_gpio_num = EXAMPLE_PIN_NUM_RST;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
#else
    panel_config.color_space = ESP_LCD_COLOR_SPACE_RGB;
#endif
    panel_config.bits_per_pixel = 16;

    ret = esp_lcd_new_panel_st7789(io_handle, &panel_config, &s_lcd_panel);
    if (ret != ESP_OK) {
        ESP_LOGW("yui-esp32", "esp_lcd_new_panel_st7789 failed (%s), running in headless mode", esp_err_to_name(ret));
        return 0;
    }
    printf("YUI: ST7789 panel created ok\n");
    ESP_ERROR_CHECK(gpio_set_level(EXAMPLE_PIN_NUM_BK_LIGHT, EXAMPLE_LCD_BK_LIGHT_ON_LEVEL));

    esp_lcd_panel_reset(s_lcd_panel);
    vTaskDelay(pdMS_TO_TICKS(150));
    esp_lcd_panel_init(s_lcd_panel);
    esp_lcd_panel_invert_color(s_lcd_panel, true);
    esp_lcd_panel_mirror(s_lcd_panel, true, false);
    esp_lcd_panel_disp_on_off(s_lcd_panel, true);

    ESP_ERROR_CHECK(gpio_set_level(EXAMPLE_PIN_NUM_BK_LIGHT, EXAMPLE_LCD_BK_LIGHT_OFF_LEVEL));

    /* TEMP 测试：循环推红色测试图案 */
    static uint16_t red[240 * 240];
    for (;;) {
        for (int i = 0; i < 240 * 240; i++) red[i] = 0xF800;
        esp_err_t dr = esp_lcd_panel_draw_bitmap(s_lcd_panel, 0, 0, 240, 240, red);
        printf("YUI: draw bitmap ret=%s\n", esp_err_to_name(dr));
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    if (EXAMPLE_PIN_NUM_BK_LIGHT >= 0) {
        gpio_set_direction((gpio_num_t)EXAMPLE_PIN_NUM_BK_LIGHT, GPIO_MODE_OUTPUT);
        gpio_set_level((gpio_num_t)EXAMPLE_PIN_NUM_BK_LIGHT, 1);
    }
    return 0;
#endif
}

static int esp32_touch_init(void) {
    esp_lcd_touch_config_t tcfg;
    esp_lcd_touch_handle_t t = NULL;
    if (!backend_esp32_get_hw_display()) return 0;
    if (!s_lcd_panel) return 0;
    if (YUI_TOUCH_I2C_HOST < 0 || YUI_TOUCH_SDA < 0) return 0;

    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = (i2c_port_num_t)YUI_TOUCH_I2C_HOST,
        .sda_io_num = YUI_TOUCH_SDA,
        .scl_io_num = YUI_TOUCH_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus = NULL;
    if (i2c_new_master_bus(&bus_cfg, &bus) != ESP_OK) {
        ESP_LOGW("yui-esp32", "touch i2c bus init failed, running without touch");
        return 0;
    }

    esp_lcd_panel_io_i2c_config_t io_cfg = {
        .dev_addr = YUI_TOUCH_ADDR,
        .scl_speed_hz = 400000,
        .control_phase_bytes = 1,
        .dc_bit_offset = 0,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 0,
        .flags = {
            .dc_low_on_data = 0,
            .disable_control_phase = 1,
        },
    };
    esp_lcd_panel_io_handle_t io = NULL;
    if (esp_lcd_new_panel_io_i2c(bus, &io_cfg, &io) != ESP_OK) {
        ESP_LOGW("yui-esp32", "touch panel io init failed, running without touch");
        return 0;
    }

    memset(&tcfg, 0, sizeof(tcfg));
    tcfg.x_max = YUI_SCREEN_WIDTH;
    tcfg.y_max = YUI_SCREEN_HEIGHT;
    tcfg.rst_gpio_num = -1;
    tcfg.int_gpio_num = YUI_TOUCH_INT;

    t = yui_esp32_touch_create(io, &tcfg);
    if (t != NULL) {
        s_lcd_touch = t;
    } else {
        ESP_LOGW("yui-esp32", "touch init failed, running without touch");
    }
    return 0;
}

/* 简单 UI 描述（SPIFFS 里没有 app.json 时的回退；也可作渲染自检场景：
 * 覆盖 View 背景、各字号 Label、Button、Progress 的基础渲染） */
static const char s_fallback_ui_json[] =
    "{"
    "  \"id\": \"watch_os\","
    "  \"type\": \"View\","
    "  \"size\": [240, 240],"
    "  \"layout\": {\"type\": \"vertical\", \"spacing\": 6},"
    "  \"style\": {\"bgColor\": \"#202020\"},"
    "  \"text\": \"YUI Test\","
    "  \"children\": ["
    "    {\"id\": \"lbl_red\", \"type\": \"Label\", \"text\": \"RED Label 24\", \"size\": [220, 30],"
    "     \"style\": {\"color\": \"#FF0000\", \"fontSize\": 24}},"
    "    {\"id\": \"lbl_white\", \"type\": \"Label\", \"text\": \"WHITE 32\", \"size\": [220, 40],"
    "     \"style\": {\"color\": \"#FFFFFF\", \"fontSize\": 32}},"
    "    {\"id\": \"lbl_cyan\", \"type\": \"Label\", \"text\": \"CYAN BIG 48\", \"size\": [220, 50],"
    "     \"style\": {\"color\": \"#00D4FF\", \"fontSize\": 48}},"
    "    {\"id\": \"btn_primary\", \"type\": \"Button\", \"text\": \"PRIMARY\", \"size\": [220, 36],"
    "     \"style\": {\"bgColor\": \"#00D4FF\", \"color\": \"#000000\", \"borderRadius\": 12}},"
    "    {\"id\": \"btn_dark\", \"type\": \"Button\", \"text\": \"DARK BTN\", \"size\": [220, 34],"
    "     \"style\": {\"bgColor\": \"#1C1C1E\", \"color\": \"#FFFFFF\", \"borderRadius\": 10}},"
    "    {\"id\": \"progress_h\", \"type\": \"Progress\", \"value\": 66, \"size\": [220, 8],"
    "     \"style\": {\"bgColor\": \"#333333\", \"fillColor\": \"#30D158\"}}"
    "  ]"
    "}";

/* 读整个文件到堆缓冲（调用方负责 free）。失败返回 NULL。 */
static void check_heap(const char* tag) {
    printf("YUI: [%s] total=%u free=%u minfree=%u largest=%u\n", tag,
           (unsigned)heap_caps_get_total_size(MALLOC_CAP_8BIT),
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT),
           (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT),
           (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
}

/* JS 可调用事件：YUI.call("check_heap", "tag")
 * data 为 JSON 字符串（可为 null），打印堆占用后返回成功 JSON。 */
static void* handle_check_heap(void* data) {
    const char* tag = data ? (const char*)data : "";
    check_heap(tag);
    return strdup("{\"success\":true}");
}

static char* read_file_alloc(const char* path, size_t max_len) {    FILE* f = fopen(path, "rb");
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

    printf("YUI ESP32 starting...%d\n", SPI2_HOST);

    /* 0. 板级配置：分辨率/SPI 主机/CS/DC/RST/BL/SPI 时钟
     * ESP32-C3 只有 SPI2_HOST(=1)，没有 host=2；GPIO 仅 0–21。
     * 旧默认 (host=2, MOSI=23) 会触发 spi_bus_initialize: invalid host_id。 */
#if CONFIG_IDF_TARGET_ESP32C3

    gpio_config_t bk_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << EXAMPLE_PIN_NUM_BK_LIGHT //BL脚
    };
    // Initialize the GPIO of backlight
    ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));

    /* SPI 总线由 esp32_lcd_init() 初始化（避免重复 spi_bus_initialize） */


    /* 模块丝印 gnd vcc scl sda res dc blk：无 CS 脚（内部固定选通，cs=-1）；
     * RST 接 GPIO7，背光接 GPIO8。杜邦线/面包板 40MHz 信号完整性差，
     * 降到 10MHz 更稳（ST7789 支持 5-10MHz 正常工作）。
     * 板级引脚/SPI 主机/时钟见 esp32_lcd_init() 内宏（YUI_LCD_SPI_HOST 等）。 */
#else
    /* 板级引脚/SPI 主机/时钟见 esp32_lcd_init() 内宏（YUI_LCD_SPI_HOST 等）。 */
#endif
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
    /* 触摸参数见 esp32_touch_init() 内宏（YUI_TOUCH_*）；C3 无触摸硬件默认不探测 */
#endif

    /* 1. 平台初始化 LCD/触摸（硬件相关，由平台层负责），并把句柄注入后端。 */
    if (esp32_lcd_init() != 0) {
        printf("YUI: esp32_lcd_init failed\n");
        return;
    }
    esp32_touch_init();
    backend_esp32_set_panel(s_lcd_panel);
    backend_esp32_set_touch_handle(s_lcd_touch);

    /* framebuffer：QEMU 从虚拟面板取专属显存，真机 buffer 模式堆分配 */
    {
        uint16_t* fb = NULL;
        int fb_w = YUI_SCREEN_WIDTH, fb_h = YUI_SCREEN_HEIGHT;
#ifdef YUI_ESP32_QEMU
        esp_lcd_rgb_qemu_get_frame_buffer(s_lcd_panel, (void**)&fb);
        if (!fb) {
            printf("YUI: esp_lcd_rgb_qemu_get_frame_buffer failed\n");
            return;
        }
        printf("YUI: QEMU framebuffer %p (%dx%d RGB565, dedicated RAM)\n",
               (void*)fb, fb_w, fb_h);
#else
#if YUI_ESP32_LCD_BUFFER
        fb = (uint16_t*)calloc((size_t)fb_w * fb_h, 2);
        if (!fb) {
            printf("YUI: framebuffer calloc %ux%ux2=%u bytes failed, "
                   "free=%u largest=%u\n",
                   fb_w, fb_h, (unsigned)((size_t)fb_w * fb_h * 2),
                   (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT),
                   (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
            return;
        }
#else
        printf("YUI: LCD buffer disabled (direct draw), free=%u largest=%u\n",
               (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT),
               (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
#endif
#endif
        backend_esp32_set_framebuffer(fb, fb_w, fb_h);
    }

    /* 2. 初始化后端 + 弹层管理。
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
    register_event_handler("check_heap", handle_check_heap);
    printf("YUI: backend ready\n");

    /* 3. 挂载 SPIFFS 分区到 "/spiffs"（watch-os JS/JSON 等资源）。
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
        js_module_set_root("/spiffs");
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
    printf("YUI: ui id='%s' flags=0x%x onLoad='%s'\n",
           ui_root->id, (unsigned)ui_root->lifecycle_flags, ui_root->lifecycle_on_load);
    check_heap("after ui build");

    /* 5. 两阶段 JS 加载：
     *    a) 先绑定图层树（g_layer_root，事件注册需按 layer id 查找图层，不依赖 JS 引擎）。
     *    b) cJSON 树还活着时，收集 JS 文件路径 + 注册事件。
     *    c) 释放 cJSON 树（cJSON 解析 3.3KB JSON 需 ~20-40KB 堆）。
     *    d) 再初始化 JS 引擎 64KB 池 —— 否则堆碎片化导致 malloc 失败（QEMU 实测）。 */
    js_module_init_layer(ui_root);
    printf("YUI: lc after init_layer flags=0x%x onLoad='%s'\n",
           (unsigned)ui_root->lifecycle_flags, ui_root->lifecycle_on_load);

    if (json) {
        js_module_collect_from_json(json, "/spiffs/app.json", 0);
        printf("YUI: lc after collect flags=0x%x onLoad='%s'\n",
               (unsigned)ui_root->lifecycle_flags, ui_root->lifecycle_on_load);
        check_heap("after collect");
        cJSON_Delete(json);
        printf("YUI: lc after cjson free flags=0x%x onLoad='%s'\n",
               (unsigned)ui_root->lifecycle_flags, ui_root->lifecycle_on_load);
        check_heap("after cjson free");
        json = NULL;
    }

    if (js_module_init() != 0) {
        printf("YUI: JS engine init failed, continuing without JS\n");
    }
    check_heap("after js pool");
    printf("YUI: lc after js_init flags=0x%x onLoad='%s'\n",
           (unsigned)ui_root->lifecycle_flags, ui_root->lifecycle_on_load);
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
    printf("YUI: lc after font flags=0x%x onLoad='%s'\n",
           (unsigned)ui_root->lifecycle_flags, ui_root->lifecycle_on_load);

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
    printf("YUI: lc after load_all_fonts flags=0x%x onLoad='%s'\n",
           (unsigned)ui_root->lifecycle_flags, ui_root->lifecycle_on_load);

    /* 加载并执行 JS（阶段2：加载阶段1收集的路径）
     * onLoad 等生命周期事件由 layer_lifecycle 在脚本就绪后触发 */
    {
        int js_count = js_module_load_collected();
        printf("YUI: loaded %d JS file(s)\n", js_count);
        print_registered_events();
    }

    /* 屏幕尺寸：与 esp32_lcd_init() 的 LCD 分辨率一致（YUI_SCREEN_WIDTH/
     * HEIGHT，对齐 app/watch-os/app.json 根节点 size，布局按此计算）。 */
    ui_root->rect.w = YUI_SCREEN_WIDTH;
    ui_root->rect.h = YUI_SCREEN_HEIGHT;
    load_textures(ui_root);
    printf("YUI: layout...\n");
    layout_layer(ui_root);
    printf("YUI: enter main loop\n");

    /* 7. 主循环（内部不返回） */
    backend_run(ui_root);
    js_module_cleanup();  // 清理 JS 引擎
    backend_quit();
}
