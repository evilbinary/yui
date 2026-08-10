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
#include "esp_idf_version.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch.h"
#ifdef YUI_ESP32_QEMU
#include "esp_lcd_qemu_rgb.h"
#endif
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_partition.h"
#include "esp_heap_caps.h"
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

/* 0 = 仅软件 framebuffer（QEMU / 无屏）；1 = 初始化 SPI LCD + 触摸 */
static int s_hw_display = 1;
void backend_esp32_set_hw_display(int on) { s_hw_display = on ? 1 : 0; }
int backend_esp32_get_hw_display(void) { return s_hw_display; }

#ifdef ESP_PLATFORM
static esp_partition_mmap_handle_t s_font_mmap_handle;
/* 缓存已映射的字体分区数据，避免每次 load 重新 mmap（重复 mmap 会累积句柄） */
static const void* s_font_mapped = NULL;
static size_t s_font_mapped_size = 0;

static esp_partition_mmap_handle_t s_bcrom_mmap_handle;
static const void* s_bcrom_mapped = NULL;
static size_t s_bcrom_mapped_size = 0;
#endif

/* 从 SPI Flash 分区映射 TTF（不占 RAM），适合子集化小字体。
 * partitions.csv 中需定义同名 data 分区，烧录子集化 TTF 到该分区。 */
DFont* backend_esp32_load_font_from_flash(const char* partition_label, int size) {
#ifdef ESP_PLATFORM
    if (!partition_label) return NULL;
    if (!s_font_mapped) {
        const esp_partition_t* part;
        const void* mapped;
        esp_err_t err;
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
        s_font_mapped = mapped;
        s_font_mapped_size = part->size;
        printf("YUI: font mmap addr=0x%08x size=%u (flash_offset=0x%x)\n",
               (unsigned)(uintptr_t)mapped, (unsigned)part->size,
               (unsigned)part->address);
    }
    return embed_font_load_from_memory(s_font_mapped, s_font_mapped_size, size, "normal");
#else
    (void)partition_label; (void)size;
    return NULL;
#endif
}

/* bcrom: 预编译 JS 字节码只读区（XIP）。返回 mmap 到的 flash 基址（不占 RAM）
 * 或 NULL。partition 在 partitions.csv 中定义，ROM 数据由 build_bcrom.py 生成。 */
const void* backend_esp32_bc_rom_base(size_t* psize) {
#ifdef ESP_PLATFORM
    if (!s_bcrom_mapped) {
        const esp_partition_t* part;
        const void* mapped;
        esp_err_t err;
        part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
                                        ESP_PARTITION_SUBTYPE_ANY, "bcrom");
        if (!part) {
            ESP_LOGE(YUI_E32_TAG, "bcrom partition not found");
            return NULL;
        }
        err = esp_partition_mmap(part, 0, part->size, ESP_PARTITION_MMAP_DATA,
                                 &mapped, &s_bcrom_mmap_handle);
        if (err != ESP_OK) {
            ESP_LOGE(YUI_E32_TAG, "bcrom mmap failed: %s", esp_err_to_name(err));
            return NULL;
        }
        s_bcrom_mapped = mapped;
        s_bcrom_mapped_size = part->size;
        printf("YUI: bcrom mmap addr=0x%08x size=%u (flash_offset=0x%x)\n",
               (unsigned)(uintptr_t)mapped, (unsigned)part->size,
               (unsigned)part->address);
    }
    if (psize) *psize = s_bcrom_mapped_size;
    return s_bcrom_mapped;
#else
    if (psize) *psize = 0;
    return NULL;
#endif
}

/* 编译期宏：是否启用 LCD 软件 framebuffer（RGB565，240x240 约 115KB）。
 *   - 默认 0（不启用）：不分配 s_fb，backend_render_* 不跳过，而是经
 *     direct_draw_point 逐点直写 LCD；无面板（QEMU/headless）时像素落点
 *     为空操作，但渲染调用链（组件渲染/字体光栅化/逐像素循环）完整执行。
 *     RAM 占用最小，适合真实 LCD 硬件与内存紧张场景。
 *   - 定义 YUI_ESP32_LCD_BUFFER=1 时分配 s_fb，backend_render_* 写入
 *     framebuffer，经脏矩形批量推送（esp32_flush_dirty）。
 *   - QEMU 构建（YUI_ESP32_QEMU）强制为 1，但 s_fb 不 calloc——指向虚拟
 *     RGB 面板的专属 framebuffer（不占内部 SRAM），见 backend_init。 */
#ifdef YUI_ESP32_QEMU
#ifndef YUI_ESP32_LCD_BUFFER
#define YUI_ESP32_LCD_BUFFER 1
#endif
#else
#ifndef YUI_ESP32_LCD_BUFFER
#define YUI_ESP32_LCD_BUFFER 0
#endif
#endif

/* QEMU：写专属 framebuffer（纯内存），经脏矩形 / 整帧 push 刷新 SDL 窗口。 */

static uint16_t* s_fb = NULL;       /* RGB565 */
static int s_fb_w = 0, s_fb_h = 0;
/* 分段 framebuffer：8BIT 堆最大连续块 ~114KB < 115200（240x240x2），
 * 无法一次分配整屏。改为按 40 行一段分配（每段 ~19.2KB），访问走
 * fb_row(y) 行指针；esp32_flush_dirty 按行 run 逐段推屏。 */
#define YUI_FB_SEG_ROWS 40
static uint16_t* s_fb_seg[6];
static int s_fb_seg_count = 0;
static inline uint16_t* fb_row(int y) {
    if (s_fb_seg_count > 0) {
        int seg = y / YUI_FB_SEG_ROWS;
        int off = (y % YUI_FB_SEG_ROWS) * s_fb_w;
        if (seg >= s_fb_seg_count) seg = s_fb_seg_count - 1;
        return s_fb_seg[seg] + off;
    }
    return s_fb + (size_t)y * s_fb_w;
}

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

/* ====================== 裁剪 ====================== */
static Rect s_clip;
static int s_clip_enabled = 0;

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
    if (s_clip_enabled) {
        *out = s_clip;
        return 1;
    }
    out->x = 0; out->y = 0; out->w = s_fb_w; out->h = s_fb_h;
    return 0;
}

