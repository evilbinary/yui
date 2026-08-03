/*
 * ESP32 原生后端（不依赖 LVGL）
 * 合并 LCD 驱动 + 触摸输入 + 软件渲染于一体。
 * Framebuffer 使用 RGB565（节省 RAM），字体经 backend_embed_font 渲染。
 *
 * 编译宏：
 *   - ESP_PLATFORM      由 ESP-IDF 定义，启用真实 LCD/触摸驱动
 *   - 未定义 ESP_PLATFORM 时为 PC stub（仅用于语法/逻辑检查）
 */
#include "backend.h"
#include "component_registry.h"
#include "event.h"
#include "render.h"
#include "popup_manager.h"
#include "backend_embed_font.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef ESP_PLATFORM
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_partition.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#endif

#define YUI_E32_TAG "yui-esp32"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

float yui_density = 1.0f;

float backend_get_density(void) { return yui_density > 0.0f ? yui_density : 1.0f; }
void backend_set_density(float d) { if (d > 0.0f) yui_density = d; }

/* ====================== 配置 ====================== */
typedef struct {
    int width;
    int height;
    int spi_host;       /* SPI_HOST / FSPI_HOST */
    int mosi, sclk;     /* SPI 总线引脚 */
    int cs, dc, rst, bl;
    int freq_hz;        /* SPI 时钟 (Hz) */
    int touch_i2c_host; /* -1 = 无触摸 */
    int touch_sda, touch_scl, touch_addr, touch_int;
} yui_esp32_config_t;

static yui_esp32_config_t s_cfg = {
    .width = 240, .height = 240,
    .spi_host = 2, /* FSPI_HOST */
    .mosi = 23, .sclk = 18,
    .cs = 5, .dc = 16, .rst = -1, .bl = -1,
    .freq_hz = 40 * 1000 * 1000,
    .touch_i2c_host = -1,
    .touch_sda = -1, .touch_scl = -1, .touch_addr = 0x15, .touch_int = -1,
};

void backend_esp32_set_config(int width, int height,
                              int spi_host, int cs, int dc, int rst, int bl,
                              int freq_hz) {
    s_cfg.width = width;
    s_cfg.height = height;
    s_cfg.spi_host = spi_host;
    s_cfg.cs = cs;
    s_cfg.dc = dc;
    s_cfg.rst = rst;
    s_cfg.bl = bl;
    s_cfg.freq_hz = freq_hz > 0 ? freq_hz : 40 * 1000 * 1000;
}

void backend_esp32_set_spi_pins(int mosi, int sclk) {
    s_cfg.mosi = mosi;
    s_cfg.sclk = sclk;
}

void backend_esp32_set_touch(int i2c_host, int sda, int scl, int addr, int int_pin) {
    s_cfg.touch_i2c_host = i2c_host;
    s_cfg.touch_sda = sda;
    s_cfg.touch_scl = scl;
    s_cfg.touch_addr = addr;
    s_cfg.touch_int = int_pin;
}

#ifdef ESP_PLATFORM
static esp_partition_mmap_handle_t s_font_mmap_handle;
#endif

/* 从 SPI Flash 分区映射 TTF（不占 RAM），适合子集化小字体。
 * partitions.csv 中需定义同名 data 分区，烧录子集化 TTF 到该分区。 */
DFont* backend_esp32_load_font_from_flash(const char* partition_label, int size) {
#ifdef ESP_PLATFORM
    const esp_partition_t* part;
    const void* mapped;
    esp_err_t err;
    if (!partition_label) return NULL;
    part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, partition_label);
    if (!part) {
        ESP_LOGE(YUI_E32_TAG, "font partition '%s' not found", partition_label);
        return NULL;
    }
    err = esp_partition_mmap(part, 0, part->size, ESP_PARTITION_MMAP_DATA,
                             &mapped, &s_font_mmap_handle);
    if (err != ESP_OK) {
        ESP_LOGE(YUI_E32_TAG, "font mmap failed: %s", esp_err_to_name(err));
        return NULL;
    }
    /* mapped 指向 Flash，embed_font_load_from_memory 不复制、不释放 */
    return embed_font_load_from_memory(mapped, part->size, size, "normal");
