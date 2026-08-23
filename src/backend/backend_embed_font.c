/*
 * 通用嵌入式字体模块（ESP32 / STM32 共用）
 * 基于 stb_truetype，返回 CPU 端 RGBA8888 像素数据。
 *
 * 内存策略：
 *   - embed_font_load            : fopen 读文件到 RAM（owns_data=1）
 *   - embed_font_load_from_memory: 用外部数据指针（owns_data=0），适合 Flash mmap
 *
 * 纹理缓存：
 *   - embed_font_render 内置 LRU 纹理缓存（font+text+color -> Texture*）
 *   - 命中时 refcount++ 返回缓存纹理；embed_font_texture_release 时 refcount--
 *   - LRU 仅淘汰 refcount==0 的槽，避免悬空
 */
#include "backend_embed_font.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"

#define EMBED_TEXT_CACHE_KEY_MAX 127   /* 文本 key 最大长度（超长不缓存） */
#ifndef EMBED_TEXT_CACHE_DEFAULT
#define EMBED_TEXT_CACHE_DEFAULT 64    /* 默认缓存槽数 */
#endif

typedef struct {
    unsigned char* data;     /* ttf 文件数据（可能指向 Flash） */
    size_t data_size;
    stbtt_fontinfo info;
    float scale;             /* = stbtt_ScaleForPixelHeight(size * density) */
    int size;                /* 逻辑像素大小 */
    int owns_data;           /* 1=free 时释放 data，0=不释放（外部管理） */
} EmbedFont;

typedef struct {
    unsigned char* pixels;   /* RGBA8888，w*h*4 */
    int w;
    int h;
    int refcount;            /* 引用计数（render +1，release -1） */
    int cached;              /* 1=在缓存中，0=nocache 纹理 */
} EmbedTexture;

extern float yui_density;

static float embed_density(void) {
    return yui_density > 0.0f ? yui_density : 1.0f;
}

/* ====================== 纹理缓存 ====================== */
typedef struct {
    Texture* tex;            /* 缓存的纹理（tex->priv->cached==1） */
    DFont* font;
    char text[EMBED_TEXT_CACHE_KEY_MAX + 1];
    Color color;
    uint32_t last_used;
    int valid;
} EmbedCacheSlot;

static EmbedCacheSlot g_cache[EMBED_TEXT_CACHE_DEFAULT];
static int g_cache_max = EMBED_TEXT_CACHE_DEFAULT;
static uint32_t g_cache_tick = 0;

void embed_font_texture_cache_invalidate(void) {
    int i;
    for (i = 0; i < g_cache_max; i++) {
        if (g_cache[i].valid && g_cache[i].tex) {
            EmbedTexture* et = (EmbedTexture*)g_cache[i].tex->priv;
            if (et) {
                free(et->pixels);
                free(et);
            }
            free(g_cache[i].tex);
        }
        g_cache[i].valid = 0;
        g_cache[i].tex = NULL;
        g_cache[i].font = NULL;
        g_cache[i].text[0] = '\0';
    }
}

void embed_font_texture_cache_set_max(int max_slots) {
    (void)max_slots;
    /* 固定数组，运行时不动态扩容；仅作占位接口 */
}

int embed_font_texture_cache_get_max(void) {
    return g_cache_max;
}

