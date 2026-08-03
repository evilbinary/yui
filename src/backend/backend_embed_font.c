/*
 * 通用嵌入式字体模块（ESP32 / STM32 共用）
 * 基于 stb_truetype，返回 CPU 端 RGBA8888 像素数据。
 */
#include "backend_embed_font.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"

typedef struct {
    unsigned char* data;     /* ttf 文件数据 */
    size_t data_size;
    stbtt_fontinfo info;
    float scale;             /* = stbtt_ScaleForPixelHeight(size * density) */
    int size;                /* 逻辑像素大小 */
} EmbedFont;

typedef struct {
    unsigned char* pixels;   /* RGBA8888，w*h*4 */
    int w;
    int h;
} EmbedTexture;

extern float yui_density;

static float embed_density(void) {
    return yui_density > 0.0f ? yui_density : 1.0f;
}

static unsigned char* embed_read_file(const char* path, size_t* out_size) {
    FILE* f;
    long sz;
    unsigned char* buf;

    if (!path || !out_size) return NULL;
    f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    sz = ftell(f);
    if (sz <= 0) { fclose(f); return NULL; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return NULL; }
    buf = (unsigned char*)malloc((size_t)sz);
    if (!buf) { fclose(f); return NULL; }
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        free(buf);
        fclose(f);
        return NULL;
    }
    fclose(f);
    *out_size = (size_t)sz;
    return buf;
}

/* UTF-8 解码：推进 *cursor，返回码点；失败返回 0 */
static int embed_utf8_next(const char** cursor, int* cp) {
    const unsigned char* s = (const unsigned char*)*cursor;
    if (!s[0]) return 0;
    if (s[0] < 0x80) {
        *cp = (int)s[0];
        *cursor += 1;
        return 1;
    }
    if ((s[0] & 0xE0) == 0xC0 && s[1]) {
        *cp = ((int)(s[0] & 0x1F) << 6) | (int)(s[1] & 0x3F);
        *cursor += 2;
        return 1;
    }
    if ((s[0] & 0xF0) == 0xE0 && s[1] && s[2]) {
        *cp = ((int)(s[0] & 0x0F) << 12) | ((int)(s[1] & 0x3F) << 6) | (int)(s[2] & 0x3F);
        *cursor += 3;
        return 1;
    }
    if ((s[0] & 0xF8) == 0xF0 && s[1] && s[2] && s[3]) {
        *cp = ((int)(s[0] & 0x07) << 18) | ((int)(s[1] & 0x3F) << 12) |
              ((int)(s[2] & 0x3F) << 6) | (int)(s[3] & 0x3F);
        *cursor += 4;
        return 1;
    }
    *cp = '?';
    *cursor += 1;
    return 1;
}

DFont* embed_font_load_with_weight(const char* path, int size, const char* weight) {
    size_t data_size = 0;
    unsigned char* data = NULL;
    EmbedFont* ef;
    DFont* font;
    int offset;

    (void)weight;
    if (!path || !path[0] || size <= 0) return NULL;

    data = embed_read_file(path, &data_size);
    if (!data) return NULL;

    offset = stbtt_GetFontOffsetForIndex(data, 0);
    if (offset < 0) {
        free(data);
        return NULL;
    }

    ef = (EmbedFont*)calloc(1, sizeof(EmbedFont));
    if (!ef) { free(data); return NULL; }
    if (!stbtt_InitFont(&ef->info, data, offset)) {
        free(data);
        free(ef);
        return NULL;
    }

    ef->data = data;
    ef->data_size = data_size;
    ef->size = size;
    ef->scale = stbtt_ScaleForPixelHeight(&ef->info, (float)size * embed_density());

    font = (DFont*)calloc(1, sizeof(DFont));
    if (!font) { free(data); free(ef); return NULL; }
    font->size = size;
    font->priv = ef;
    return font;
}

DFont* embed_font_load(const char* path, int size) {
    return embed_font_load_with_weight(path, size, "normal");
}

void embed_font_free(DFont* font) {
    EmbedFont* ef;
    if (!font) return;
    ef = (EmbedFont*)font->priv;
    if (ef) {
        free(ef->data);
        free(ef);
    }
    free(font);
}

int embed_font_measure_text(DFont* font, const char* text) {
    EmbedFont* ef;
    const char* cursor;
    int cp = 0;
    int pen_x = 0;

    if (!font || !text || !text[0]) return 0;
    ef = (EmbedFont*)font->priv;
    if (!ef) return 0;

    cursor = text;
    while (embed_utf8_next(&cursor, &cp)) {
        int advance = 0;
        int lsb = 0;
        if (cp == '\n') continue;
        stbtt_GetCodepointHMetrics(&ef->info, cp, &advance, &lsb);
        pen_x += (int)(advance * ef->scale);
    }
    /* 转换为布局像素（去掉 density） */
    return (int)((float)pen_x / embed_density() + 0.5f);
}