#else
    (void)partition_label; (void)size;
    return NULL;
#endif
}

/* ====================== Framebuffer ====================== */
static uint16_t* s_fb = NULL;       /* RGB565 */
static int s_fb_w = 0, s_fb_h = 0;

/* 脏区域合并 */
static Rect s_dirty = {0, 0, 0, 0};
static int s_has_dirty = 0;

static void dirty_reset(void) { s_has_dirty = 0; s_dirty.x = s_dirty.y = s_dirty.w = s_dirty.h = 0; }
static void dirty_add(int x, int y, int w, int h) {
    int x0, y0, x1, y1;
    if (w <= 0 || h <= 0) return;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x >= s_fb_w || y >= s_fb_h) return;
    if (x + w > s_fb_w) w = s_fb_w - x;
    if (y + h > s_fb_h) h = s_fb_h - y;
    if (w <= 0 || h <= 0) return;
    x0 = x; y0 = y; x1 = x + w; y1 = y + h;
    if (!s_has_dirty) {
        s_dirty.x = x0; s_dirty.y = y0; s_dirty.w = w; s_dirty.h = h;
        s_has_dirty = 1;
    } else {
        int nx0 = s_dirty.x < x0 ? s_dirty.x : x0;
        int ny0 = s_dirty.y < y0 ? s_dirty.y : y0;
        int nx1 = (s_dirty.x + s_dirty.w) > x1 ? (s_dirty.x + s_dirty.w) : x1;
        int ny1 = (s_dirty.y + s_dirty.h) > y1 ? (s_dirty.y + s_dirty.h) : y1;
        s_dirty.x = nx0; s_dirty.y = ny0;
        s_dirty.w = nx1 - nx0; s_dirty.h = ny1 - ny0;
    }
}

static inline uint16_t color_to_rgb565(Color c) {
    return (uint16_t)((((uint16_t)(c.r >> 3)) << 11) |
                      (((uint16_t)(c.g >> 2)) << 5) |
                      ((uint16_t)(c.b >> 3)));
}

static inline Color rgb565_to_color(uint16_t px) {
    Color c;
    c.r = (unsigned char)((px >> 11) & 0x1F) << 3;
    c.g = (unsigned char)((px >> 5) & 0x3F) << 2;
    c.b = (unsigned char)(px & 0x1F) << 3;
    c.a = 255;
    return c;
}

/* ====================== 裁剪栈 ====================== */
#define CLIP_MAX 16
static Rect s_clip_stack[CLIP_MAX];
static int s_clip_depth = 0;

static void clip_intersect(Rect* out, const Rect* a, const Rect* b) {
    int x0, y0, x1, y1;
    x0 = a->x > b->x ? a->x : b->x;
    y0 = a->y > b->y ? a->y : b->y;
    x1 = (a->x + a->w) < (b->x + b->w) ? (a->x + a->w) : (b->x + b->w);
    y1 = (a->y + a->h) < (b->y + b->h) ? (a->y + a->h) : (b->y + b->h);
    out->x = x0; out->y = y0;
    out->w = x1 > x0 ? x1 - x0 : 0;
    out->h = y1 > y0 ? y1 - y0 : 0;
}

static int clip_get_current(Rect* out) {
    if (s_clip_depth <= 0) {
        out->x = 0; out->y = 0; out->w = s_fb_w; out->h = s_fb_h;
        return 0;
    }
    *out = s_clip_stack[s_clip_depth - 1];
    return 1;
}

/* ====================== ESP32 LCD/Touch ====================== */
#ifdef ESP_PLATFORM
static esp_lcd_panel_handle_t s_panel = NULL;
static esp_lcd_touch_handle_t s_touch = NULL;

static bool s_touch_active = false;
static int s_touch_x = 0, s_touch_y = 0;