static int color_equal(Color a, Color b) {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

static Texture* cache_lookup(DFont* font, const char* text, Color color) {
    int i;
    for (i = 0; i < g_cache_max; i++) {
        if (g_cache[i].valid && g_cache[i].font == font &&
            color_equal(g_cache[i].color, color) &&
            strcmp(g_cache[i].text, text) == 0) {
            EmbedTexture* et;
            g_cache[i].last_used = ++g_cache_tick;
            et = (EmbedTexture*)g_cache[i].tex->priv;
            if (et) et->refcount++;
            return g_cache[i].tex;
        }
    }
    return NULL;
}

/* 找一个空槽或 refcount==0 的最久未用槽 */
static int cache_find_victim(void) {
    int i;
    int victim = -1;
    uint32_t oldest = 0xFFFFFFFFu;
    for (i = 0; i < g_cache_max; i++) {
        if (!g_cache[i].valid) return i;
        if (g_cache[i].tex) {
            EmbedTexture* et = (EmbedTexture*)g_cache[i].tex->priv;
            if (et && et->refcount > 0) continue;
        }
        if (g_cache[i].last_used < oldest) {
            oldest = g_cache[i].last_used;
            victim = i;
        }
    }
    if (victim >= 0) {
        EmbedTexture* et;
        if (g_cache[victim].tex) {
            et = (EmbedTexture*)g_cache[victim].tex->priv;
            if (et) { free(et->pixels); free(et); }
            free(g_cache[victim].tex);
        }
        g_cache[victim].valid = 0;
        g_cache[victim].tex = NULL;
    }
    return victim;
}

static void cache_store(Texture* tex, DFont* font, const char* text, Color color) {
    int slot = cache_find_victim();
    if (slot < 0) return; /* 缓存满且都在用，不缓存 */
    g_cache[slot].valid = 1;
    g_cache[slot].tex = tex;
    g_cache[slot].font = font;
    g_cache[slot].color = color;
    strncpy(g_cache[slot].text, text, EMBED_TEXT_CACHE_KEY_MAX);
    g_cache[slot].text[EMBED_TEXT_CACHE_KEY_MAX] = '\0';
    g_cache[slot].last_used = ++g_cache_tick;
}

/* 字形灰度缓存：时钟每分钟只换一两个数字，避免反复 stbtt_GetCodepointBitmap */
#define EMBED_GLYPH_CACHE_SLOTS 16
typedef struct {
    EmbedFont* ef;
    int cp;
    unsigned char* bits;
    int w, h;
    uint32_t last_used;
} EmbedGlyphSlot;
static EmbedGlyphSlot g_glyphs[EMBED_GLYPH_CACHE_SLOTS];
static uint32_t g_glyph_tick;

static void glyph_cache_evict(int i) {
    if (g_glyphs[i].bits) {
        stbtt_FreeBitmap(g_glyphs[i].bits, NULL);
        g_glyphs[i].bits = NULL;
    }
    g_glyphs[i].ef = NULL;
    g_glyphs[i].cp = 0;
}

static void glyph_cache_drop_font(EmbedFont* ef) {
    int i;
    for (i = 0; i < EMBED_GLYPH_CACHE_SLOTS; i++) {
        if (g_glyphs[i].ef == ef) {
            glyph_cache_evict(i);
        }
    }
}

static unsigned char* glyph_cache_get(EmbedFont* ef, int cp, int* gw, int* gh) {
    int i;
    int victim = 0;
    uint32_t oldest = 0xFFFFFFFFu;
    unsigned char* bm;
    int w = 0, h = 0;

    for (i = 0; i < EMBED_GLYPH_CACHE_SLOTS; i++) {
        if (g_glyphs[i].ef == ef && g_glyphs[i].cp == cp && g_glyphs[i].bits) {
            g_glyphs[i].last_used = ++g_glyph_tick;
            *gw = g_glyphs[i].w;
            *gh = g_glyphs[i].h;
            return g_glyphs[i].bits;
        }
        if (!g_glyphs[i].bits) {
            victim = i;
            oldest = 0;
        } else if (g_glyphs[i].last_used < oldest) {
            oldest = g_glyphs[i].last_used;
            victim = i;
        }
    }

    bm = stbtt_GetCodepointBitmap(&ef->info, ef->scale, ef->scale, cp, &w, &h, 0, 0);
    if (!bm) return NULL;
    glyph_cache_evict(victim);
    g_glyphs[victim].ef = ef;
    g_glyphs[victim].cp = cp;
    g_glyphs[victim].bits = bm;
    g_glyphs[victim].w = w;
    g_glyphs[victim].h = h;
    g_glyphs[victim].last_used = ++g_glyph_tick;
    *gw = w;
    *gh = h;
    return bm;
}

/* ====================== 文件读取 ====================== */
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

/* ====================== 字体加载 ====================== */
DFont* embed_font_load_from_memory(const void* data, size_t data_size, int size, const char* weight) {
    EmbedFont* ef;
    DFont* font;
    int offset;
    (void)weight;
    if (!data || data_size == 0 || size <= 0) return NULL;

    offset = stbtt_GetFontOffsetForIndex((const unsigned char*)data, 0);
    if (offset < 0) return NULL;

    ef = (EmbedFont*)calloc(1, sizeof(EmbedFont));
    if (!ef) return NULL;
    if (!stbtt_InitFont(&ef->info, (const unsigned char*)data, offset)) {
        free(ef);
        return NULL;
    }
    ef->data = (unsigned char*)data;  /* 不复制，外部管理 */
    ef->data_size = data_size;
    ef->size = size;
    ef->scale = stbtt_ScaleForPixelHeight(&ef->info, (float)size * embed_density());
    ef->owns_data = 0;

    font = (DFont*)calloc(1, sizeof(DFont));
    if (!font) { free(ef); return NULL; }
    font->size = size;
    font->priv = ef;
    return font;
}

DFont* embed_font_load_with_weight(const char* path, int size, const char* weight) {
    size_t data_size = 0;
    unsigned char* data;
    DFont* font;
    EmbedFont* ef;

    if (!path || !path[0] || size <= 0) return NULL;
    data = embed_read_file(path, &data_size);
    if (!data) return NULL;

    font = embed_font_load_from_memory(data, data_size, size, weight);
    if (!font) { free(data); return NULL; }
    /* 文件加载的字体 owns_data=1，free 时释放 */
    ef = (EmbedFont*)font->priv;
    ef->owns_data = 1;
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
        glyph_cache_drop_font(ef);
        if (ef->owns_data && ef->data) free(ef->data);
        free(ef);
    }
    free(font);
}

