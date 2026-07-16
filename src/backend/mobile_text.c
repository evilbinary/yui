#ifdef __ANDROID__

#include "mobile_text.h"
#include "backend.h"

#include <GLES2/gl2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"

typedef struct {
    unsigned char* data;
    stbtt_fontinfo info;
    float scale;
    int size;
} MobileFont;

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
    mobile_font->scale = stbtt_ScaleForPixelHeight(&mobile_font->info, (float)mobile_font->size);

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

Texture* mobile_render_text_texture(DFont* font, const char* text, Color color) {
    MobileFont* mobile_font;
    const char* cursor;
    int width = 0;
    int height = 0;
    int ascent = 0;
    int descent = 0;
    int line_gap = 0;
    int baseline = 0;
    unsigned char* bitmap = NULL;
    Texture* texture = NULL;
    MobileGlTexture* gl_tex = NULL;
    int xpos = 0;
    int cp = 0;

    if (!font || !font->priv || !text || !text[0]) {
        return NULL;
    }

    mobile_font = (MobileFont*)font->priv;
    cursor = text;
    while (mobile_utf8_next(&cursor, &cp)) {
        int advance = 0;
        int lsb = 0;
        int x0 = 0;
        int y0 = 0;
        int x1 = 0;
        int y1 = 0;
        if (cp == '\n') {
            continue;
        }
        stbtt_GetCodepointHMetrics(&mobile_font->info, cp, &advance, &lsb);
        stbtt_GetCodepointBitmapBox(&mobile_font->info, cp, mobile_font->scale,
                                    mobile_font->scale, &x0, &y0, &x1, &y1);
        xpos += (int)(advance * mobile_font->scale);
        if (y1 - y0 > height) {
            height = y1 - y0;
        }
    }
    width = xpos;
    if (width <= 0 || height <= 0) {
        return NULL;
    }

    stbtt_GetFontVMetrics(&mobile_font->info, &ascent, &descent, &line_gap);
    baseline = (int)(ascent * mobile_font->scale);

    bitmap = (unsigned char*)calloc((size_t)width * (size_t)height, 4);
    if (!bitmap) {
        return NULL;
    }

    xpos = 0;
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

        stbtt_GetCodepointHMetrics(&mobile_font->info, cp, &advance, &lsb);
        stbtt_GetCodepointBitmapBox(&mobile_font->info, cp, mobile_font->scale,
                                    mobile_font->scale, &x0, &y0, &x1, &y1);
        gw = x1 - x0;
        gh = y1 - y0;
        glyph_bitmap = stbtt_GetCodepointBitmap(&mobile_font->info, 0, mobile_font->scale,
                                                cp, &gw, &gh, 0, 0);
        if (glyph_bitmap) {
            int dst_x = xpos + x0;
            int dst_y = baseline + y0;
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
        xpos += (int)(advance * mobile_font->scale);
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

void mobile_draw_text_texture(Texture* texture, const Rect* srcrect, const Rect* dstrect) {
    MobileGlTexture* gl_tex;
    float x;
    float y;
    float w;
    float h;
    float u0 = 0.0f;
    float v0 = 0.0f;
    float u1 = 1.0f;
    float v1 = 1.0f;
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
        v0 = (float)srcrect->y / (float)texture->h;
        u1 = (float)(srcrect->x + srcrect->w) / (float)texture->w;
        v1 = (float)(srcrect->y + srcrect->h) / (float)texture->h;
    }

    verts[0] = 0.0f; verts[1] = 0.0f; verts[2] = u0; verts[3] = v0;
    verts[4] = 1.0f; verts[5] = 0.0f; verts[6] = u1; verts[7] = v0;
    verts[8] = 0.0f; verts[9] = 1.0f; verts[10] = u0; verts[11] = v1;
    verts[12] = 1.0f; verts[13] = 1.0f; verts[14] = u1; verts[15] = v1;

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