static int esp32_lcd_init(void) {
    spi_device_handle_t spi = NULL;
    esp_lcd_panel_io_handle_t io_handle = NULL;
    spi_bus_config_t buscfg;
    esp_lcd_panel_io_spi_config_t io_config;
    esp_lcd_panel_dev_config_t panel_config;
    esp_err_t ret;

    memset(&buscfg, 0, sizeof(buscfg));
    buscfg.mosi_io_num = s_cfg.mosi;
    buscfg.miso_io_num = -1;
    buscfg.sclk_io_num = s_cfg.sclk;
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;
    buscfg.max_transfer_sz = s_cfg.width * s_cfg.height * 2 + 8;

    ret = spi_bus_initialize((spi_host_device_t)s_cfg.spi_host, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(YUI_E32_TAG, "spi_bus_initialize failed: %s", esp_err_to_name(ret));
        return -1;
    }

    memset(&io_config, 0, sizeof(io_config));
    io_config.dc_gpio_num = s_cfg.dc;
    io_config.cs_gpio_num = s_cfg.cs;
    io_config.pclk_hz = s_cfg.freq_hz;
    io_config.lcd_cmd_bits = 8;
    io_config.lcd_param_bits = 8;
    io_config.spi_mode = 0;
    io_config.trans_queue_depth = 10;

    ret = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)s_cfg.spi_host, &io_config, &io_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(YUI_E32_TAG, "esp_lcd_new_panel_io_spi failed: %s", esp_err_to_name(ret));
        return -1;
    }

    memset(&panel_config, 0, sizeof(panel_config));
    panel_config.reset_gpio_num = s_cfg.rst;
    panel_config.color_space = ESP_LCD_COLOR_SPACE_RGB;
    panel_config.bits_per_pixel = 16;

    ret = esp_lcd_new_panel_st7789(io_handle, &panel_config, &s_panel);
    if (ret != ESP_OK) {
        ESP_LOGE(YUI_E32_TAG, "esp_lcd_new_panel_st7789 failed: %s", esp_err_to_name(ret));
        return -1;
    }

    esp_lcd_panel_reset(s_panel);
    esp_lcd_panel_init(s_panel);
    esp_lcd_panel_mirror(s_panel, true, false);
    esp_lcd_panel_disp_on_off(s_panel, true);

    if (s_cfg.bl >= 0) {
        gpio_set_direction((gpio_num_t)s_cfg.bl, GPIO_MODE_OUTPUT);
        gpio_set_level((gpio_num_t)s_cfg.bl, 1);
    }
    return 0;
}

/* 触摸芯片创建钩子（弱符号，默认无触摸）。
 * 平台层可强定义同名函数覆盖，例如 CST816S：
 *   esp_lcd_touch_new_i2c_cst816s(io, cfg, &t); */
__attribute__((weak))
esp_lcd_touch_handle_t yui_esp32_touch_create(esp_lcd_panel_io_handle_t io,
                                              const esp_lcd_touch_config_t* cfg) {
    (void)io; (void)cfg;
    return NULL;
}

static int esp32_touch_init(void) {
    esp_lcd_touch_config_t tcfg;
    esp_lcd_touch_handle_t t = NULL;
    if (s_cfg.touch_i2c_host < 0 || s_cfg.touch_sda < 0) return 0;

    /* I2C 总线（IDF 5.x 新驱动） */
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = (i2c_port_num_t)s_cfg.touch_i2c_host,
        .sda_io_num = s_cfg.touch_sda,
        .scl_io_num = s_cfg.touch_scl,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus = NULL;
    if (i2c_new_master_bus(&bus_cfg, &bus) != ESP_OK) {
        ESP_LOGW(YUI_E32_TAG, "touch i2c bus init failed, running without touch");
        return 0;
    }

    /* 触摸用 panel IO（通用 I2C 寄存器式配置，芯片无关） */
    esp_lcd_panel_io_i2c_config_t io_cfg = {
        .dev_addr = s_cfg.touch_addr,
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
        ESP_LOGW(YUI_E32_TAG, "touch panel io init failed, running without touch");
        return 0;
    }

    memset(&tcfg, 0, sizeof(tcfg));
    tcfg.x_max = s_cfg.width;
    tcfg.y_max = s_cfg.height;
    tcfg.rst_gpio_num = -1;
    tcfg.int_gpio_num = s_cfg.touch_int;

    /* 创建触摸芯片（由平台层钩子提供具体驱动实现） */
    t = yui_esp32_touch_create(io, &tcfg);
    if (t != NULL) {
        s_touch = t;
    } else {
        ESP_LOGW(YUI_E32_TAG, "touch init failed, running without touch");
    }
    return 0;
}

