#ifndef YUI_BACKEND_H
#define YUI_BACKEND_H

#include "layer.h"

extern float yui_density;

float backend_get_density(void);
void backend_set_density(float density);

int backend_init();
void backend_quit();
Texture* backend_load_texture(char* path);
Texture* backend_load_texture_from_base64(const char* base64_data, size_t data_len);
Texture* backend_render_texture(DFont* font,const char* text,Color color);
/* Measure text width in layout pixels without creating a render texture. */
int backend_measure_text_width(DFont* font, const char* text);
void backend_render_fill_rect(Rect* rect,Color color);
void backend_render_rect(Rect* rect,Color color);
void backend_render_rect_color(Rect* rect,unsigned char r,unsigned char g,unsigned char b,unsigned char a);

void backend_get_windowsize(int* width,int * height);
void backend_set_windowsize(int width,int  height);
void backend_set_window_size(char* title);
void backend_set_resizable(int resizable);
void backend_set_minimum_windowsize(int width, int height);
void backend_quit();
void backend_render_present();
void backend_delay(int delay);
Uint32 backend_get_ticks(void);
void backend_get_pointer_state(int* x, int* y);
void backend_render_clear_color(unsigned char r,unsigned char g,unsigned char b,unsigned char a);

DFont* backend_load_font(char* font_path,int size);
DFont* backend_load_font_with_weight(char* font_path,int size,const char* weight);

void backend_render_text_destroy(Texture * texture);
void backend_render_text_copy(Texture * texture,
                   const Rect * srcrect,
                   const Rect * dstrect);

void backend_render_fill_rect_color(Rect* rect,unsigned char r,unsigned char g,unsigned char b,unsigned char a);


void backend_render_get_clip_rect(Rect* prev_clip);

void backend_render_set_clip_rect(Rect* clip);

void backend_run(Layer* ui_root);

/* 单帧更新（移动端 / platform 宿主驱动；桌面 SDL 可用 backend_run） */
void backend_tick(Layer* ui_root);

/* Auto-test helpers: quit after N frames (-1 = forever), or request quit with exit code */
void backend_set_auto_frames(int frames);
void backend_request_quit(int exit_code);
int backend_get_exit_code(void);
int backend_should_quit(void);

int backend_query_texture(Texture * texture,
                     Uint32 * format, int *access,
                     int *w, int *h);

// 主循环回调类型
typedef void (*UpdateCallback)(void);
typedef void (*ResizeCallback)(Layer* root, int width, int height);

// 注册主循环更新回调（每帧调用）
void backend_register_update_callback(UpdateCallback callback);
void backend_set_resize_callback(ResizeCallback callback);

void backend_render_rounded_rect(Rect* rect, Color color, int radius);
void backend_render_rounded_rect_color(Rect* rect, unsigned char r, unsigned char g, unsigned char b, unsigned char a, int radius);
void backend_render_rounded_rect_with_border(Rect* rect, Color bg_color, int radius, int border_width, Color border_color);
// Add this declaration in backend.h with the other function declarations
void backend_render_line(int x1, int y1, int x2, int y2, Color color);
void backend_render_bezier_cubic(int x0, int y0,
                                 int cx1, int cy1, int cx2, int cy2,
                                 int x1, int y1, Color color, int width);
// 抗锯齿圆弧渲染函数
void backend_render_arc(int center_x, int center_y, int radius, float start_angle, float end_angle, Color color, int line_width);
// 毛玻璃效果渲染函数
void backend_render_backdrop_filter(Rect* rect, int blur_radius, float saturation, float brightness);
// 剪贴板操作
char* backend_get_clipboard_text();
void backend_set_clipboard_text(const char* text);

// IME 文本输入支持
void backend_start_text_input();
void backend_stop_text_input();
void backend_set_text_input_rect(Rect* rect);

// Windows 标题栏背景色
void backend_set_titlebar_color(Color bg, Color text);

// 设置窗口图标
void backend_set_window_icon(const char* path);

// 字体 fallback：主字体缺字时自动使用备用字体（如 emoji）
void backend_set_font_fallback_path(const char* path);
int backend_has_font_fallback(void);

// 文本纹理缓存：预热 / 固定 / 失效（SDL 后端）
void backend_texture_cache_invalidate(void);
void backend_texture_cache_pin(DFont* font, const char* text, Color color);
void backend_texture_cache_warmup(DFont* font, const char** texts, int count, Color color);

// 截取当前 UI 为 PNG（桌面 SDL；失败返回负值）
int backend_screenshot(const char* path);

// 设置当前 UI 根图层（在 backend_run 之前即可截图）
void backend_set_ui_root(Layer* root);

#endif