/* ====================== ESP32 LCD/Touch ====================== */
#ifdef ESP_PLATFORM
static esp_lcd_panel_handle_t s_panel = NULL;
static esp_lcd_touch_handle_t s_touch = NULL;

/* 平台初始化（main.c）创建 LCD/触摸后注入到后端；后端渲染/轮询共用句柄。 */
void backend_esp32_set_panel(esp_lcd_panel_handle_t p) {
    s_panel = p;
    printf("YUI: [dbg] panel injected=%p\n", (void*)p);
}
void backend_esp32_set_touch_handle(esp_lcd_touch_handle_t t) { s_touch = t; }
/* 平台层（main.c）获取/分配 RGB565 framebuffer 后注入；后端只负责渲染写入。
 * fb 为 NULL 时用分段形式：随后调用 backend_esp32_set_framebuffer_seg 填入各段。 */
void backend_esp32_set_framebuffer(uint16_t* fb, int w, int h) {
    s_fb = fb;
    s_fb_w = w;
    s_fb_h = h;
    s_fb_seg_count = 0;
}
/* 分段 framebuffer：最多 6 段，每段 YUI_FB_SEG_ROWS 行。s_fb 置 NULL。 */
void backend_esp32_set_framebuffer_seg(uint16_t* seg0, uint16_t* seg1,
                                       uint16_t* seg2, uint16_t* seg3,
                                       uint16_t* seg4, uint16_t* seg5, int w, int h) {
    s_fb = NULL;
    s_fb_w = w;
    s_fb_h = h;
    s_fb_seg[0] = seg0;
    s_fb_seg[1] = seg1;
    s_fb_seg[2] = seg2;
    s_fb_seg[3] = seg3;
    s_fb_seg[4] = seg4;
    s_fb_seg[5] = seg5;
    s_fb_seg_count = 1;
    if (seg1) s_fb_seg_count++;
    if (seg2) s_fb_seg_count++;
    if (seg3) s_fb_seg_count++;
    if (seg4) s_fb_seg_count++;
    if (seg5) s_fb_seg_count++;
}

/* 可选"像素块直推"回调：真机改用裸 SPI 驱动 ST7789（esp_lcd 层不兼容该面板）
 * 时，由平台层注入，esp32_flush_dirty 优先走它而跳过 esp_lcd_panel_draw_bitmap。
 * 回调签名：(x, y, w, h, 源矩形左上角 RGB565 指针)。 */
typedef void (*esp32_blit_rect_fn)(int x, int y, int w, int h, const uint16_t* px);
static esp32_blit_rect_fn s_blit_rect = NULL;
static int64_t s_frame_written_px = 0;
void backend_esp32_set_blit_rect(esp32_blit_rect_fn fn) { s_blit_rect = fn; }

/* 像素块落屏统一入口：有 blit 回调（raw SPI 驱动）走回调，否则走 esp_lcd。 */
static void panel_blit(int x, int y, int w, int h, const uint16_t* buf) {
#ifndef YUI_ESP32_QEMU
    if (w > 0 && h > 0) {
        s_frame_written_px += (int64_t)w * h;
        if (s_blit_rect) {
            s_blit_rect(x, y, w, h, buf);
        } else if (s_panel) {
            esp_lcd_panel_draw_bitmap(s_panel, x, y, x + w, y + h, buf);
        }
        return;
    }
#endif
    if (s_blit_rect) {
        s_blit_rect(x, y, w, h, buf);
    } else if (s_panel) {
        esp_lcd_panel_draw_bitmap(s_panel, x, y, x + w, y + h, buf);
    }
}

static bool s_touch_active = false;
static int s_touch_x = 0, s_touch_y = 0;

static void esp32_flush_dirty(void) {
    if (!s_has_dirty) return;
    if (!s_fb && s_fb_seg_count == 0) { dirty_reset(); return; }
    /* 分段 framebuffer：按段边界拆分成行 run，保证每 run 内行连续可一次推。 */
    if (s_fb_seg_count > 0) {
        int y = s_dirty.y;
        int y_end = s_dirty.y + s_dirty.h;
        while (y < y_end) {
            int seg = y / YUI_FB_SEG_ROWS;
            int block_end = (seg + 1) * YUI_FB_SEG_ROWS;
            int run_h = (y_end < block_end ? y_end : block_end) - y;
            if (run_h > 0) {
                const uint16_t* px = fb_row(y) + s_dirty.x;
                if (s_blit_rect) {
                    s_blit_rect(s_dirty.x, y, s_dirty.w, run_h, px);
                } else if (s_panel) {
                    esp_lcd_panel_draw_bitmap(s_panel, s_dirty.x, y,
                                              s_dirty.x + s_dirty.w, y + run_h, px);
                }
            }
            y += run_h;
        }
        dirty_reset();
        return;
    }
    if (s_blit_rect) {
        s_blit_rect(s_dirty.x, s_dirty.y, s_dirty.w, s_dirty.h,
                    s_fb + s_dirty.y * s_fb_w + s_dirty.x);
    } else if (s_panel) {
        esp_lcd_panel_draw_bitmap(s_panel, s_dirty.x, s_dirty.y,
                                  s_dirty.x + s_dirty.w, s_dirty.y + s_dirty.h,
                                  s_fb + s_dirty.y * s_fb_w + s_dirty.x);
    }
    dirty_reset();
}

#ifdef YUI_ESP32_QEMU
/* QEMU 虚拟 RGB 面板寄存器布局（同 esp_lcd_qemu_rgb 组件的私有定义，
 * 这里本地复制一份以做带超时的推送，避免组件内无限忙等卡死）：
 *   0x00 version  0x04 size(高16=height,低16=width)
 *   0x08 update_from(高16=y,低16=x)  0x0c update_to
 *   0x10 update_content(像素源地址)  0x14 update_st(bit0=ena)
 *   0x18 bpp */
typedef volatile struct {
    uint32_t version;
    uint32_t size;
    uint32_t update_from;
    uint32_t update_to;
    uint32_t update_content;
    uint32_t update_st;
    uint32_t bpp;
} yui_qemu_rgb_dev_t;
#define YUI_QEMU_RGB_DEV ((volatile yui_qemu_rgb_dev_t*)0x21000000)

/* 整帧（或脏矩形）推送到虚拟屏。组件默认实现在 ena 上无限忙等，
 * 设备不复位时会把整个 guest 卡死；这里加超时保护并主动清零。 */