static void esp32_flush_dirty(void) {
    if (!s_has_dirty || !s_panel) return;
    esp_lcd_panel_draw_bitmap(s_panel, s_dirty.x, s_dirty.y,
                              s_dirty.x + s_dirty.w, s_dirty.y + s_dirty.h,
                              s_fb + s_dirty.y * s_fb_w + s_dirty.x);
    dirty_reset();
}

static void esp32_touch_poll(PointerEvent* ev, int* has_event) {
    uint16_t tx[5], ty[5];
    uint16_t strength[5];
    uint8_t cnt = 0;
    *has_event = 0;
    if (!s_touch) return;
    esp_lcd_touch_read_data(s_touch);
    if (esp_lcd_touch_get_coordinates(s_touch, tx, ty, strength, &cnt, 5) && cnt > 0) {
        int x = tx[0], y = ty[0];
        if (!s_touch_active) {
            s_touch_active = true;
            s_touch_x = x; s_touch_y = y;
            ev->device = POINTER_DEVICE_TOUCH;
            ev->phase = POINTER_DOWN;
            ev->x = x; ev->y = y;
            ev->button = 1; ev->pointer_id = 0; ev->finger_count = cnt;
            *has_event = 1;
        } else {
            int dx = x - s_touch_x, dy = y - s_touch_y;
            s_touch_x = x; s_touch_y = y;
            if (dx != 0 || dy != 0) {
                ev->device = POINTER_DEVICE_TOUCH;
                ev->phase = POINTER_MOVE;
                ev->x = x; ev->y = y;
                ev->delta_x = dx; ev->delta_y = dy;
                ev->pointer_id = 0; ev->finger_count = cnt;
                *has_event = 1;
            }
        }
    } else if (s_touch_active) {
        s_touch_active = false;
        ev->device = POINTER_DEVICE_TOUCH;
        ev->phase = POINTER_UP;
        ev->x = s_touch_x; ev->y = s_touch_y;
        ev->button = 1; ev->pointer_id = 0; ev->finger_count = 0;
        *has_event = 1;
    }
}

static uint32_t esp32_ticks(void) {
    return (uint32_t)(esp_timer_get_time() / 1000);
}
#endif /* ESP_PLATFORM */

/* ====================== Auto frames / quit ====================== */
static int s_auto_frames = -1;
static int s_frame_count = 0;
static int s_exit_code = 0;
static int s_should_quit = 0;
static int s_headless = 0;

void backend_set_auto_frames(int f) { s_auto_frames = f; }
void backend_request_quit(int c) { s_exit_code = c; s_should_quit = 1; }
int backend_get_exit_code(void) { return s_exit_code; }
int backend_should_quit(void) { return s_should_quit; }
void backend_set_headless(int on) { s_headless = on; }
int backend_is_headless(void) { return s_headless; }

/* ====================== 初始化 ====================== */
int backend_init(void) {
    yui_component_registry_init();
    yui_components_register_builtin();

    s_fb_w = s_cfg.width;
    s_fb_h = s_cfg.height;
    s_fb = (uint16_t*)calloc((size_t)s_fb_w * s_fb_h, 2);
    if (!s_fb) return -1;

#ifdef ESP_PLATFORM
    esp32_lcd_init();
    esp32_touch_init();
#endif
    return 0;
}

