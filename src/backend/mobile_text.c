#ifdef __ANDROID__

#include "mobile_text.h"
#include "backend.h"
#include "util.h"

#include <GLES2/gl2.h>
/* #include <android/log.h> */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* #define MOBILE_FONT_TAG "YuiFont" */
/* #define MOBILE_FONT_LOG(...) __android_log_print(ANDROID_LOG_INFO, MOBILE_FONT_TAG, __VA_ARGS__) */
/* #define MOBILE_FONT_WARN(...) __android_log_print(ANDROID_LOG_WARN, MOBILE_FONT_TAG, __VA_ARGS__) */
#define MOBILE_FONT_LOG(...) ((void)0)
#define MOBILE_FONT_WARN(...) ((void)0)

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"

typedef struct {
    unsigned char* data;
    stbtt_fontinfo info;
    float yui_density;
    int size;
} MobileFont;

DFont* mobile_load_font(const char* font_path, int size, const char* weight);

static unsigned char* mobile_read_file(const char* path, size_t* out_size);

static char g_font_fallback_path[MAX_PATH] = "";
static char g_fallback_loaded_path[MAX_PATH] = "";
static DFont* g_fallback_font = NULL;
static int g_fallback_font_size = 0;

void mobile_set_font_fallback_path(const char* path) {
    if (!path) {
        g_font_fallback_path[0] = '\0';
        return;
    }
    strncpy(g_font_fallback_path, path, sizeof(g_font_fallback_path) - 1);
    g_font_fallback_path[sizeof(g_font_fallback_path) - 1] = '\0';
    MOBILE_FONT_LOG("fontFallback path set: %s", g_font_fallback_path);
}

int mobile_has_font_fallback(void) {
    return g_font_fallback_path[0] != '\0';
}

static void mobile_free_dfont(DFont* font) {
    MobileFont* mobile_font;
    if (!font) {
        return;
    }
    mobile_font = (MobileFont*)font->priv;
    if (mobile_font) {
        free(mobile_font->data);
        free(mobile_font);
    }
    free(font);
}

static int mobile_get_font_size(DFont* font) {
    MobileFont* mobile_font;
    if (!font || !font->priv) {
        return 16;
    }
    mobile_font = (MobileFont*)font->priv;
    return mobile_font->size > 0 ? mobile_font->size : 16;
}

static int mobile_glyph_is_provided(MobileFont* mobile_font, int cp) {
    int glyph;
    int notdef;
    if (!mobile_font || cp <= 0) {
        return 0;
    }
    glyph = stbtt_FindGlyphIndex(&mobile_font->info, cp);
    if (glyph == 0) {
        return 0;
    }
    /* Roboto 等主字体会把 emoji 映射到 .notdef，stb 仍返回非 0 字形 id */
    notdef = stbtt_FindGlyphIndex(&mobile_font->info, 0x10FFFF);
    if (notdef != 0 && glyph == notdef) {
        return 0;
    }
    notdef = stbtt_FindGlyphIndex(&mobile_font->info, 0);
    if (notdef != 0 && glyph == notdef) {
        return 0;
    }
    return 1;
}

static int mobile_font_has_cbdt(const unsigned char* data, size_t size) {
    int offset;
    const unsigned char* base;
    uint16_t num_tables;
    int i;

    if (!data || size < 16) {
        return 0;
    }

    offset = stbtt_GetFontOffsetForIndex(data, 0);
    if (offset < 0 || (size_t)offset + 12 > size) {
        return 0;
    }

    base = data + offset;
    num_tables = (uint16_t)((base[4] << 8) | base[5]);
    if ((size_t)offset + 12u + (size_t)num_tables * 16u > size) {
        return 0;
    }

    for (i = 0; i < num_tables; i++) {
        const unsigned char* entry = base + 12 + i * 16;
        if (entry[0] == 'C' && entry[1] == 'B' && entry[2] == 'D' && entry[3] == 'T') {
            return 1;
        }
    }
    return 0;
}