static void esp32_qemu_push_rect(int x0, int y0, int x1, int y1) {
    volatile yui_qemu_rgb_dev_t* dev = YUI_QEMU_RGB_DEV;
    int i;
    if (!s_fb) return;
    /* 设备尚在处理上一次推送（-nographic 无显示后端时 ena 永远不清）：
     * 跳过本次，避免每帧 100k 次轮询拖慢渲染。 */
    if (dev->update_st == 1) return;
    dev->update_from = (uint32_t)(((uint32_t)y0 << 16) | (uint32_t)(x0 & 0xffff));
    dev->update_to = (uint32_t)(((uint32_t)y1 << 16) | (uint32_t)(x1 & 0xffff));
    dev->update_content = (uint32_t)(uintptr_t)(s_fb + (size_t)y0 * s_fb_w + x0);
    dev->update_st = 1;
    for (i = 0; i < 100000 && dev->update_st == 1; i++) {
    }
    if (dev->update_st == 1) {
        static int warned = 0;
        dev->update_st = 0;
        if (!warned) {
            warned = 1;
            printf("YUI: qemu update busy timeout (display backend idle?)\n");
        }
    }
}
#endif /* YUI_ESP32_QEMU */

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

static YuiRenderMode s_render_mode = YUI_RENDER_MODE_DIRTY;

void backend_set_render_mode(YuiRenderMode mode) { s_render_mode = mode; }
YuiRenderMode backend_get_render_mode(void) { return s_render_mode; }

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

    /* framebuffer 由平台层（main.c）获取/分配并经 backend_esp32_set_framebuffer
     * 注入（含尺寸 s_fb_w/s_fb_h）；后端只做渲染写入，不负责平台相关的
     * 内存/面板获取。direct draw 模式（LCD_BUFFER=0 且非 QEMU）渲染直写
     * 面板，无软件 framebuffer。 */
#if defined(YUI_ESP32_QEMU) || YUI_ESP32_LCD_BUFFER
    if (!s_fb && s_fb_seg_count == 0) {
        /* 允许 framebuffer 延迟注入：真机在 UI 构建完成、进入主循环前
         * 才分配，避免 apps 加载阶段挤占内存。 */
        printf("YUI: framebuffer not injected yet (will be set before main loop)\n");
    }
#endif
    return 0;
}

#if YUI_ESP32_LCD_BUFFER
static void yui_aa_cache_teardown(void); /* 圆角 AA 覆盖率缓存回收（定义见文件后部） */
#endif

