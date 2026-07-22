#ifndef MOBILE_TEXT_H
#define MOBILE_TEXT_H

#include "ytype.h"

#ifdef YUI_BACKEND_MOBILE

DFont* mobile_load_font(const char* font_path, int size, const char* weight);
int mobile_measure_text_width(DFont* font, const char* text);
Texture* mobile_render_text_texture(DFont* font, const char* text, Color color);
void mobile_draw_text_texture(Texture* texture, const Rect* srcrect, const Rect* dstrect);
void mobile_blit_rgba_rect(const unsigned char* rgba, int tex_w, int tex_h,
                           float x, float y, float w, float h,
                           int window_w, int window_h);
void mobile_destroy_text_texture(Texture* texture);
void mobile_text_cache_invalidate(void);
void mobile_init_text_gl(void);
void mobile_shutdown_text_gl(void);
void mobile_set_font_fallback_path(const char* path);
int mobile_has_font_fallback(void);

#endif

#endif