void backend_quit(void) {
#ifdef ESP_PLATFORM
    if (s_panel) {
        esp_lcd_panel_disp_on_off(s_panel, false);
    }
#endif
    if (s_fb) { free(s_fb); s_fb = NULL; }
}

/* ====================== 纹理 ====================== */
/* 图片纹理：priv 指向 ImageTexture */
typedef struct {
    unsigned char* pixels; /* RGBA8888 */
    int w, h;
    int owns;
} ImageTexture;

Texture* backend_load_texture(char* path) {
    /* 嵌入式平台图片加载按需实现（可用 stb_image） */
    (void)path;
    return NULL;
}

Texture* backend_load_texture_from_base64(const char* base64_data, size_t data_len) {
    (void)base64_data; (void)data_len;
    return NULL;
}

int backend_measure_text_width(DFont* font, const char* text) {
    return embed_font_measure_text(font, text);
}

Texture* backend_render_texture(DFont* font, const char* text, Color color) {
    return embed_font_render(font, text, color);
}

void backend_render_text_destroy(Texture* texture) {
    /* embed_font_render 返回的纹理归缓存管理，release 仅引用计数 -1 */
    embed_font_texture_release(texture);
}

/* ====================== 基础绘制 ====================== */
void backend_render_fill_rect(Rect* rect, Color color) {
    Rect clip, r;
    int x, y, x1, y1;
    uint16_t px;
    if (!s_fb || !rect) return;
    clip_get_current(&clip);
    r = *rect;
    clip_intersect(&r, &r, &clip);
    if (r.w <= 0 || r.h <= 0) return;

    px = color_to_rgb565(color);
    /* alpha 混合 */
    if (color.a == 255) {
        for (y = r.y; y < r.y + r.h; y++) {
            uint16_t* row = s_fb + y * s_fb_w + r.x;
            for (x = 0; x < r.w; x++) row[x] = px;
        }
    } else if (color.a > 0) {
        unsigned a = color.a;
        for (y = r.y; y < r.y + r.h; y++) {
            uint16_t* row = s_fb + y * s_fb_w + r.x;
            for (x = 0; x < r.w; x++) {
                Color d = rgb565_to_color(row[x]);
                d.r = (unsigned char)((d.r * (255 - a) + color.r * a) / 255);
                d.g = (unsigned char)((d.g * (255 - a) + color.g * a) / 255);
                d.b = (unsigned char)((d.b * (255 - a) + color.b * a) / 255);
                row[x] = color_to_rgb565(d);
            }
        }
    }
    dirty_add(r.x, r.y, r.w, r.h);
}

void backend_render_rect(Rect* rect, Color color) {
    Rect t;
    if (!rect) return;
    t = *rect;
    t.h = 1; backend_render_fill_rect(&t, color);
    t = *rect; t.y = rect->y + rect->h - 1; t.h = 1; backend_render_fill_rect(&t, color);
    t = *rect; t.w = 1; backend_render_fill_rect(&t, color);
    t = *rect; t.x = rect->x + rect->w - 1; t.w = 1; backend_render_fill_rect(&t, color);
}

void backend_render_rect_color(Rect* rect, unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    Color c = {r, g, b, a};
    backend_render_rect(rect, c);
}

void backend_render_fill_rect_color(Rect* rect, unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    Color c = {r, g, b, a};
    backend_render_fill_rect(rect, c);
}

void backend_render_clear_color(unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    Rect full;
    if (!s_fb) return;
    full.x = 0; full.y = 0; full.w = s_fb_w; full.h = s_fb_h;
    backend_render_fill_rect(&full, (Color){r, g, b, a});
}

