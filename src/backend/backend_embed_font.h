#ifndef BACKEND_EMBED_FONT_H
#define BACKEND_EMBED_FONT_H

/*
 * 通用嵌入式字体模块（ESP32 / STM32 共用）
 * 基于 stb_truetype，返回 CPU 端 RGBA 像素数据，不依赖 OpenGL/SDL_ttf。
 * DFont->priv  -> EmbedFont
 * Texture->priv-> EmbedTexture
 */

#include "ytype.h"

#ifdef __cplusplus
extern "C" {
#endif

DFont* embed_font_load(const char* path, int size);
DFont* embed_font_load_with_weight(const char* path, int size, const char* weight);
void embed_font_free(DFont* font);

/* 测量文本布局宽度（像素，已按 density 换算） */
int embed_font_measure_text(DFont* font, const char* text);

/* 渲染文本为 RGBA8888 纹理；调用方用 embed_font_texture_free 释放 */
Texture* embed_font_render(DFont* font, const char* text, Color color);
void embed_font_texture_free(Texture* tex);

/* 取纹理像素数据（RGBA8888，行主序，w*h*4 字节） */
unsigned char* embed_font_texture_pixels(Texture* tex);

#ifdef __cplusplus
}
#endif

#endif /* BACKEND_EMBED_FONT_H */
