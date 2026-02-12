#include "backend.h"
#include "event.h"
#include "render.h"
#include "ytype.h"
#include "popup_manager.h"
#include <stdbool.h>
#include <math.h>

// STM32 平台特定头文件
#ifdef STM32F7xx
#include "stm32f7xx.h"
#include "stm32f7xx_hal.h"
#include "stm32f7xx_hal_ltdc.h"
#include "stm32f7xx_hal_dma2d.h"
#include "stm32f7xx_hal_sdram.h"
#include "ltdc.h"
#include "dma2d.h"
#include "sdram.h"
#include "touch.h"
#elif defined(STM32F4xx)
#include "stm32f4xx.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_ltdc.h"
#include "stm32f4xx_hal_dma2d.h"
#include "stm32f4xx_hal_sdram.h"
#include "ltdc.h"
#include "dma2d.h"
#include "sdram.h"
#include "touch.h"
#endif

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 480
#define MAX_TOUCHES 5
#define MAX_UPDATE_CALLBACKS 8

// ====================== 全局渲染器 ======================
extern LTDC_HandleTypeDef hltdc;
extern DMA2D_HandleTypeDef hdma2d;

float scale = 1.0;
DFont* default_font = NULL;
static uint32_t* framebuffer = NULL;

// 主循环更新回调管理
static UpdateCallback update_callbacks[MAX_UPDATE_CALLBACKS] = {NULL};
static int update_callback_count = 0;

// 显示缓冲区
static uint32_t display_buffer[LCD_WIDTH * LCD_HEIGHT];
static bool display_needs_update = false;

// 触摸状态
typedef struct {
    bool active;
    int x;
    int y;
    int last_x;
    int last_y;
} TouchState;

static TouchState touch_states[MAX_TOUCHES] = {0};

// ====================== 初始化函数 ======================
int backend_init() {
    // 初始化显示缓冲区
    framebuffer = (uint32_t*)SDRAM_BANK_ADDR; // 使用外部SDRAM作为帧缓冲区
    
    // 清空显示缓冲区
    memset(framebuffer, 0, LCD_WIDTH * LCD_HEIGHT * 4);
    
    // 初始化触摸屏
    touch_init();
    
    return 0;
}

void backend_quit() {
    // 清理资源
    framebuffer = NULL;
    default_font = NULL;
}

// ====================== 纹理和渲染函数 ======================
Texture* backend_load_texture(char* path) {
    // STM32 平台实现 - 从文件系统加载图片
    // 这里需要根据具体文件系统实现
    return NULL;
}

Texture* backend_load_texture_from_base64(const char* base64_data, size_t data_len) {
    // STM32 平台实现 - 从base64数据解码图片
    // 这里需要实现base64解码和图片解析
    return NULL;
}

Texture* backend_render_texture(DFont* font, const char* text, Color color) {
    // STM32 平台实现 - 渲染文本为纹理
    // 这里需要实现字体渲染
    Texture* texture = (Texture*)malloc(sizeof(Texture));
    if (!texture) return NULL;
    
    // 计算文本大小
    int text_width = strlen(text) * font->size / 2; // 简单估算
    int text_height = font->size;
    
    texture->w = text_width;
    texture->h = text_height;
    texture->data = malloc(text_width * text_height * 4);
    
    if (!texture->data) {
        free(texture);
        return NULL;
    }
    
    // 渲染文本到纹理数据
    // 这里需要实现具体的文本渲染逻辑
    memset(texture->data, 0, text_width * text_height * 4);
    
    return texture;
}

