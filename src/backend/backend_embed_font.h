#ifndef BACKEND_EMBED_FONT_H
#define BACKEND_EMBED_FONT_H

/*
 * 通用嵌入式字体模块（ESP32 / STM32 共用）
 * 基于 stb_truetype，返回 CPU 端 RGBA 像素数据，不依赖 OpenGL/SDL_ttf。
 * DFont->priv  -> EmbedFont
 * Texture->priv-> EmbedTexture
 *
 * 内存策略：
 *   - embed_font_load            : fopen 读文件到 RAM（owns_data=1，free 时释放）
 *   - embed_font_load_from_memory: 直接用外部数据指针（owns_data=0，free 时不释放）
 *     适合 ESP32 esp_partition_mmap 把 TTF 映射到 Flash 地址，零 RAM 占用
 *
 * 纹理缓存：
 *   - embed_font_render 内置 LRU 纹理缓存（font+text+color -> Texture*）
 *   - 同一段文字重复渲染直接命中，避免每帧重新栅格化
 */

#include "ytype.h"

#ifdef __cplusplus
extern "C" {
#endif

DFont* embed_font_load(const char* path, int size);
DFont* embed_font_load_with_weight(const char* path, int size, const char* weight);

/* 直接用内存中的 TTF 数据创建字体（不复制、不释放数据） */
DFont* embed_font_load_from_memory(const void* data, size_t data_size, int size, const char* weight);

void embed_font_free(DFont* font);

/* 测量文本布局宽度（像素，已按 density 换算） */
int embed_font_measure_text(DFont* font, const char* text);

/* 渲染文本为 RGBA8888 纹理（命中缓存时复用，不会重复创建）。
 * 注意：带缓存时返回的纹理归缓存所有，不要用 embed_font_texture_free 释放，
 *       用 embed_font_texture_release 表示不再使用（仅引用计数 -1）。 */
Texture* embed_font_render(DFont* font, const char* text, Color color);

/* 无缓存渲染：每次都重新栅格化，调用方负责用 embed_font_texture_free 释放 */
Texture* embed_font_render_nocache(DFont* font, const char* text, Color color);

void embed_font_texture_free(Texture* tex);
void embed_font_texture_release(Texture* tex);

/* 取纹理像素数据（RGBA8888，行主序，w*h*4 字节） */
unsigned char* embed_font_texture_pixels(Texture* tex);

/* 纹理缓存管理 */
void embed_font_texture_cache_invalidate(void);
void embed_font_texture_cache_set_max(int max_slots);
int  embed_font_texture_cache_get_max(void);

#ifdef __cplusplus
}
#endif

#endif /* BACKEND_EMBED_FONT_H */