static DFont* mobile_load_font_strict(const char* font_path, int size, const char* weight) {
    size_t data_size = 0;
    unsigned char* data = NULL;
    MobileFont* mobile_font;
    DFont* font;

    if (!font_path || !font_path[0]) {
        return NULL;
    }

    data = mobile_read_file(font_path, &data_size);
    if (!data) {
        MOBILE_FONT_WARN("read failed: %s", font_path);
        return NULL;
    }

    if (mobile_font_has_cbdt(data, data_size)) {
        MOBILE_FONT_LOG("skip CBDT font: %s (size=%zu)", font_path, data_size);
        free(data);
        return NULL;
    }

    mobile_font = (MobileFont*)calloc(1, sizeof(MobileFont));
    if (!mobile_font) {
        free(data);
        return NULL;
    }

    if (!stbtt_InitFont(&mobile_font->info, data, stbtt_GetFontOffsetForIndex(data, 0))) {
        MOBILE_FONT_WARN("stbtt_InitFont failed: %s", font_path);
        free(data);
        free(mobile_font);
        return NULL;
    }

    mobile_font->data = data;
    mobile_font->size = size > 0 ? size : 16;
    mobile_font->yui_density = stbtt_ScaleForPixelHeight(
        &mobile_font->info, (float)mobile_font->size * (yui_density > 0.0f ? yui_density : 1.0f));

    font = (DFont*)calloc(1, sizeof(DFont));
    if (!font) {
        free(data);
        free(mobile_font);
        return NULL;
    }

    font->size = mobile_font->size;
    font->priv = mobile_font;
    (void)weight;
    MOBILE_FONT_LOG("loaded font: %s size=%d", font_path, mobile_font->size);
    return font;
}

static DFont* mobile_get_fallback_font_for(DFont* primary) {
    static const char* k_system_fallbacks[] = {
        "/system/fonts/NotoSansSymbols-Regular-Subsetted2.ttf",
        "/system/fonts/NotoSansSymbols-Regular-Subsetted.ttf",
        "/product/fonts/NotoSansSymbols-Regular-Subsetted2.ttf",
        NULL,
    };
    char sibling_path[MAX_PATH];
    const char* loaded_from = NULL;
    char* slash;
    int size;
    int i;

    if (!mobile_has_font_fallback() || !primary) {
        return NULL;
    }

    size = mobile_get_font_size(primary);
    if (g_fallback_font && g_fallback_font_size == size) {
        return g_fallback_font;
    }
    if (g_fallback_font) {
        mobile_free_dfont(g_fallback_font);
        g_fallback_font = NULL;
        g_fallback_font_size = 0;
    }

    g_fallback_loaded_path[0] = '\0';
    g_fallback_font = mobile_load_font_strict(g_font_fallback_path, size, "normal");
    if (g_fallback_font) {
        loaded_from = g_font_fallback_path;
    }

    if (!g_fallback_font) {
        snprintf(sibling_path, sizeof(sibling_path), "%s", g_font_fallback_path);
        slash = strrchr(sibling_path, '/');
        if (!slash) {
            slash = strrchr(sibling_path, '\\');
        }
        if (slash) {
            strcpy(slash + 1, "NotoEmoji-Regular.ttf");
            g_fallback_font = mobile_load_font_strict(sibling_path, size, "normal");
            if (g_fallback_font) {
                loaded_from = sibling_path;
            }
        }
    }

    if (!g_fallback_font) {
        g_fallback_font = mobile_load_font_strict("app/assets/NotoEmoji-Regular.ttf", size, "normal");
        if (g_fallback_font) {
            loaded_from = "app/assets/NotoEmoji-Regular.ttf";
        }
    }

    if (!g_fallback_font) {
        for (i = 0; k_system_fallbacks[i]; i++) {
            g_fallback_font = mobile_load_font_strict(k_system_fallbacks[i], size, "normal");
            if (g_fallback_font) {
                loaded_from = k_system_fallbacks[i];
                break;
            }
        }
    }

    if (g_fallback_font && loaded_from) {
        strncpy(g_fallback_loaded_path, loaded_from, sizeof(g_fallback_loaded_path) - 1);
        g_fallback_loaded_path[sizeof(g_fallback_loaded_path) - 1] = '\0';
        g_fallback_font_size = size;
        MOBILE_FONT_LOG("fallback ready: %s (px=%d)", g_fallback_loaded_path, size);
    } else {
        MOBILE_FONT_WARN("fallback load failed, configured=%s", g_font_fallback_path);
    }
    return g_fallback_font;
}