void backend_render_fill_rect(Rect* rect, Color color) {
    if (!framebuffer || !rect) return;
    
    int x = rect->x;
    int y = rect->y;
    int w = rect->w;
    int h = rect->h;
    
    // 边界检查
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > LCD_WIDTH) w = LCD_WIDTH - x;
    if (y + h > LCD_HEIGHT) h = LCD_HEIGHT - y;
    if (w <= 0 || h <= 0) return;
    
    // 转换颜色格式 (ARGB8888)
    uint32_t color_value = ((uint32_t)color.a << 24) | 
                          ((uint32_t)color.r << 16) | 
                          ((uint32_t)color.g << 8) | 
                          (uint32_t)color.b;
    
    // 填充矩形
    for (int j = y; j < y + h; j++) {
        for (int i = x; i < x + w; i++) {
            framebuffer[j * LCD_WIDTH + i] = color_value;
        }
    }
    
    display_needs_update = true;
}

void backend_render_rect(Rect* rect, Color color) {
    if (!framebuffer || !rect) return;
    
    int x = rect->x;
    int y = rect->y;
    int w = rect->w;
    int h = rect->h;
    
    // 绘制边框
    Rect top = {x, y, w, 1};
    Rect bottom = {x, y + h - 1, w, 1};
    Rect left = {x, y, 1, h};
    Rect right = {x + w - 1, y, 1, h};
    
    backend_render_fill_rect(&top, color);
    backend_render_fill_rect(&bottom, color);
    backend_render_fill_rect(&left, color);
    backend_render_fill_rect(&right, color);
}

void backend_render_rect_color(Rect* rect, unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    Color color = {r, g, b, a};
    backend_render_rect(rect, color);
}

void backend_render_fill_rect_color(Rect* rect, unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    Color color = {r, g, b, a};
    backend_render_fill_rect(rect, color);
}

void backend_render_rounded_rect(Rect* rect, Color color, int radius) {
    if (!framebuffer || !rect) return;
    
    // 简化实现：先绘制普通矩形，后续可优化为真正的圆角矩形
    backend_render_fill_rect(rect, color);
}

void backend_render_rounded_rect_color(Rect* rect, unsigned char r, unsigned char g, unsigned char b, unsigned char a, int radius) {
    Color color = {r, g, b, a};
    backend_render_rounded_rect(rect, color, radius);
}

void backend_render_rounded_rect_with_border(Rect* rect, Color bg_color, int radius, int border_width, Color border_color) {
    if (!framebuffer || !rect) return;
    
    // 简化实现：先绘制背景，再绘制边框
    backend_render_fill_rect(rect, bg_color);
    
    Rect border_rect = {rect->x, rect->y, rect->w, border_width};
    backend_render_fill_rect(&border_rect, border_color);
    
    border_rect.y = rect->y + rect->h - border_width;
    backend_render_fill_rect(&border_rect, border_color);
    
    border_rect.x = rect->x;
    border_rect.y = rect->y;
    border_rect.w = border_width;
    border_rect.h = rect->h;
    backend_render_fill_rect(&border_rect, border_color);
    
    border_rect.x = rect->x + rect->w - border_width;
    backend_render_fill_rect(&border_rect, border_color);
}

void backend_render_line(int x1, int y1, int x2, int y2, Color color) {
    if (!framebuffer) return;
    
    // Bresenham直线算法
    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy;
    
    int x = x1;
    int y = y1;
    
    // 边界检查
    if (x < 0 || x >= LCD_WIDTH || y < 0 || y >= LCD_HEIGHT) return;
    
    // 转换颜色格式
    uint32_t color_value = ((uint32_t)color.a << 24) | 
                          ((uint32_t)color.r << 16) | 
                          ((uint32_t)color.g << 8) | 
                          (uint32_t)color.b;
    
    while (1) {
        // 边界检查
        if (x >= 0 && x < LCD_WIDTH && y >= 0 && y < LCD_HEIGHT) {
            framebuffer[y * LCD_WIDTH + x] = color_value;
        }
        
        if (x == x2 && y == y2) break;
        
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x += sx;
        }
        if (e2 < dx) {
            err += dx;
            y += sy;
        }
    }
    
    display_needs_update = true;
}