void backend_render_line(int x1, int y1, int x2, int y2, Color color) {
    int dx, dy, sx, sy, err, e2;
    Rect clip;
    if (!s_fb) return;
    clip_get_current(&clip);
    dx = abs(x2 - x1); dy = abs(y2 - y1);
    sx = x1 < x2 ? 1 : -1; sy = y1 < y2 ? 1 : -1;
    err = dx - dy;
    while (1) {
        if (x1 >= clip.x && x1 < clip.x + clip.w && y1 >= clip.y && y1 < clip.y + clip.h) {
            if (color.a == 255) {
                s_fb[y1 * s_fb_w + x1] = color_to_rgb565(color);
            } else if (color.a > 0) {
                Color d = rgb565_to_color(s_fb[y1 * s_fb_w + x1]);
                unsigned a = color.a;
                d.r = (unsigned char)((d.r * (255 - a) + color.r * a) / 255);
                d.g = (unsigned char)((d.g * (255 - a) + color.g * a) / 255);
                d.b = (unsigned char)((d.b * (255 - a) + color.b * a) / 255);
                s_fb[y1 * s_fb_w + x1] = color_to_rgb565(d);
            }
        }
        if (x1 == x2 && y1 == y2) break;
        e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x1 += sx; }
        if (e2 < dx) { err += dx; y1 += sy; }
    }
    dirty_add(x1 < x2 ? x1 : x2, y1 < y2 ? y1 : y2, abs(x2 - x1) + 1, abs(y2 - y1) + 1);
}

void backend_render_bezier_cubic(int x0, int y0, int cx1, int cy1, int cx2, int cy2,
                                 int x1, int y1, Color color, int width) {
    int i, steps = 64;
    int px, py;
    (void)width;
    px = x0; py = y0;
    for (i = 1; i <= steps; i++) {
        float t = (float)i / steps;
        float mt = 1 - t;
        float x = mt*mt*mt*x0 + 3*mt*mt*t*cx1 + 3*mt*t*t*cx2 + t*t*t*x1;
        float y = mt*mt*mt*y0 + 3*mt*mt*t*cy1 + 3*mt*t*t*cy2 + t*t*t*y1;
        backend_render_line(px, py, (int)(x + 0.5f), (int)(y + 0.5f), color);
        px = (int)(x + 0.5f); py = (int)(y + 0.5f);
    }
}

void backend_render_arc(int center_x, int center_y, int radius, float start_angle, float end_angle, Color color, int line_width) {
    float step = 0.1f;
    float prev_x = center_x + radius * cosf(start_angle);
    float prev_y = center_y + radius * sinf(start_angle);
    (void)line_width;
    for (float a = start_angle + step; a <= end_angle; a += step) {
        float x = center_x + radius * cosf(a);
        float y = center_y + radius * sinf(a);
        backend_render_line((int)prev_x, (int)prev_y, (int)x, (int)y, color);
        prev_x = x; prev_y = y;
    }
}

/* ====================== 圆角/阴影/渐变 ====================== */
void backend_render_rounded_rect(Rect* rect, Color color, int radius) {
    (void)radius;
    backend_render_fill_rect(rect, color);
}
void backend_render_rounded_rect_color(Rect* rect, unsigned char r, unsigned char g, unsigned char b, unsigned char a, int radius) {
    backend_render_rounded_rect(rect, (Color){r, g, b, a}, radius);
}
void backend_render_rounded_rect_with_border(Rect* rect, Color bg, int radius, int bw, Color bc) {
    Rect t;
    backend_render_fill_rect(rect, bg);
    t = *rect; t.h = bw; backend_render_fill_rect(&t, bc);
    t = *rect; t.y = rect->y + rect->h - bw; t.h = bw; backend_render_fill_rect(&t, bc);
    t = *rect; t.w = bw; backend_render_fill_rect(&t, bc);
    t = *rect; t.x = rect->x + rect->w - bw; t.w = bw; backend_render_fill_rect(&t, bc);
    (void)radius;
}
void backend_render_shadow(const Rect* rect, int radius, int ox, int oy, int blur, int spread, Color color) {
    Rect r;
    (void)radius; (void)blur;
    if (!rect || color.a == 0) return;
    r = *rect;
    r.x += ox - spread; r.y += oy - spread;
    r.w += spread * 2; r.h += spread * 2;
    if (r.w > 0 && r.h > 0) backend_render_fill_rect(&r, color);
}
void backend_render_rounded_gradient(const Rect* rect, int radius, int vertical, const Color* colors, int count) {
    (void)radius; (void)vertical; (void)count;
    if (!rect || !colors) return;
    backend_render_fill_rect((Rect*)rect, colors[0]);
}
void backend_render_backdrop_filter(Rect* rect, int blur_radius, float saturation, float brightness) {
    (void)rect; (void)blur_radius; (void)saturation; (void)brightness;
}

