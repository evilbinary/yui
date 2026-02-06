#ifndef YUI_BACKEND_H
#define YUI_BACKEND_H

#include "layer.h"

extern float scale;

int backend_init();
void backend_quit();
Texture* backend_load_texture(char* path);
Texture* backend_load_texture_from_base64(const char* base64_data, size_t data_len);
Texture* backend_render_texture(DFont* font,const char* text,Color color);
void backend_render_fill_rect(Rect* rect,Color color);
void backend_render_rect(Rect* rect,Color color);
void backend_render_rect_color(Rect* rect,unsigned char r,unsigned char g,unsigned char b,unsigned char a);

void backend_get_windowsize(int* width,int * height);
void backend_set_windowsize(int width,int  height);
void backend_set_window_size(char* title);
void backend_quit();
void backend_render_present();
void backend_delay(int delay);
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
int backend_query_texture(Texture * texture,
                     Uint32 * format, int *access,
                     int *w, int *h);

// 主循环回调类型
typedef void (*UpdateCallback)(void);

// 注册主循环更新回调（每帧调用）
void backend_register_update_callback(UpdateCallback callback);

void backend_render_rounded_rect(Rect* rect, Color color, int radius);
void backend_render_rounded_rect_color(Rect* rect, unsigned char r, unsigned char g, unsigned char b, unsigned char a, int radius);
void backend_render_rounded_rect_with_border(Rect* rect, Color bg_color, int radius, int border_width, Color border_color);
// Add this declaration in backend.h with the other function declarations
void backend_render_line(int x1, int y1, int x2, int y2, Color color);
// 抗锯齿圆弧渲染函数
void backend_render_arc(int center_x, int center_y, int radius, float start_angle, float end_angle, Color color, int line_width);
// 毛玻璃效果渲染函数
void backend_render_backdrop_filter(Rect* rect, int blur_radius, float saturation, float brightness);
// 剪贴板操作
char* backend_get_clipboard_text();

#endif