void backend_quit(void) {
#ifdef ESP_PLATFORM
    if (s_panel) {
        esp_lcd_panel_disp_on_off(s_panel, false);
    }
#endif
    /* framebuffer 归平台层（main.c）管理，backend 不负责释放 */
    s_fb = NULL;
    #if YUI_ESP32_LCD_BUFFER
    yui_aa_cache_teardown();
#endif
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
/* 无 framebuffer 模式（YUI_ESP32_LCD_BUFFER=0）的像素落点：
 *   - QEMU：写虚拟 RGB 面板的专属 framebuffer（纯内存写，快），
 *     每帧在 present 时整帧推送一次；
 *   - 真实 LCD：禁止逐点 1x1 SPI（240x240 会触发 task_wdt）；
 *     fill/blit 用行缓冲一次推一整行；
 *   - 无面板：空操作 —— 渲染调用链照常执行。 */
#if defined(ESP_PLATFORM) && !defined(YUI_ESP32_QEMU) && !YUI_ESP32_LCD_BUFFER
#define YUI_SPI_LINE_MAX 320
static uint16_t s_spi_line[YUI_SPI_LINE_MAX];
static uint16_t s_spi_block[YUI_SPI_LINE_MAX * 16];

/* fill 纯色大块：合并为 16 行块 SPI 事务，避免每行一次 setcol/ramwr 开销 */
static void spi_draw_block(int x, int y, int w, int h, uint16_t px)
{
    int row, i;
    if ((!s_blit_rect && !s_panel) || w <= 0 || h <= 0) return;
    if (w > YUI_SPI_LINE_MAX) w = YUI_SPI_LINE_MAX;
    for (i = 0; i < w * 16; i++) s_spi_block[i] = px;
    while (h > 0) {
        int hh = h > 16 ? 16 : h;
        panel_blit(x, y, w, hh, s_spi_block);
        y += hh; h -= hh;
    }
}
static void spi_draw_line(int x, int y, int w, uint16_t px)
{
    int i;
    if ((!s_blit_rect && !s_panel) || w <= 0) return;
    if (w > YUI_SPI_LINE_MAX) w = YUI_SPI_LINE_MAX;
    for (i = 0; i < w; i++) s_spi_line[i] = px;
    panel_blit(x, y, w, 1, s_spi_line);
}
static void spi_draw_line_buf(int x, int y, int w, const uint16_t* buf)
{
    if ((!s_blit_rect && !s_panel) || w <= 0 || !buf) return;
    if (w > YUI_SPI_LINE_MAX) w = YUI_SPI_LINE_MAX;
    panel_blit(x, y, w, 1, buf);
}
#endif

static void direct_draw_point(int x, int y, Color c)
{
#if YUI_ESP32_LCD_BUFFER
    (void)x; (void)y; (void)c;
#elif defined(YUI_ESP32_QEMU)
    if (!s_fb || x < 0 || y < 0 || x >= s_fb_w || y >= s_fb_h) return;
    s_fb[y * s_fb_w + x] = color_to_rgb565(c);
#elif defined(ESP_PLATFORM)
    /* 真实 LCD：单点 SPI 极慢，仅用于少量点绘；大面积走 spi_draw_line */
    uint16_t px;
    if (!s_blit_rect && !s_panel) return;
    px = color_to_rgb565(c);
    panel_blit(x, y, 1, 1, &px);
#else
    (void)x; (void)y; (void)c;
#endif
}

/* 抗锯齿单点落笔：有 framebuffer（YUI_ESP32_LCD_BUFFER / QEMU）时做 alpha 混合；
 * 直写模式（无 fb、无法读回）不使用（rounded_rect_fill 走批量行填充）。 */
#if YUI_ESP32_LCD_BUFFER
static void backend_draw_point_aa(int x, int y, Color c) {
    uint16_t* p;
    if ((!s_fb && s_fb_seg_count == 0) || x < 0 || y < 0 || x >= s_fb_w || y >= s_fb_h) return;
    p = fb_row(y) + x;
    if (c.a == 255) {
        *p = color_to_rgb565(c);
    } else if (c.a > 0) {
        Color d = rgb565_to_color(*p);
        unsigned a = c.a;
        d.r = (unsigned char)((d.r * (255 - a) + c.r * a) / 255);
        d.g = (unsigned char)((d.g * (255 - a) + c.g * a) / 255);
        d.b = (unsigned char)((d.b * (255 - a) + c.b * a) / 255);
        *p = color_to_rgb565(d);
    }
    dirty_add(x, y, 1, 1);
}
#endif /* YUI_ESP32_LCD_BUFFER */

void backend_render_fill_rect(Rect* rect, Color color) {
    Rect clip, r;
    int x, y;
    if (!rect) return;
    clip_get_current(&clip);
    r = *rect;
    clip_intersect(&r, &r, &clip);
    if (r.w <= 0 || r.h <= 0) return;

#if YUI_ESP32_LCD_BUFFER
    uint16_t px;
    if (!s_fb && s_fb_seg_count == 0) return;
    px = color_to_rgb565(color);
    /* alpha 混合 */
    if (color.a == 255) {
        for (y = r.y; y < r.y + r.h; y++) {
            uint16_t* row = fb_row(y) + r.x;
            for (x = 0; x < r.w; x++) row[x] = px;
        }
    } else if (color.a > 0) {
        unsigned a = color.a;
        for (y = r.y; y < r.y + r.h; y++) {
            uint16_t* row = fb_row(y) + r.x;
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
#elif defined(ESP_PLATFORM) && !defined(YUI_ESP32_QEMU)
    /* 16 行块 SPI 事务：一次 RAMWR 传多行，避免逐行 SPI 开销 */
    if (color.a == 0 || (!s_blit_rect && !s_panel)) return;
    {
        uint16_t px = color_to_rgb565(color);
        spi_draw_block(r.x, r.y, r.w, r.h, px);
    }
#else
    /* QEMU / stub：逐点（QEMU 写内存；无面板为空操作） */
    if (color.a == 0) return;
    for (y = r.y; y < r.y + r.h; y++) {
        for (x = r.x; x < r.x + r.w; x++) {
            direct_draw_point(x, y, color);
        }
    }
#endif
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
    full.x = 0; full.y = 0; full.w = s_fb_w; full.h = s_fb_h;
    backend_render_fill_rect(&full, (Color){r, g, b, a});
}

void backend_render_line(int x1, int y1, int x2, int y2, Color color) {
    int dx, dy, sx, sy, err, e2;
    int pts_cnt = 0;
    Rect clip;
#if YUI_ESP32_LCD_BUFFER
    if (!s_fb && s_fb_seg_count == 0) return;
#endif
    clip_get_current(&clip);
    dx = abs(x2 - x1); dy = abs(y2 - y1);
    sx = x1 < x2 ? 1 : -1; sy = y1 < y2 ? 1 : -1;
    err = dx - dy;
    while (1) {
        if (x1 >= clip.x && x1 < clip.x + clip.w && y1 >= clip.y && y1 < clip.y + clip.h) {
#if YUI_ESP32_LCD_BUFFER
            if (color.a == 255) {
                fb_row(y1)[x1] = color_to_rgb565(color);
            } else if (color.a > 0) {
                Color d = rgb565_to_color(fb_row(y1)[x1]);
                unsigned a = color.a;
                d.r = (unsigned char)((d.r * (255 - a) + color.r * a) / 255);
                d.g = (unsigned char)((d.g * (255 - a) + color.g * a) / 255);
                d.b = (unsigned char)((d.b * (255 - a) + color.b * a) / 255);
                fb_row(y1)[x1] = color_to_rgb565(d);
            }
#else
            if (color.a > 0) direct_draw_point(x1, y1, color);
            /* 真机直写是逐点 1x1 SPI，长线/圆弧会占满 CPU 不吃狗；
             * 每 64 点让出一次调度，使 IDLE 能喂看门狗。（vTaskDelay(0)=让出
             * 一个时间片；用 (1) 会每次睡 10ms 拖慢整帧。） */
            if (++pts_cnt >= 64) {
                pts_cnt = 0;
                vTaskDelay(0);
            }
#endif
        }
        if (x1 == x2 && y1 == y2) break;
        e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x1 += sx; }
        if (e2 < dx) { err += dx; y1 += sy; }
    }
#if YUI_ESP32_LCD_BUFFER
    dirty_add(x1 < x2 ? x1 : x2, y1 < y2 ? y1 : y2, abs(x2 - x1) + 1, abs(y2 - y1) + 1);
#endif
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

static int rounded_rect_radius(const Rect* rect, int radius) {
    int max_r = rect->w < rect->h ? rect->w / 2 : rect->h / 2;
    if (radius < 0) radius = max_r;      /* 负值 = 胶囊/全圆角 */
    if (radius > max_r) radius = max_r;
    if (radius < 0) radius = 0;
    return radius;
}

void backend_render_arc(int center_x, int center_y, int radius, float start_angle, float end_angle, Color color, int line_width) {
    /* ESP32-C3 无 FPU，软浮点 sin/cos 极慢。这里用运行时初始化一次的整数
     * sin 表（0.5°/档，14 位定点），每帧 arc 全程纯整数查表，无浮点。
     * step 0.5° 在 240x240 屏上足够密。 */
    static int16_t sin_tab[720];
    static int sin_tab_ready = 0;
    int step_idx = 1;  /* 0.5° */
    int half;
    int pts_cnt = 0;
    int wo_min, wo_max, wo;
    Rect clip;
    int a0, a1, a;
    if (radius <= 0 || color.a == 0) return;

    if (!sin_tab_ready) {
        int i;
        for (i = 0; i < 720; i++) {
            sin_tab[i] = (int16_t)(sinf((float)i * 0.5f * 3.14159265358979f / 180.0f) * (float)(1 << 14));
        }
        sin_tab_ready = 1;
    }
    /* 角度范围：float→ 0.5° 档索引（取整），处理 wrap */
    a0 = (int)start_angle * 2;
    a1 = (int)end_angle * 2;
    if (a0 > a1) { int t = a0; a0 = a1; a1 = t; }
    if (a0 < 0) a0 += 720;
    if (a1 < 0) a1 += 720;
    a0 %= 720; a1 %= 720;
    half = line_width / 2;
    wo_min = -half;
    wo_max = line_width - 1 - half;
    clip_get_current(&clip);
    for (a = a0; a <= a1; a += step_idx) {
        int s = sin_tab[a % 720];
        int c = sin_tab[(a + 180) % 720];   /* cos = sin(a+90°) */
        for (wo = wo_min; wo <= wo_max; wo++) {
            int rad = radius + wo;
            int x, y;
            if (rad <= 0) continue;
            x = center_x + (int)((int64_t)rad * c >> 14);
            y = center_y + (int)((int64_t)rad * s >> 14);
            if (x < clip.x || x >= clip.x + clip.w || y < clip.y || y >= clip.y + clip.h) continue;
#if YUI_ESP32_LCD_BUFFER
            if (s_fb || s_fb_seg_count > 0) {
                if (color.a == 255) {
                    fb_row(y)[x] = color_to_rgb565(color);
                } else if (color.a > 0) {
                    Color d = rgb565_to_color(fb_row(y)[x]);
                    unsigned a2 = color.a;
                    d.r = (unsigned char)((d.r * (255 - a2) + color.r * a2) / 255);
                    d.g = (unsigned char)((d.g * (255 - a2) + color.g * a2) / 255);
                    d.b = (unsigned char)((d.b * (255 - a2) + color.b * a2) / 255);
                    fb_row(y)[x] = color_to_rgb565(d);
                }
            }
#else
            if (color.a > 0) direct_draw_point(x, y, color);
            if (++pts_cnt >= 64) { pts_cnt = 0; vTaskDelay(0); }
#endif
        }
    }
}

#if YUI_ESP32_LCD_BUFFER
/* ============================================================
 * 圆角抗锯齿光栅化 —— 移植自 backend_sdl.c（同款算法）：
 * 用 4x 超采样的 Bresenham 圆预计算覆盖率表，对每个角落扫描线
 * 给出「自实心区向外」的每像素覆盖率（0..~240），再逐点 alpha 混合。
 * 仅在 framebuffer 路径（QEMU / buffer=1）编译启用。
 * ============================================================ */
#define YUI_AA_CACHE_SIZE 32

typedef struct {
    int radius;                     /* 0 = 空槽 */
    uint8_t* cir_opa;               /* 覆盖率流（每扫描线自内向外） */
    uint16_t* opa_start_on_y;       /* 各扫描线在 cir_opa 的起点（len = radius+2） */
    uint16_t* x_start_on_y;         /* 各扫描线的起点 x（len = radius+2） */
} YuiAA;

static YuiAA yui_aa_cache[YUI_AA_CACHE_SIZE];

static void yui_aa_free(YuiAA* c) {
    free(c->cir_opa);
    free(c->opa_start_on_y);
    free(c->x_start_on_y);
    memset(c, 0, sizeof(*c));
}

static void yui_aa_cache_teardown(void) {
    for (int i = 0; i < YUI_AA_CACHE_SIZE; i++) {
        if (yui_aa_cache[i].cir_opa) yui_aa_free(&yui_aa_cache[i]);
    }
}

static void yui_aa_circ_init(int* cx, int* cy, int* tmp, int radius) { *cx = radius; *cy = 0; *tmp = 1 - radius; }
static int yui_aa_circ_cont(int cx, int cy) { return cy <= cx; }
static void yui_aa_circ_next(int* cx, int* cy, int* tmp) {
    if (*tmp <= 0) {
        *tmp += 2 * (*cy) + 3;
    } else {
        *tmp += 2 * ((*cy) - (*cx)) + 5;
        (*cx)--;
    }
    (*cy)++;
}

static void yui_aa_circle_build(YuiAA* c, int radius) {
    int* cir_x;
    int* cir_y;
    int cir_size = 0;
    int cp_x, cp_y, tmp, i;
    if (c->cir_opa) { free(c->cir_opa); c->cir_opa = NULL; }
    if (radius <= 0) { c->radius = 0; return; }
    c->radius = radius;
    c->cir_opa = (uint8_t*)malloc((size_t)radius * 2 + 2);
    c->opa_start_on_y = (uint16_t*)calloc((size_t)radius + 2, 2);
    c->x_start_on_y = (uint16_t*)calloc((size_t)radius + 2, 2);
    if (!c->cir_opa || !c->opa_start_on_y || !c->x_start_on_y) { yui_aa_free(c); return; }
    if (radius == 1) {
        c->cir_opa[0] = 180;
        c->opa_start_on_y[0] = 0;
        c->opa_start_on_y[1] = 1;
        c->x_start_on_y[0] = 0;
        return;
    }
    cir_x = (int*)malloc((size_t)(radius + 1) * 4 * sizeof(int));
    cir_y = cir_x + (radius + 1) * 2;
    if (!cir_x) { yui_aa_free(c); return; }
    {
        const int cir_opa_max = radius * 2 + 2;
        int y_8th_cnt = 0;
        int x_int[4];
        int x_fract[4];
        yui_aa_circ_init(&cp_x, &cp_y, &tmp, radius * 4);
        x_int[0] = cp_x >> 2;
        x_fract[0] = 0;
        while (yui_aa_circ_cont(cp_x, cp_y)) {
            for (i = 0; i < 4; i++) {
                yui_aa_circ_next(&cp_x, &cp_y, &tmp);
                if (!yui_aa_circ_cont(cp_x, cp_y)) break;
                x_int[i] = cp_x >> 2;
                x_fract[i] = cp_x & 0x3;
            }
            if (i != 4) break;
#define YUI_AA_PUSH_CIR(px, py, opa) do { \
                if (cir_size >= cir_opa_max) { goto yui_aa_build_done; } \
                cir_x[cir_size] = (px); \
                cir_y[cir_size] = (py); \
                c->cir_opa[cir_size] = (uint8_t)(opa); \
                cir_size++; \
            } while (0)
            if (x_int[0] == x_int[3]) {
                YUI_AA_PUSH_CIR(x_int[0], y_8th_cnt, (x_fract[0] + x_fract[1] + x_fract[2] + x_fract[3]) * 16);
            } else if (x_int[0] != x_int[1]) {
                YUI_AA_PUSH_CIR(x_int[0], y_8th_cnt, x_fract[0] * 16);
                YUI_AA_PUSH_CIR(x_int[0] - 1, y_8th_cnt, (4 + x_fract[1] + x_fract[2] + x_fract[3]) * 16);
            } else if (x_int[0] != x_int[2]) {
                YUI_AA_PUSH_CIR(x_int[0], y_8th_cnt, (x_fract[0] + x_fract[1]) * 16);
                YUI_AA_PUSH_CIR(x_int[0] - 1, y_8th_cnt, (8 + x_fract[2] + x_fract[3]) * 16);
            } else {
                YUI_AA_PUSH_CIR(x_int[0], y_8th_cnt, (x_fract[0] + x_fract[1] + x_fract[2]) * 16);
                YUI_AA_PUSH_CIR(x_int[0] - 1, y_8th_cnt, (12 + x_fract[3]) * 16);
            }
            y_8th_cnt++;
        }
        {
            int mid = radius * 723;
            int mid_int = mid >> 10;
            if (cir_size == 0 || cir_x[cir_size - 1] != mid_int || cir_y[cir_size - 1] != mid_int) {
                int tmp_val = mid - (mid_int << 10);
                if (tmp_val <= 512) {
                    tmp_val = (tmp_val * tmp_val * 2) >> (10 + 6);
                } else {
                    tmp_val = 1024 - tmp_val;
                    tmp_val = (tmp_val * tmp_val * 2) >> (10 + 6);
                    tmp_val = 15 - tmp_val;
                }
                YUI_AA_PUSH_CIR(mid_int, mid_int, tmp_val * 16);
            }
        }
        for (i = cir_size - 2; i >= 0; i--, cir_size++) {
            if (cir_size >= cir_opa_max) break;
            cir_x[cir_size] = cir_y[i];
            cir_y[cir_size] = cir_x[i];
            c->cir_opa[cir_size] = c->cir_opa[i];
        }
yui_aa_build_done:
#undef YUI_AA_PUSH_CIR
        {
            int y = 0;
            i = 0;
            c->opa_start_on_y[0] = 0;
            while (i < cir_size && y <= radius) {
                c->opa_start_on_y[y] = (uint16_t)i;
                c->x_start_on_y[y] = (uint16_t)cir_x[i];
                for (; i < cir_size && cir_y[i] == y; i++) {
                    if (cir_x[i] < (int)c->x_start_on_y[y]) c->x_start_on_y[y] = (uint16_t)cir_x[i];
                }
                y++;
            }
            if (y <= radius) c->opa_start_on_y[y] = (uint16_t)cir_size;
        }
    }
    free(cir_x);
}

static YuiAA* yui_aa_circle_get(int radius) {
    int start, probe, slot, probes;
    if (radius <= 0) return NULL;
    start = radius % YUI_AA_CACHE_SIZE;
    probe = start; slot = -1; probes = 0;
    while (probes < YUI_AA_CACHE_SIZE) {
        if (yui_aa_cache[probe].radius == radius && yui_aa_cache[probe].cir_opa) return &yui_aa_cache[probe];
        if (yui_aa_cache[probe].radius == 0 && slot < 0) slot = probe;
        probe = (probe + 1) % YUI_AA_CACHE_SIZE;
        probes++;
    }
    if (slot < 0) slot = start;
    yui_aa_circle_build(&yui_aa_cache[slot], radius);
    return yui_aa_cache[slot].cir_opa ? &yui_aa_cache[slot] : NULL;
}

static void yui_aa_circle_get_line(const YuiAA* c, int cir_y, int* aa_len, int* x_start, uint8_t** aa_opa) {
    int r = c->radius;
    uint16_t s0, s1;
    if (cir_y < 0) cir_y = 0;
    if (cir_y >= r) cir_y = r - 1;
    s0 = c->opa_start_on_y[cir_y];
    s1 = c->opa_start_on_y[cir_y + 1];
    *aa_len = (int)(s1 - s0);
    *x_start = (int)c->x_start_on_y[cir_y];
    *aa_opa = &c->cir_opa[s0];
    if (*aa_len < 0) *aa_len = 0;
}

/* 画圆角矩形的一行扫描线（顶部/底部角帽区）：中间实心 + 两侧逐点抗锯齿 */
static void rounded_row_aa(int x, int py, int w, int r, int cir_y, const YuiAA* aa, Color color) {
    int aa_len, x_start, i;
    int cir_x_left, cir_x_right;
    uint8_t* aa_opa;
    yui_aa_circle_get_line(aa, cir_y, &aa_len, &x_start, &aa_opa);
    cir_x_left = x + r - x_start - 1;
    cir_x_right = x + w - r + x_start;
    if (cir_x_right > cir_x_left + 1) {
        Rect solid = {cir_x_left + 1, py, cir_x_right - cir_x_left - 1, 1};
        backend_render_fill_rect(&solid, color);
    }
    for (i = 0; i < aa_len; i++) {
        unsigned a = (color.a * aa_opa[aa_len - 1 - i]) / 255;
        Color e = color;
        if (a == 0) continue;
        e.a = (unsigned char)a;
        backend_draw_point_aa(cir_x_left - i, py, e);
        backend_draw_point_aa(cir_x_right + i, py, e);
    }
}
#endif /* YUI_ESP32_LCD_BUFFER */

/* 按行填充圆角矩形：中部全行实心，顶部/底部两帽按角样例抗锯齿。 */
static void rounded_rect_fill(Rect* rect, Color color, int radius) {
    int x = rect->x, y = rect->y, w = rect->w, h = rect->h;
    int r = rounded_rect_radius(rect, radius);
    int py;
    if (r <= 0) { backend_render_fill_rect(rect, color); return; }
#if YUI_ESP32_LCD_BUFFER
    /* 有 framebuffer（QEMU / buffer=1）时启用逐点抗锯齿。 */
    {
    const YuiAA* aa = yui_aa_circle_get(r);
    for (py = y; py < y + h; py++) {
        int local_y = py - y;
        int corner = -1;
        if (local_y < r) {
            corner = r - local_y - 1;
        } else if (local_y >= h - r) {
            corner = local_y - (h - r);
        }
        if (corner < 0) {
            Rect row = {x, py, w, 1};
            backend_render_fill_rect(&row, color);
        } else if (aa) {
            rounded_row_aa(x, py, w, r, corner, aa, color);
        } else {
            /* 覆盖率缓存失败：退回按行 inset 硬边填充 */
            int inset = (int)(r - sqrtf((float)r * r - (float)(corner + 1) * (corner + 1)));
            int run_w;
            if (inset < 0) inset = 0;
            run_w = w - 2 * inset;
            if (run_w > 0) {
                Rect row = {x + inset, py, run_w, 1};
                backend_render_fill_rect(&row, color);
            }
}
    }
}
#else
    /* 直写模式（真实 LCD 无 framebuffer，buffer=0）：逐点画抗锯齿边缘像素
     * 意味着每像素一次 1x1 SPI 写，会拖垮主任务触发 task WDT。
     * 退回按行 inset 的批量行填充（整行一次 SPI），保持稳定。 */
    for (py = y; py < y + h; py++) {
        int local_y = py - y;
        int corner = -1;
        if (local_y < r) {
            corner = r - local_y - 1;
        } else if (local_y >= h - r) {
            corner = local_y - (h - r);
        }
        if (corner < 0) {
            Rect row = {x, py, w, 1};
            backend_render_fill_rect(&row, color);
        } else {
            int inset = (int)(r - sqrtf((float)r * r - (float)(corner + 1) * (corner + 1)));
            int run_w;
            if (inset < 0) inset = 0;
            run_w = w - 2 * inset;
            if (run_w > 0) {
                Rect row = {x + inset, py, run_w, 1};
                backend_render_fill_rect(&row, color);
            }
        }
    }
#endif
}

/* ====================== 圆角/阴影/渐变 ====================== */
void backend_render_rounded_rect(Rect* rect, Color color, int radius) {
    if (!rect) return;
    if (color.a == 0) return;
    rounded_rect_fill(rect, color, radius);
}
void backend_render_rounded_rect_color(Rect* rect, unsigned char r, unsigned char g, unsigned char b, unsigned char a, int radius) {
    backend_render_rounded_rect(rect, (Color){r, g, b, a}, radius);
}
void backend_render_rounded_rect_with_border(Rect* rect, Color bg, int radius, int bw, Color bc) {
    Rect inner;
    int r_in;
    if (!rect) return;
    if (bw <= 0) { backend_render_rounded_rect(rect, bg, radius); return; }
    /* 先画外圆角 = 边框色，再叠内圆角 = 背景色，形成圆角描边 */
    rounded_rect_fill(rect, bc, radius);
    inner.x = rect->x + bw; inner.y = rect->y + bw;
    inner.w = rect->w - 2 * bw; inner.h = rect->h - 2 * bw;
    if (inner.w <= 0 || inner.h <= 0) return;
    r_in = radius < 0 ? -1 : (radius > bw ? radius - bw : 0);
    rounded_rect_fill(&inner, bg, r_in);
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
    int n, k, p, p1, p2;
    float t;
    if (!rect || !colors || count < 1) return;
    n = vertical ? rect->h : rect->w;
    if (n < 1) return;
    if (count == 1) { backend_render_rounded_rect((Rect*)rect, colors[0], radius); return; }
    for (k = 0; k < n; k++) {
        float f = count > 1 ? (float)k / (float)(n - 1) : 0.0f;
        if (f < 0) f = 0; else if (f > 1) f = 1;
        t = f * (count - 1);
        p = (int)t;
        if (p >= count - 1) { p = count - 2; t = 1.0f; } else { t -= p; }
        p1 = p; p2 = p + 1;
        {
            unsigned char cr = (unsigned char)(colors[p1].r + (colors[p2].r - colors[p1].r) * t);
            unsigned char cg = (unsigned char)(colors[p1].g + (colors[p2].g - colors[p1].g) * t);
            unsigned char cb = (unsigned char)(colors[p1].b + (colors[p2].b - colors[p1].b) * t);
            unsigned char ca = (unsigned char)(colors[p1].a + (colors[p2].a - colors[p1].a) * t);
            Rect row;
            if (vertical) {
                row.x = rect->x; row.y = rect->y + k; row.w = rect->w; row.h = 1;
            } else {
                row.x = rect->x + k; row.y = rect->y; row.w = 1; row.h = rect->h;
            }
            backend_render_rounded_rect(&row, (Color){cr, cg, cb, ca}, radius);
        }
    }
}
void backend_render_backdrop_filter(Rect* rect, int blur_radius, float saturation, float brightness) {
    (void)rect; (void)blur_radius; (void)saturation; (void)brightness;
}

/* ====================== 纹理 blit ====================== */
void backend_render_text_copy(Texture* texture, const Rect* srcrect, const Rect* dstrect) {
    unsigned char* src;
    Rect clip, dst, src_r;
    int x, y, sw, sh;
    if (!texture || !dstrect) return;
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

#if YUI_ESP32_LCD_BUFFER
    if (!s_fb && s_fb_seg_count == 0) return;
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
                fb_row(dst.y + y)[dst.x + x] =
                    color_to_rgb565((Color){src[si], src[si+1], src[si+2], 255});
            } else {
                Color dc = rgb565_to_color(fb_row(dst.y + y)[dst.x + x]);
                dc.r = (unsigned char)((dc.r * (255 - a) + src[si] * a) / 255);
                dc.g = (unsigned char)((dc.g * (255 - a) + src[si+1] * a) / 255);
                dc.b = (unsigned char)((dc.b * (255 - a) + src[si+2] * a) / 255);
                fb_row(dst.y + y)[dst.x + x] = color_to_rgb565(dc);
            }
        }
    }
    dirty_add(dst.x, dst.y, dst.w, dst.h);