static DFont* mobile_resolve_render_font(DFont* primary, const char* text, DFont** out_fallback) {
    DFont* fallback;
    const char* cursor;
    int all_in_primary = 1;
    int all_in_fallback = 1;
    int any_in_primary = 0;
    MobileFont* primary_font;
    MobileFont* fallback_font;

    if (out_fallback) {
        *out_fallback = NULL;
    }

    if (!primary || !text || !mobile_has_font_fallback()) {
        return primary;
    }

    fallback = mobile_get_fallback_font_for(primary);
    if (out_fallback) {
        *out_fallback = fallback;
    }
    if (!fallback || fallback == primary || !fallback->priv || !primary->priv) {
        if (mobile_has_font_fallback()) {
            MOBILE_FONT_WARN("render '%.32s' -> no fallback font (fb=%p)", text, (void*)fallback);
        }
        return primary;
    }

    primary_font = (MobileFont*)primary->priv;
    fallback_font = (MobileFont*)fallback->priv;
    cursor = text;
    while (*cursor) {
        uint32_t codepoint = 0;
        int pri_g;
        int fb_g;
        int pri_ok;
        int fb_ok;

        if (!utf8_decode_codepoint(&cursor, &codepoint)) {
            break;
        }
        pri_g = stbtt_FindGlyphIndex(&primary_font->info, (int)codepoint);
        fb_g = stbtt_FindGlyphIndex(&fallback_font->info, (int)codepoint);
        pri_ok = mobile_glyph_is_provided(primary_font, (int)codepoint);
        fb_ok = mobile_glyph_is_provided(fallback_font, (int)codepoint);
        if (codepoint >= 0x1F300u || codepoint == 0x26A1 || codepoint == 0x1F514) {
            MOBILE_FONT_LOG("cp U+%X pri_g=%d fb_g=%d pri_ok=%d fb_ok=%d",
                            (unsigned)codepoint, pri_g, fb_g, pri_ok, fb_ok);
        }
        if (pri_ok) {
            any_in_primary = 1;
        } else {
            all_in_primary = 0;
        }
        if (!fb_ok) {
            all_in_fallback = 0;
        }
    }

    if (all_in_primary) {
        MOBILE_FONT_LOG("render '%.32s' -> primary only", text);
        return primary;
    }
    if (all_in_fallback && !any_in_primary) {
        MOBILE_FONT_LOG("render '%.32s' -> fallback only", text);
        return fallback;
    }
    MOBILE_FONT_LOG("render '%.32s' -> mixed (pri=%d fb=%d any_pri=%d)",
                    text, all_in_primary, all_in_fallback, any_in_primary);
    return NULL;
}

static MobileFont* mobile_pick_font_for_codepoint(MobileFont* primary, MobileFont* fallback, int cp) {
    if (primary && mobile_glyph_is_provided(primary, cp)) {
        return primary;
    }
    if (fallback && mobile_glyph_is_provided(fallback, cp)) {
        return fallback;
    }
    return primary;
}

typedef struct {
    GLuint id;
} MobileGlTexture;

static GLuint g_tex_program = 0;

static unsigned char* mobile_read_file(const char* path, size_t* out_size) {
    FILE* file;
    long file_size;
    unsigned char* data;

    if (!path || !out_size) {
        return NULL;
    }

    file = fopen(path, "rb");
    if (!file) {
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }
    file_size = ftell(file);
    if (file_size <= 0) {
        fclose(file);
        return NULL;
    }
    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }

    data = (unsigned char*)malloc((size_t)file_size);
    if (!data) {
        fclose(file);
        return NULL;
    }
    if (fread(data, 1, (size_t)file_size, file) != (size_t)file_size) {
        free(data);
        fclose(file);
        return NULL;
    }

    fclose(file);
    *out_size = (size_t)file_size;
    return data;
}

static int mobile_utf8_next(const char** text, int* codepoint) {
    const unsigned char* s = (const unsigned char*)(*text);
    if (!s[0]) {
        return 0;
    }
    if (s[0] < 0x80) {
        *codepoint = (int)s[0];
        *text += 1;
        return 1;
    }
    if ((s[0] & 0xE0) == 0xC0 && s[1]) {
        *codepoint = ((int)(s[0] & 0x1F) << 6) | (int)(s[1] & 0x3F);
        *text += 2;
        return 1;
    }
    if ((s[0] & 0xF0) == 0xE0 && s[1] && s[2]) {
        *codepoint = ((int)(s[0] & 0x0F) << 12) | ((int)(s[1] & 0x3F) << 6) | (int)(s[2] & 0x3F);
        *text += 3;
        return 1;
    }
    if ((s[0] & 0xF8) == 0xF0 && s[1] && s[2] && s[3]) {
        *codepoint = ((int)(s[0] & 0x07) << 18) | ((int)(s[1] & 0x3F) << 12) |
                     ((int)(s[2] & 0x3F) << 6) | (int)(s[3] & 0x3F);
        *text += 4;
        return 1;
    }
    *codepoint = '?';
    *text += 1;
    return 1;
}