/* 计算文本边界框，返回 1 成功 */
static int embed_measure_bounds(EmbedFont* ef, const char* text,
                                int* out_w, int* out_h, int* out_min_x, int* out_min_y) {
    int ascent = 0, descent = 0, line_gap = 0;
    int baseline;
    int pen_x = 0;
    int cp = 0;
    const char* cursor;
    int min_x = 0, max_x = 0, min_y = 0, max_y = 0;
    int has_glyph = 0;

    stbtt_GetFontVMetrics(&ef->info, &ascent, &descent, &line_gap);
    baseline = (int)(ascent * ef->scale);

    cursor = text;
    while (embed_utf8_next(&cursor, &cp)) {
        int advance = 0, lsb = 0;
        int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
        int gx0, gx1, gy0, gy1;
        if (cp == '\n') continue;
        stbtt_GetCodepointHMetrics(&ef->info, cp, &advance, &lsb);
        stbtt_GetCodepointBitmapBox(&ef->info, cp, ef->scale, ef->scale, &x0, &y0, &x1, &y1);
        gx0 = pen_x + x0;
        gx1 = pen_x + x1;
        gy0 = baseline + y0;
        gy1 = baseline + y1;
        if (!has_glyph) {
            min_x = gx0; max_x = gx1;
            min_y = gy0; max_y = gy1;
            has_glyph = 1;
        } else {
            if (gx0 < min_x) min_x = gx0;
            if (gx1 > max_x) max_x = gx1;
            if (gy0 < min_y) min_y = gy0;
            if (gy1 > max_y) max_y = gy1;
        }
        pen_x += (int)(advance * ef->scale);
    }

    if (!has_glyph) return 0;
    if (max_x - min_x <= 0 || max_y - min_y <= 0) return 0;
    *out_w = max_x - min_x;
    *out_h = max_y - min_y;
    if (out_min_x) *out_min_x = min_x;
    if (out_min_y) *out_min_y = min_y;
    return 1;
}

Texture* embed_font_render(DFont* font, const char* text, Color color) {
    EmbedFont* ef;
    EmbedTexture* et;
    Texture* tex;
    int w = 0, h = 0, min_x = 0, min_y = 0;
    int ascent = 0, descent = 0, line_gap = 0;
    int baseline;
    int pen_x = 0;
    int cp = 0;
    const char* cursor;
    unsigned char* bitmap;
    float density;

    if (!font || !text || !text[0]) return NULL;
    ef = (EmbedFont*)font->priv;
    if (!ef) return NULL;

    if (!embed_measure_bounds(ef, text, &w, &h, &min_x, &min_y)) return NULL;

    stbtt_GetFontVMetrics(&ef->info, &ascent, &descent, &line_gap);
    baseline = (int)(ascent * ef->scale);

    bitmap = (unsigned char*)calloc((size_t)w * (size_t)h, 4);
    if (!bitmap) return NULL;

    cursor = text;
    while (embed_utf8_next(&cursor, &cp)) {
        int advance = 0, lsb = 0;
        int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
        int gw, gh;
        unsigned char* glyph_bitmap;
        int dst_x, dst_y, gy, gx;
        if (cp == '\n') continue;
        stbtt_GetCodepointHMetrics(&ef->info, cp, &advance, &lsb);
        stbtt_GetCodepointBitmapBox(&ef->info, cp, ef->scale, ef->scale, &x0, &y0, &x1, &y1);
        gw = x1 - x0;
        gh = y1 - y0;
        glyph_bitmap = stbtt_GetCodepointBitmap(&ef->info, 0, ef->scale, cp, &gw, &gh, 0, 0);
        if (glyph_bitmap) {
            dst_x = pen_x + x0 - min_x;
            dst_y = baseline + y0 - min_y;
            for (gy = 0; gy < gh; gy++) {
                for (gx = 0; gx < gw; gx++) {
                    int px = dst_x + gx;
                    int py = dst_y + gy;
                    unsigned char alpha;
                    size_t dst_index;
                    if (px < 0 || py < 0 || px >= w || py >= h) continue;
                    alpha = glyph_bitmap[gy * gw + gx];
                    if (alpha == 0) continue;
                    dst_index = ((size_t)py * (size_t)w + (size_t)px) * 4;
                    bitmap[dst_index + 0] = color.r;
                    bitmap[dst_index + 1] = color.g;
                    bitmap[dst_index + 2] = color.b;
                    bitmap[dst_index + 3] = (unsigned char)((alpha * color.a) / 255);
                }
            }
            stbtt_FreeBitmap(glyph_bitmap, NULL);
        }
        pen_x += (int)(advance * ef->scale);
    }

    et = (EmbedTexture*)calloc(1, sizeof(EmbedTexture));
    if (!et) { free(bitmap); return NULL; }
    et->pixels = bitmap;
    et->w = w;
    et->h = h;

    tex = (Texture*)calloc(1, sizeof(Texture));
    if (!tex) { free(bitmap); free(et); return NULL; }
    tex->w = w;
    tex->h = h;
    tex->priv = et;

    /* density 仅用于测量换算，纹理已是物理像素 */
    density = embed_density();
    (void)density;
    return tex;
}

void embed_font_texture_free(Texture* tex) {
    EmbedTexture* et;
    if (!tex) return;
    et = (EmbedTexture*)tex->priv;
    if (et) {
        free(et->pixels);
        free(et);
    }
    free(tex);
}

unsigned char* embed_font_texture_pixels(Texture* tex) {
    EmbedTexture* et;
    if (!tex) return NULL;
    et = (EmbedTexture*)tex->priv;
    return et ? et->pixels : NULL;
}