/* ====================== 纹理 blit ====================== */
void backend_render_text_copy(Texture* texture, const Rect* srcrect, const Rect* dstrect) {
    unsigned char* src;
    Rect clip, dst, src_r;
    int x, y, sw, sh;
    if (!texture || !s_fb || !dstrect) return;
    src = embed_font_texture_pixels(texture);
    if (!src) return;
    sw = texture->w; sh = texture->h;
    src_r.x = srcrect ? srcrect->x : 0;
    src_r.y = srcrect ? srcrect->y : 0;
    src_r.w = srcrect ? srcrect->w : sw;
    src_r.h = srcrect ? srcrect->h : sh;
    clip_get_current(&clip);
    dst = *dstrect;
    clip_intersect(&dst, &dst, &clip);
    if (dst.w <= 0 || dst.h <= 0) return;

    for (y = 0; y < dst.h; y++) {
        int sy = src_r.y + (y * src_r.h) / (dstrect->h > 0 ? dstrect->h : 1);
        if (sy < 0 || sy >= sh) continue;
        for (x = 0; x < dst.w; x++) {
            int sx = src_r.x + (x * src_r.w) / (dstrect->w > 0 ? dstrect->w : 1);
            size_t si;
            unsigned a;
            if (sx < 0 || sx >= sw) continue;
            si = ((size_t)sy * sw + sx) * 4;
            a = src[si + 3];
            if (a == 0) continue;
            if (a == 255) {
                s_fb[(dst.y + y) * s_fb_w + (dst.x + x)] =
                    color_to_rgb565((Color){src[si], src[si+1], src[si+2], 255});
            } else {
                Color dc = rgb565_to_color(s_fb[(dst.y + y) * s_fb_w + (dst.x + x)]);
                dc.r = (unsigned char)((dc.r * (255 - a) + src[si] * a) / 255);
                dc.g = (unsigned char)((dc.g * (255 - a) + src[si+1] * a) / 255);
                dc.b = (unsigned char)((dc.b * (255 - a) + src[si+2] * a) / 255);
                s_fb[(dst.y + y) * s_fb_w + (dst.x + x)] = color_to_rgb565(dc);
            }
        }
    }
    dirty_add(dst.x, dst.y, dst.w, dst.h);
}

void backend_render_texture_tinted(Texture* texture, const Rect* srcrect, const Rect* dstrect, Color tint) {
    (void)tint;
    backend_render_text_copy(texture, srcrect, dstrect);
}

/* ====================== 裁剪 ====================== */
void backend_render_get_clip_rect(Rect* prev) {
    if (prev) clip_get_current(prev);
}
void backend_render_set_clip_rect(Rect* clip) {
    if (!clip) {
        s_clip_depth = 0;
        return;
    }
    if (s_clip_depth < CLIP_MAX) {
        Rect cur;
        clip_get_current(&cur);
        clip_intersect(&s_clip_stack[s_clip_depth], &cur, clip);
        s_clip_depth++;
    }
}

/* ====================== 字体 ====================== */
DFont* backend_load_font(char* path, int size) {
    return embed_font_load(path, size);
}
DFont* backend_load_font_with_weight(char* path, int size, const char* weight) {
    return embed_font_load_with_weight(path, size, weight);
}