/* ====================== 测量 ====================== */
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
    return (int)((float)pen_x / embed_density() + 0.5f);
}

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
        gx0 = pen_x + x0; gx1 = pen_x + x1;
        gy0 = baseline + y0; gy1 = baseline + y1;
        if (!has_glyph) {
            min_x = gx0; max_x = gx1; min_y = gy0; max_y = gy1;
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

/* ====================== 渲染（无缓存） ====================== */
Texture* embed_font_render_nocache(DFont* font, const char* text, Color color) {
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
        gw = x1 - x0; gh = y1 - y0;
        glyph_bitmap = glyph_cache_get(ef, cp, &gw, &gh);
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
        }
        pen_x += (int)(advance * ef->scale);
    }

    et = (EmbedTexture*)calloc(1, sizeof(EmbedTexture));
    if (!et) { free(bitmap); return NULL; }
    et->pixels = bitmap;
    et->w = w; et->h = h;
    et->refcount = 1;
    et->cached = 0;

    tex = (Texture*)calloc(1, sizeof(Texture));
    if (!tex) { free(bitmap); free(et); return NULL; }
    tex->w = w; tex->h = h;
    tex->priv = et;
    return tex;
}

/* ====================== 渲染（带缓存） ====================== */
#ifndef EMBED_TEXT_CACHE_MAX_TEX_SIZE
#define EMBED_TEXT_CACHE_MAX_TEX_SIZE (16 * 1024)   /* 大纹理不入缓存，帧末即释放 */
#endif
Texture* embed_font_render(DFont* font, const char* text, Color color) {
    Texture* tex;
    EmbedTexture* et;
    size_t tlen;

    if (!font || !text || !text[0]) return NULL;
    tlen = strlen(text);
    /* 超长文本不缓存，直接 nocache 渲染 */
    if (tlen > EMBED_TEXT_CACHE_KEY_MAX) {
        return embed_font_render_nocache(font, text, color);
    }

    tex = cache_lookup(font, text, color);
    if (tex) return tex;

    tex = embed_font_render_nocache(font, text, color);
    if (!tex) return NULL;
    et = (EmbedTexture*)tex->priv;
    /* 大纹理（如整屏大字号时钟）不入缓存：缓存只淘汰 refcount==0 的槽，
     * 而大纹理常驻会让 LRU 无法及时回收，导致堆在首帧就耗尽。 */
    if ((size_t)et->w * (size_t)et->h * 4 > EMBED_TEXT_CACHE_MAX_TEX_SIZE) {
        et->cached = 0;
        return tex;
    }
    et->cached = 1;
    cache_store(tex, font, text, color);
    return tex;
}

/* ====================== 纹理释放 ====================== */
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

void embed_font_texture_release(Texture* tex) {
    EmbedTexture* et;
    if (!tex) return;
    et = (EmbedTexture*)tex->priv;
    if (!et) { free(tex); return; }
    /* nocache 纹理：refcount-- 到 0 直接释放 */
    if (!et->cached) {
        et->refcount--;
        if (et->refcount <= 0) embed_font_texture_free(tex);
        return;
    }
    /* 缓存纹理：refcount--，不释放（归缓存 LRU 管理） */
    if (et->refcount > 0) et->refcount--;
}

unsigned char* embed_font_texture_pixels(Texture* tex) {
    EmbedTexture* et;
    if (!tex) return NULL;
    et = (EmbedTexture*)tex->priv;
    return et ? et->pixels : NULL;
}