static GLuint mobile_compile_shader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    GLint status = 0;
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (!status) {
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

void mobile_init_text_gl(void) {
    const char* vs =
        "attribute vec2 aPos;\n"
        "attribute vec2 aUV;\n"
        "uniform vec2 uOffset;\n"
        "uniform vec2 uScale;\n"
        "varying vec2 vUV;\n"
        "void main() {\n"
        "  vUV = aUV;\n"
        "  vec2 p = aPos * uScale + uOffset;\n"
        "  gl_Position = vec4(p, 0.0, 1.0);\n"
        "}\n";
    const char* fs =
        "precision mediump float;\n"
        "varying vec2 vUV;\n"
        "uniform sampler2D uTex;\n"
        "void main() { gl_FragColor = texture2D(uTex, vUV); }\n";
    GLuint vsh;
    GLuint fsh;
    GLint linked = 0;

    if (g_tex_program != 0) {
        return;
    }

    vsh = mobile_compile_shader(GL_VERTEX_SHADER, vs);
    fsh = mobile_compile_shader(GL_FRAGMENT_SHADER, fs);
    if (!vsh || !fsh) {
        return;
    }

    g_tex_program = glCreateProgram();
    glAttachShader(g_tex_program, vsh);
    glAttachShader(g_tex_program, fsh);
    glLinkProgram(g_tex_program);
    glGetProgramiv(g_tex_program, GL_LINK_STATUS, &linked);
    glDeleteShader(vsh);
    glDeleteShader(fsh);
    if (!linked) {
        glDeleteProgram(g_tex_program);
        g_tex_program = 0;
        return;
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void mobile_shutdown_text_gl(void) {
    if (g_tex_program != 0) {
        glDeleteProgram(g_tex_program);
        g_tex_program = 0;
    }
}

DFont* mobile_load_font(const char* font_path, int size, const char* weight) {
    static const char* k_fallback_fonts[] = {
        "/system/fonts/NotoSansSC-Regular.otf",
        "/system/fonts/NotoSansCJK-Regular.ttc",
        "/system/fonts/Roboto-Regular.ttf",
        NULL,
    };
    MobileFont* mobile_font;
    DFont* font;
    size_t data_size = 0;
    unsigned char* data = NULL;
    int i;

    (void)weight;

    if (font_path && font_path[0]) {
        data = mobile_read_file(font_path, &data_size);
    }
    for (i = 0; !data && k_fallback_fonts[i]; i++) {
        data = mobile_read_file(k_fallback_fonts[i], &data_size);
    }
    if (!data) {
        fprintf(stderr, "mobile_text: failed to load font: %s\n",
                font_path ? font_path : "(null)");
        return NULL;
    }

    mobile_font = (MobileFont*)calloc(1, sizeof(MobileFont));
    if (!mobile_font) {
        free(data);
        return NULL;
    }

    if (!stbtt_InitFont(&mobile_font->info, data, stbtt_GetFontOffsetForIndex(data, 0))) {
        fprintf(stderr, "mobile_text: stbtt_InitFont failed\n");
        free(data);
        free(mobile_font);
        return NULL;
    }

    mobile_font->data = data;
    mobile_font->size = size > 0 ? size : 16;
    mobile_font->yui_density = stbtt_ScaleForPixelHeight(
        &mobile_font->info, (float)mobile_font->size * (yui_density > 0.0f ? yui_density : 1.0f));

    font = (DFont*)calloc(1, sizeof(DFont));
    if (!font) {
        free(data);
        free(mobile_font);
        return NULL;
    }

    font->size = mobile_font->size;
    font->priv = mobile_font;
    return font;
}

static Texture* mobile_upload_text_bitmap(unsigned char* bitmap, int width, int height) {
    Texture* texture;
    MobileGlTexture* gl_tex;

    if (!bitmap || width <= 0 || height <= 0) {
        free(bitmap);
        return NULL;
    }

    gl_tex = (MobileGlTexture*)calloc(1, sizeof(MobileGlTexture));
    texture = (Texture*)calloc(1, sizeof(Texture));
    if (!gl_tex || !texture) {
        free(bitmap);
        free(gl_tex);
        free(texture);
        return NULL;
    }

    glGenTextures(1, &gl_tex->id);
    glBindTexture(GL_TEXTURE_2D, gl_tex->id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, bitmap);
    glBindTexture(GL_TEXTURE_2D, 0);

    texture->w = width;
    texture->h = height;
    texture->priv = gl_tex;
    free(bitmap);
    return texture;
}

static unsigned char* mobile_rasterize_font_text(MobileFont* mobile_font, MobileFont* fallback_font,
                                                 const char* text, Color color, int* out_w, int* out_h) {
    const char* cursor;
    int width = 0;
    int height = 0;
    int ascent = 0;
    int descent = 0;
    int line_gap = 0;
    int baseline = 0;
    int min_x = 0;
    int min_y = 0;
    int max_x = 0;
    int max_y = 0;
    int has_glyph = 0;
    unsigned char* bitmap = NULL;
    int pen_x = 0;
    int cp = 0;
    MobileFont* active_font;

    if (!mobile_font || !text || !text[0] || !out_w || !out_h) {
        return NULL;
    }

    stbtt_GetFontVMetrics(&mobile_font->info, &ascent, &descent, &line_gap);
    baseline = (int)(ascent * mobile_font->yui_density);

    cursor = text;
    while (mobile_utf8_next(&cursor, &cp)) {
        int advance = 0;
        int lsb = 0;
        int x0 = 0;
        int y0 = 0;
        int x1 = 0;
        int y1 = 0;
        int gx0 = 0;
        int gx1 = 0;
        int gy0 = 0;
        int gy1 = 0;

        if (cp == '\n') {
            continue;
        }

        active_font = mobile_pick_font_for_codepoint(mobile_font, fallback_font, cp);
        stbtt_GetCodepointHMetrics(&active_font->info, cp, &advance, &lsb);
        stbtt_GetCodepointBitmapBox(&active_font->info, cp, active_font->yui_density,
                                    active_font->yui_density, &x0, &y0, &x1, &y1);
        gx0 = pen_x + x0;
        gx1 = pen_x + x1;
        gy0 = baseline + y0;
        gy1 = baseline + y1;
        if (!has_glyph) {
            min_x = gx0;
            max_x = gx1;
            min_y = gy0;
            max_y = gy1;
            has_glyph = 1;
        } else {
            if (gx0 < min_x) min_x = gx0;
            if (gx1 > max_x) max_x = gx1;
            if (gy0 < min_y) min_y = gy0;
            if (gy1 > max_y) max_y = gy1;
        }
        pen_x += (int)(advance * active_font->yui_density);
    }

    if (!has_glyph) {
        return NULL;
    }

    width = max_x - min_x;
    height = max_y - min_y;
    if (width <= 0 || height <= 0) {
        return NULL;
    }

    bitmap = (unsigned char*)calloc((size_t)width * (size_t)height, 4);
    if (!bitmap) {
        return NULL;
    }

    pen_x = 0;
    cursor = text;
    while (mobile_utf8_next(&cursor, &cp)) {
        int advance = 0;
        int lsb = 0;
        int x0 = 0;
        int y0 = 0;
        int x1 = 0;
        int y1 = 0;
        int gw = 0;
        int gh = 0;
        unsigned char* glyph_bitmap = NULL;

        if (cp == '\n') {
            continue;
        }

        active_font = mobile_pick_font_for_codepoint(mobile_font, fallback_font, cp);
        stbtt_GetCodepointHMetrics(&active_font->info, cp, &advance, &lsb);
        stbtt_GetCodepointBitmapBox(&active_font->info, cp, active_font->yui_density,
                                    active_font->yui_density, &x0, &y0, &x1, &y1);
        gw = x1 - x0;
        gh = y1 - y0;
        glyph_bitmap = stbtt_GetCodepointBitmap(&active_font->info, 0, active_font->yui_density,
                                                cp, &gw, &gh, 0, 0);
        if (glyph_bitmap) {
            int dst_x = pen_x + x0 - min_x;
            int dst_y = baseline + y0 - min_y;
            int gy;
            for (gy = 0; gy < gh; gy++) {
                int gx;
                for (gx = 0; gx < gw; gx++) {
                    int px = dst_x + gx;
                    int py = dst_y + gy;
                    unsigned char alpha;
                    size_t dst_index;
                    if (px < 0 || py < 0 || px >= width || py >= height) {
                        continue;
                    }
                    alpha = glyph_bitmap[gy * gw + gx];
                    if (alpha == 0) {
                        continue;
                    }
                    dst_index = ((size_t)py * (size_t)width + (size_t)px) * 4;
                    bitmap[dst_index + 0] = color.r;
                    bitmap[dst_index + 1] = color.g;
                    bitmap[dst_index + 2] = color.b;
                    bitmap[dst_index + 3] = (unsigned char)((alpha * color.a) / 255);
                }
            }
            stbtt_FreeBitmap(glyph_bitmap, NULL);
        }
        pen_x += (int)(advance * active_font->yui_density);
    }

    *out_w = width;
    *out_h = height;
    return bitmap;
}

static Texture* mobile_render_mixed_texture(DFont* primary, DFont* fallback, const char* text, Color color) {
    const char* cursor = text;
    const char* run_starts[128];
    const char* run_ends[128];
    MobileFont* run_fonts[128];
    unsigned char* run_bitmaps[128];
    int run_widths[128];
    int run_heights[128];
    int run_count = 0;
    int total_w = 0;
    int max_h = 0;
    int i;
    int x = 0;
    char run_buf[256];
    unsigned char* final_bitmap = NULL;
    MobileFont* primary_font = (MobileFont*)primary->priv;
    MobileFont* fallback_font = fallback ? (MobileFont*)fallback->priv : NULL;

    for (i = 0; i < 128; i++) {
        run_bitmaps[i] = NULL;
    }

    while (*cursor && run_count < 128) {
        const char* run_start = cursor;
        uint32_t codepoint = 0;
        MobileFont* chosen;

        if (!utf8_decode_codepoint(&cursor, &codepoint)) {
            break;
        }
        chosen = mobile_pick_font_for_codepoint(primary_font, fallback_font, (int)codepoint);
        if (!chosen) {
            continue;
        }

        if (run_count > 0 && run_fonts[run_count - 1] == chosen) {
            run_ends[run_count - 1] = cursor;
        } else {
            run_starts[run_count] = run_start;
            run_ends[run_count] = cursor;
            run_fonts[run_count] = chosen;
            run_count++;
        }
    }

    for (i = 0; i < run_count; i++) {
        int len = (int)(run_ends[i] - run_starts[i]);
        int rw = 0;
        int rh = 0;
        MobileFont* other = (run_fonts[i] == primary_font) ? fallback_font : primary_font;

        if (len <= 0 || len >= (int)sizeof(run_buf)) {
            continue;
        }
        memcpy(run_buf, run_starts[i], (size_t)len);
        run_buf[len] = '\0';
        run_bitmaps[i] = mobile_rasterize_font_text(run_fonts[i], other, run_buf, color, &rw, &rh);
        if (!run_bitmaps[i]) {
            continue;
        }
        run_widths[i] = rw;
        run_heights[i] = rh;
        total_w += rw;
        if (rh > max_h) {
            max_h = rh;
        }
    }

    if (total_w <= 0 || max_h <= 0) {
        for (i = 0; i < run_count; i++) {
            free(run_bitmaps[i]);
        }
        return NULL;
    }

    final_bitmap = (unsigned char*)calloc((size_t)total_w * (size_t)max_h, 4);
    if (!final_bitmap) {
        for (i = 0; i < run_count; i++) {
            free(run_bitmaps[i]);
        }
        return NULL;
    }

    for (i = 0; i < run_count; i++) {
        unsigned char* src = run_bitmaps[i];
        int rw = run_widths[i];
        int rh = run_heights[i];
        int dy;
        int dx;
        if (!src || rw <= 0 || rh <= 0) {
            continue;
        }
        for (dy = 0; dy < rh; dy++) {
            for (dx = 0; dx < rw; dx++) {
                int src_idx = (dy * rw + dx) * 4;
                int dst_x = x + dx;
                int dst_y = dy + (max_h - rh) / 2;
                size_t dst_idx;
                if (dst_x < 0 || dst_y < 0 || dst_x >= total_w || dst_y >= max_h) {
                    continue;
                }
                if (src[src_idx + 3] == 0) {
                    continue;
                }
                dst_idx = ((size_t)dst_y * (size_t)total_w + (size_t)dst_x) * 4;
                final_bitmap[dst_idx + 0] = src[src_idx + 0];
                final_bitmap[dst_idx + 1] = src[src_idx + 1];
                final_bitmap[dst_idx + 2] = src[src_idx + 2];
                final_bitmap[dst_idx + 3] = src[src_idx + 3];
            }
        }
        x += rw;
        free(src);
    }

    return mobile_upload_text_bitmap(final_bitmap, total_w, max_h);
}

Texture* mobile_render_text_texture(DFont* font, const char* text, Color color) {
    DFont* fallback = NULL;
    DFont* render_font;
    MobileFont* mobile_font;
    MobileFont* fallback_font = NULL;
    unsigned char* bitmap = NULL;
    int width = 0;
    int height = 0;

    if (!font || !font->priv || !text || !text[0]) {
        return NULL;
    }

    render_font = mobile_resolve_render_font(font, text, &fallback);
    if (render_font == NULL && fallback && fallback != font) {
        Texture* mixed = mobile_render_mixed_texture(font, fallback, text, color);
        if (mixed) {
            MOBILE_FONT_LOG("texture mixed ok for '%.32s'", text);
            return mixed;
        }
        MOBILE_FONT_WARN("mixed render failed for '%.32s'", text);
        render_font = font;
    }

    if (!render_font) {
        render_font = font;
    }

    mobile_font = (MobileFont*)render_font->priv;
    if (render_font == fallback && fallback && fallback->priv) {
        fallback_font = NULL;
    } else if (fallback && fallback->priv) {
        fallback_font = (MobileFont*)fallback->priv;
    }

    bitmap = mobile_rasterize_font_text(mobile_font, fallback_font, text, color, &width, &height);
    if (!bitmap && fallback && fallback != render_font) {
        mobile_font = (MobileFont*)fallback->priv;
        bitmap = mobile_rasterize_font_text(mobile_font, NULL, text, color, &width, &height);
    }
    if (!bitmap) {
        MOBILE_FONT_WARN("rasterize failed for '%.32s'", text);
        return NULL;
    }

    MOBILE_FONT_LOG("texture ok '%.32s' %dx%d", text, width, height);
    return mobile_upload_text_bitmap(bitmap, width, height);
}

void mobile_draw_text_texture(Texture* texture, const Rect* srcrect, const Rect* dstrect) {
    MobileGlTexture* gl_tex;
    float x;
    float y;
    float w;
    float h;
    float u0 = 0.0f;
    float u1 = 1.0f;
    float v_top = 0.0f;
    float v_bottom = 1.0f;
    float verts[16];
    GLint offset_loc;
    GLint scale_loc;
    GLint tex_loc;
    GLint pos_loc;
    GLint uv_loc;
    int window_w = 0;
    int window_h = 0;

    if (!texture || !texture->priv || !dstrect || g_tex_program == 0) {
        return;
    }

    backend_get_windowsize(&window_w, &window_h);
    {
        float d = yui_density > 0.0f ? yui_density : 1.0f;
        window_w = (int)((float)window_w * d);
        window_h = (int)((float)window_h * d);
    }
    if (window_w <= 0 || window_h <= 0) {
        return;
    }

    gl_tex = (MobileGlTexture*)texture->priv;
    x = (float)dstrect->x;
    y = (float)dstrect->y;
    w = (float)dstrect->w;
    h = (float)dstrect->h;

    if (srcrect && srcrect->w > 0 && srcrect->h > 0 && texture->w > 0 && texture->h > 0) {
        u0 = (float)srcrect->x / (float)texture->w;
        u1 = (float)(srcrect->x + srcrect->w) / (float)texture->w;
        v_top = 1.0f - (float)(srcrect->y + srcrect->h) / (float)texture->h;
        v_bottom = 1.0f - (float)srcrect->y / (float)texture->h;
    }

    /* aPos y=0 is the bottom of dstrect; y=1 is the top. */
    verts[0] = 0.0f; verts[1] = 0.0f; verts[2] = u0; verts[3] = v_bottom;
    verts[4] = 1.0f; verts[5] = 0.0f; verts[6] = u1; verts[7] = v_bottom;
    verts[8] = 0.0f; verts[9] = 1.0f; verts[10] = u0; verts[11] = v_top;
    verts[12] = 1.0f; verts[13] = 1.0f; verts[14] = u1; verts[15] = v_top;

    offset_loc = glGetUniformLocation(g_tex_program, "uOffset");
    scale_loc = glGetUniformLocation(g_tex_program, "uScale");
    tex_loc = glGetUniformLocation(g_tex_program, "uTex");
    pos_loc = glGetAttribLocation(g_tex_program, "aPos");
    uv_loc = glGetAttribLocation(g_tex_program, "aUV");

    glUseProgram(g_tex_program);
    glUniform2f(offset_loc, -1.0f + (2.0f * x / (float)window_w),
                1.0f - (2.0f * (y + h) / (float)window_h));
    glUniform2f(scale_loc, 2.0f * w / (float)window_w, 2.0f * h / (float)window_h);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gl_tex->id);
    glUniform1i(tex_loc, 0);
    glEnableVertexAttribArray((GLuint)pos_loc);
    glEnableVertexAttribArray((GLuint)uv_loc);
    glVertexAttribPointer((GLuint)pos_loc, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), verts);
    glVertexAttribPointer((GLuint)uv_loc, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), verts + 2);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisableVertexAttribArray((GLuint)pos_loc);
    glDisableVertexAttribArray((GLuint)uv_loc);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void mobile_blit_rgba_rect(const unsigned char* rgba, int tex_w, int tex_h,
                           float x, float y, float w, float h,
                           int window_w, int window_h) {
    GLuint tex = 0;
    float verts[16];
    GLint offset_loc;
    GLint scale_loc;
    GLint tex_loc;
    GLint pos_loc;
    GLint uv_loc;

    if (!rgba || tex_w <= 0 || tex_h <= 0 || g_tex_program == 0 ||
        window_w <= 0 || window_h <= 0 || w <= 0.0f || h <= 0.0f) {
        return;
    }

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex_w, tex_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);

    verts[0] = 0.0f; verts[1] = 0.0f; verts[2] = 0.0f; verts[3] = 1.0f;
    verts[4] = 1.0f; verts[5] = 0.0f; verts[6] = 1.0f; verts[7] = 1.0f;
    verts[8] = 0.0f; verts[9] = 1.0f; verts[10] = 0.0f; verts[11] = 0.0f;
    verts[12] = 1.0f; verts[13] = 1.0f; verts[14] = 1.0f; verts[15] = 0.0f;

    offset_loc = glGetUniformLocation(g_tex_program, "uOffset");
    scale_loc = glGetUniformLocation(g_tex_program, "uScale");
    tex_loc = glGetUniformLocation(g_tex_program, "uTex");
    pos_loc = glGetAttribLocation(g_tex_program, "aPos");
    uv_loc = glGetAttribLocation(g_tex_program, "aUV");

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(g_tex_program);
    glUniform2f(offset_loc, -1.0f + (2.0f * x / (float)window_w),
                1.0f - (2.0f * (y + h) / (float)window_h));
    glUniform2f(scale_loc, 2.0f * w / (float)window_w, 2.0f * h / (float)window_h);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    glUniform1i(tex_loc, 0);
    glEnableVertexAttribArray((GLuint)pos_loc);
    glEnableVertexAttribArray((GLuint)uv_loc);
    glVertexAttribPointer((GLuint)pos_loc, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), verts);
    glVertexAttribPointer((GLuint)uv_loc, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), verts + 2);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisableVertexAttribArray((GLuint)pos_loc);
    glDisableVertexAttribArray((GLuint)uv_loc);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDeleteTextures(1, &tex);
}

void mobile_destroy_text_texture(Texture* texture) {
    MobileGlTexture* gl_tex;
    if (!texture) {
        return;
    }
    gl_tex = (MobileGlTexture*)texture->priv;
    if (gl_tex) {
        if (gl_tex->id != 0) {
            glDeleteTextures(1, &gl_tex->id);
        }
        free(gl_tex);
    }
    free(texture);
}

#endif