void backend_render_arc(int center_x, int center_y, int radius, float start_angle, float end_angle, Color color, int line_width) {
    // 简化实现：后续可优化为真正的圆弧
    if (!framebuffer || radius <= 0) return;
    
    float step = 0.1; // 步长
    float prev_x = center_x + radius * cos(start_angle);
    float prev_y = center_y + radius * sin(start_angle);
    
    for (float angle = start_angle + step; angle <= end_angle; angle += step) {
        float x = center_x + radius * cos(angle);
        float y = center_y + radius * sin(angle);
        backend_render_line(prev_x, prev_y, x, y, color);
        prev_x = x;
        prev_y = y;
    }
}

void backend_render_backdrop_filter(Rect* rect, int blur_radius, float saturation, float brightness) {
    // 简化实现：毛玻璃效果需要更复杂的图像处理，这里先保持原样
    // 后续可优化为真正的毛玻璃效果
}

// ====================== 窗口管理函数 ======================
void backend_get_windowsize(int* width, int* height) {
    if (width) *width = LCD_WIDTH;
    if (height) *height = LCD_HEIGHT;
}

void backend_set_windowsize(int width, int height) {
    // STM32 LCD 分辨率固定，此功能不适用
}

void backend_set_window_size(char* title) {
    // STM32 没有窗口标题概念，此功能不适用
}

void backend_render_present() {
    if (!framebuffer || !display_needs_update) return;
    
    // 使用DMA2D将帧缓冲区内容传输到LCD
    HAL_DMA2D_Start(&hdma2d, (uint32_t)framebuffer, (uint32_t)LCD_FRAME_BUFFER, LCD_WIDTH, LCD_HEIGHT);
    HAL_DMA2D_PollForTransfer(&hdma2d, 100);
    
    display_needs_update = false;
}

void backend_delay(int delay) {
    HAL_Delay(delay);
}

void backend_render_clear_color(unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    if (!framebuffer) return;
    
    // 转换颜色格式
    uint32_t color_value = ((uint32_t)a << 24) | 
                          ((uint32_t)r << 16) | 
                          ((uint32_t)g << 8) | 
                          (uint32_t)b;
    
    // 清空整个帧缓冲区
    for (int i = 0; i < LCD_WIDTH * LCD_HEIGHT; i++) {
        framebuffer[i] = color_value;
    }
    
    display_needs_update = true;
}

// ====================== 字体管理函数 ======================
DFont* backend_load_font(char* font_path, int size) {
    // STM32 平台实现 - 从文件系统加载字体
    DFont* font = (DFont*)malloc(sizeof(DFont));
    if (!font) return NULL;
    
    font->size = size;
    font->path = strdup(font_path);
    font->weight = "normal";
    
    // 这里需要根据具体字体格式实现字体加载
    // 可以使用FreeType等库
    
    return font;
}

DFont* backend_load_font_with_weight(char* font_path, int size, const char* weight) {
    DFont* font = backend_load_font(font_path, size);
    if (font) {
        font->weight = strdup(weight);
    }
    return font;
}

void backend_render_text_destroy(Texture* texture) {
    if (texture) {
        if (texture->data) {
            free(texture->data);
        }
        free(texture);
    }
}

void backend_render_text_copy(Texture* texture, const Rect* srcrect, const Rect* dstrect) {
    if (!texture || !framebuffer || !srcrect || !dstrect) return;
    
    // 简化实现：复制纹理到帧缓冲区
    // 这里需要实现具体的纹理复制逻辑
    if (texture->data) {
        uint32_t* src = (uint32_t*)texture->data;
        
        for (int y = 0; y < dstrect->h && y < srcrect->h; y++) {
            for (int x = 0; x < dstrect->w && x < srcrect->w; x++) {
                int src_x = srcrect->x + x;
                int src_y = srcrect->y + y;
                int dst_x = dstrect->x + x;
                int dst_y = dstrect->y + y;
                
                if (dst_x >= 0 && dst_x < LCD_WIDTH && dst_y >= 0 && dst_y < LCD_HEIGHT &&
                    src_x >= 0 && src_x < texture->w && src_y >= 0 && src_y < texture->h) {
                    framebuffer[dst_y * LCD_WIDTH + dst_x] = src[src_y * texture->w + src_x];
                }
            }
        }
        
        display_needs_update = true;
    }
}