/* ====================== 窗口/时间 ====================== */
void backend_get_windowsize(int* w, int* h) {
    if (w) *w = s_fb_w;
    if (h) *h = s_fb_h;
}
void backend_set_windowsize(int w, int h) { (void)w; (void)h; }
void backend_set_window_title(char* t) { (void)t; }
void backend_set_resizable(int r) { (void)r; }
void backend_set_minimum_windowsize(int w, int h) { (void)w; (void)h; }

Uint32 backend_get_ticks(void) {
#ifdef ESP_PLATFORM
    return esp32_ticks();
#else
    return (Uint32)0;
#endif
}

void backend_get_pointer_state(int* x, int* y) {
#ifdef ESP_PLATFORM
    if (x) *x = s_touch_x;
    if (y) *y = s_touch_y;
#else
    if (x) *x = 0;
    if (y) *y = 0;
#endif
}

void backend_delay(int ms) {
#ifdef ESP_PLATFORM
    vTaskDelay(pdMS_TO_TICKS(ms));
#else
    (void)ms;
#endif
}

void backend_render_present(void) {
#ifdef ESP_PLATFORM
    esp32_flush_dirty();
#else
    dirty_reset();
#endif
}

/* ====================== 回调 ====================== */
#define MAX_UPDATE_CB 8
static UpdateCallback s_update_cb[MAX_UPDATE_CB];
static int s_update_cb_count = 0;
void backend_register_update_callback(UpdateCallback cb) {
    if (cb && s_update_cb_count < MAX_UPDATE_CB) s_update_cb[s_update_cb_count++] = cb;
}
void backend_set_resize_callback(ResizeCallback cb) { (void)cb; }

/* ====================== 查询/剪贴板/IME/图标 ====================== */
int backend_query_texture(Texture* tex, Uint32* format, int* access, int* w, int* h) {
    if (!tex) return -1;
    if (format) *format = 0;
    if (access) *access = 0;
    if (w) *w = tex->w;
    if (h) *h = tex->h;
    return 0;
}
char* backend_get_clipboard_text(void) { return NULL; }
void backend_set_clipboard_text(const char* t) { (void)t; }
void backend_start_text_input(void) {}
void backend_stop_text_input(void) {}
void backend_set_text_input_rect(Rect* r) { (void)r; }
void backend_set_titlebar_color(Color bg, Color t) { (void)bg; (void)t; }
void backend_set_window_icon(const char* p) { (void)p; }
void backend_set_font_fallback_path(const char* p) { (void)p; }
int backend_has_font_fallback(void) { return 0; }
void backend_texture_cache_invalidate(void) {}
void backend_texture_cache_pin(DFont* f, const char* t, Color c) { (void)f; (void)t; (void)c; }
void backend_texture_cache_warmup(DFont* f, const char** t, int n, Color c) { (void)f; (void)t; (void)n; (void)c; }
int backend_screenshot(const char* p) { (void)p; return -1; }
Layer* g_ui_root = NULL;
void backend_set_ui_root(Layer* root) { g_ui_root = root; }

/* ====================== 主循环 ====================== */
static Layer* s_ui_root = NULL;

void backend_tick(Layer* ui_root) {
    int i;
#ifdef ESP_PLATFORM
    PointerEvent ev;
    int has_event = 0;
    esp32_touch_poll(&ev, &has_event);
    if (has_event) handle_pointer_event(ui_root, &ev);
#endif
    for (i = 0; i < s_update_cb_count; i++) {
        if (s_update_cb[i]) s_update_cb[i]();
    }
    backend_render_clear_color(20, 20, 20, 255);
    if (ui_root && ui_root->render) ui_root->render(ui_root);
    backend_render_present();
}

void backend_run(Layer* ui_root) {
    s_ui_root = ui_root;
    if (!ui_root) return;
    while (!s_should_quit) {
        backend_tick(ui_root);
        s_frame_count++;
        if (s_auto_frames >= 0 && s_frame_count >= s_auto_frames) break;
#ifdef ESP_PLATFORM
        vTaskDelay(pdMS_TO_TICKS(16));
#endif
    }
}