#elif defined(ESP_PLATFORM) && !defined(YUI_ESP32_QEMU)
    /* 行缓冲 SPI：先合成一行 RGB565，再一次 draw_bitmap */
    if (!s_blit_rect && !s_panel) return;
    for (y = 0; y < dst.h; y++) {
        int sy = src_r.y + (y * src_r.h) / (dstrect->h > 0 ? dstrect->h : 1);
        int run_x0 = -1, run_n = 0;
        if (sy < 0 || sy >= sh) continue;
        for (x = 0; x < dst.w; x++) {
            int sx = src_r.x + (x * src_r.w) / (dstrect->w > 0 ? dstrect->w : 1);
            size_t si;
            unsigned a;
            if (sx < 0 || sx >= sw) {
                if (run_n > 0) {
                    spi_draw_line_buf(dst.x + run_x0, dst.y + y, run_n, s_spi_line);
                    run_n = 0; run_x0 = -1;
                }
                continue;
            }
            si = ((size_t)sy * sw + sx) * 4;
            a = src[si + 3];
            if (a == 0) {
                if (run_n > 0) {
                    spi_draw_line_buf(dst.x + run_x0, dst.y + y, run_n, s_spi_line);
                    run_n = 0; run_x0 = -1;
                }
                continue;
            }
            if (run_x0 < 0) run_x0 = x;
            if (run_n < YUI_SPI_LINE_MAX) {
                s_spi_line[run_n++] = color_to_rgb565(
                    (Color){src[si], src[si + 1], src[si + 2], 255});
            }
        }
        if (run_n > 0) {
            spi_draw_line_buf(dst.x + run_x0, dst.y + y, run_n, s_spi_line);
        }
        if ((y & 15) == 15) vTaskDelay(0);
    }