// ====================== 裁剪区域函数 ======================
void backend_render_get_clip_rect(Rect* prev_clip) {
    // STM32 平台实现：获取当前裁剪区域
    // 这里需要维护裁剪状态
    if (prev_clip) {
        prev_clip->x = 0;
        prev_clip->y = 0;
        prev_clip->w = LCD_WIDTH;
        prev_clip->h = LCD_HEIGHT;
    }
}

void backend_render_set_clip_rect(Rect* clip) {
    // STM32 平台实现：设置裁剪区域
    // 这里需要实现裁剪逻辑
}

// ====================== 主循环函数 ======================
void backend_register_update_callback(UpdateCallback callback) {
    if (update_callback_count < MAX_UPDATE_CALLBACKS) {
        update_callbacks[update_callback_count++] = callback;
    }
}

void backend_run(Layer* ui_root) {
    if (!ui_root) return;
    
    // 主循环
    while (1) {
        // 处理触摸事件
        for (int i = 0; i < MAX_TOUCHES; i++) {
            touch_states[i].last_x = touch_states[i].x;
            touch_states[i].last_y = touch_states[i].y;
        }
        
        touch_update();
        
        // 检测触摸状态变化
        for (int i = 0; i < MAX_TOUCHES; i++) {
            int x, y;
            bool active = touch_get_position(i, &x, &y);
            
            if (active) {
                if (!touch_states[i].active) {
                    // 新的触摸按下
                    touch_states[i].active = true;
                    touch_states[i].x = x;
                    touch_states[i].y = y;
                    
                    // 创建鼠标按下事件
                    MouseEvent event = {0};
                    event.type = MOUSE_DOWN;
                    event.x = x;
                    event.y = y;
                    event.button = MOUSE_LEFT;
                    
                    if (ui_root->handle_mouse_event) {
                        ui_root->handle_mouse_event(ui_root, &event);
                    }
                } else {
                    // 触摸移动
                    touch_states[i].x = x;
                    touch_states[i].y = y;
                    
                    // 创建鼠标移动事件
                    MouseEvent event = {0};
                    event.type = MOUSE_MOVE;
                    event.x = x;
                    event.y = y;
                    
                    if (ui_root->handle_mouse_event) {
                        ui_root->handle_mouse_event(ui_root, &event);
                    }
                }
            } else if (touch_states[i].active) {
                // 触摸释放
                touch_states[i].active = false;
                
                // 创建鼠标释放事件
                MouseEvent event = {0};
                event.type = MOUSE_UP;
                event.x = touch_states[i].x;
                event.y = touch_states[i].y;
                event.button = MOUSE_LEFT;
                
                if (ui_root->handle_mouse_event) {
                    ui_root->handle_mouse_event(ui_root, &event);
                }
            }
        }
        
        // 调用更新回调
        for (int i = 0; i < update_callback_count; i++) {
            if (update_callbacks[i]) {
                update_callbacks[i]();
            }
        }
        
        // 渲染UI
        backend_render_clear_color(255, 255, 255, 255); // 白色背景
        
        if (ui_root->render) {
            ui_root->render(ui_root);
        }
        
        // 显示帧缓冲区内容
        backend_render_present();
        
        // 简单延时，可优化为使用定时器
        backend_delay(16); // 约60FPS
    }
}

// ====================== 其他函数 ======================
int backend_query_texture(Texture* texture, Uint32* format, int* access, int* w, int* h) {
    if (!texture) return -1;
    
    if (format) *format = 0; // ARGB8888
    if (access) *access = 0;
    if (w) *w = texture->w;
    if (h) *h = texture->h;
    
    return 0;
}

char* backend_get_clipboard_text() {
    // STM32 平台通常没有剪贴板概念
    return strdup("");
}