#else
    /* 直接写屏：逐点绘制（无混合读回，仅按 alpha 跳过透明像素） */
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
            direct_draw_point(dst.x + x, dst.y + y,
                              (Color){src[si], src[si+1], src[si+2], 255});
        }
    }
#endif
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
    if (!clip || clip->w <= 0 || clip->h <= 0) {
        s_clip_enabled = 0;
        return;
    }
    s_clip = *clip;
    s_clip_enabled = 1;
}

/* ====================== 字体 ====================== */
DFont* backend_load_font(char* path, int size) {
    DFont* font = embed_font_load(path, size);
    if (!font) {
        /* 文件不存在（字体只在 flash font 分区）：回退到分区字体 */
        font = backend_esp32_load_font_from_flash("font", size);
    }
    return font;
}
DFont* backend_load_font_with_weight(char* path, int size, const char* weight) {
    DFont* font = embed_font_load_with_weight(path, size, weight);
    if (!font) {
        /* 文件不存在（字体只在 flash font 分区）：回退到分区字体 */
        font = backend_esp32_load_font_from_flash("font", size);
    }
    return font;
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
#ifdef YUI_ESP32_QEMU
    /* QEMU：直写模式（buffer=0）无脏矩形跟踪，每帧整帧推送。
     * 官方 esp_lcd_rgb_qemu_refresh 内部对 ena 无限忙等（esp_lcd_qemu_rgb.c），
     * 无头模式（-nographic 无 SDL 显示后端）时 ena 永不清零会把 guest 卡死；
     * 用带超时的 esp32_qemu_push_rect 替代。 */
    if (!s_has_dirty) {
        esp32_qemu_push_rect(0, 0, s_fb_w, s_fb_h);
    } else {
        esp32_qemu_push_rect(s_dirty.x, s_dirty.y,
                             s_dirty.x + s_dirty.w, s_dirty.y + s_dirty.h);
        dirty_reset();
    }
#else
    esp32_flush_dirty();
#endif
#else
    dirty_reset();
#endif
#ifdef ESP_PLATFORM
    if (s_frame_count < 3) {
        printf("YUI: [dbg] present frame=%d panel=%p fb=%p dirty=%d\n",
               s_frame_count, (void*)s_panel, (void*)s_fb, s_has_dirty);
    }
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
    int64_t t0 = esp_timer_get_time();
    esp32_touch_poll(&ev, &has_event);
    if (has_event) handle_pointer_event(ui_root, &ev);
#endif
    for (i = 0; i < s_update_cb_count; i++) {
        if (s_update_cb[i]) s_update_cb[i]();
    }
    // backend_render_clear_color(30, 60, 120, 255);
    if (s_frame_count == 0) {
        /* 根层透明 + 不每帧 clear：首帧必须把 GRAM 初始化成全黑，
         * 否则 LCD 未写入区域保持出厂灰/白，产生「灰黑交替刷屏」观感。 */
        Rect full;
        full.x = 0; full.y = 0; full.w = s_fb_w; full.h = s_fb_h;
        backend_render_fill_rect(&full, (Color){0, 0, 0, 255});
    }
    if (ui_root) render_layer(ui_root);
    { int64_t t_frame = esp_timer_get_time() - t0;
    popup_manager_render();
    backend_render_present();
    /* 仅绘制帧打印（静态帧 0ms 会刷屏 UART，跳过） */
    if (t_frame > 1000) {
        printf("YUI: frm=%lldms px=%lld\n",
               (long long)(t_frame / 1000), s_frame_written_px);
    }
    s_frame_written_px = 0; }
#ifdef YUI_ESP32_QEMU
    if (s_frame_count == 0) {
        /* 首帧调试：检查 framebuffer 是否被写入 */
        int i, nonzero = 0;
        for (i = 0; i < s_fb_w * s_fb_h; i++) {
            if (s_fb[i] != 0) { nonzero++; }
        }
        printf("YUI: fb debug: %d/%d nonzero pixels, fb[0]=0x%04x fb[100]=0x%04x\n",
               nonzero, s_fb_w * s_fb_h, s_fb[0], s_fb[100]);
    }
    if ((s_frame_count % 10) == 0) printf("YUI: frame %d done\n", s_frame_count);
    if (s_frame_count == 2 && s_fb && s_fb_w > 0) {
        /* temp: confirm emoji render size on QEMU */
        static int shown = 0;
        if (!shown) { shown = 1;
        printf("frame2 emoji sample fb[100000]=0x%04x fb[200000]=0x%04x\n", s_fb[100000], s_fb[200000]);
        }
    }
#else 
    (void)t0;
#endif
}

void backend_run(Layer* ui_root) {
    s_ui_root = ui_root;
    if (!ui_root) return;
#ifdef ESP_PLATFORM
    printf("YUI: backend_run start (panel=%s buffer=%d)\n",
           s_panel ? "yes" : "no", YUI_ESP32_LCD_BUFFER);
#endif
    while (!s_should_quit) {
        backend_tick(ui_root);
        s_frame_count++;
        if (s_auto_frames >= 0 && s_frame_count >= s_auto_frames) break;
#ifdef ESP_PLATFORM
        vTaskDelay(pdMS_TO_TICKS(16));
#endif
    }
}
