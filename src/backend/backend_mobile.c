#include "backend.h"
#include "component_registry.h"
#include "event.h"
#include "popup_manager.h"
#include "render.h"
#include "perf/perf.h"
#include "screenshot.h"
#include "ytype.h"

#ifdef YUI_BACKEND_MOBILE
#include "mobile_text.h"
#endif

#ifdef __ANDROID__
extern char* android_clipboard_get_text(void);
extern void android_clipboard_set_text(const char* text);
#endif

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

#if defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
#include "ios_metal_glue.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#define MOBILE_DEFAULT_W 360
#define MOBILE_DEFAULT_H 640

static int g_window_w = MOBILE_DEFAULT_W;
static int g_window_h = MOBILE_DEFAULT_H;

#ifdef __ANDROID__
#include <android/native_window.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>

static EGLDisplay g_egl_display = EGL_NO_DISPLAY;
static EGLSurface g_egl_surface = EGL_NO_SURFACE;
static EGLContext g_egl_context = EGL_NO_CONTEXT;
static ANativeWindow* g_egl_window = NULL;
static GLuint g_color_program = 0;
static int g_egl_ready = 0;

/* Cached GL locations (set once after shader link) */
static GLint g_u_offset_loc = -1;
static GLint g_u_scale_loc = -1;
static GLint g_u_color_loc = -1;
static GLint g_a_pos_loc = -1;

/* Arc shader (GPU-based per-pixel distance field AA) */
static GLuint g_arc_program = 0;
static GLint g_arc_a_pos_loc = -1;
static GLint g_arc_u_center_loc = -1;
static GLint g_arc_u_inner_r_sq_loc = -1;
static GLint g_arc_u_outer_r_sq_loc = -1;
static GLint g_arc_u_start_angle_loc = -1;
static GLint g_arc_u_sweep_loc = -1;
static GLint g_arc_u_color_loc = -1;

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

static int mobile_init_gl(void) {
    const char* vs =
        "attribute vec2 aPos;\n"
        "uniform vec2 uOffset;\n"
        "uniform vec2 uScale;\n"
        "void main() {\n"
        "  vec2 p = aPos * uScale + uOffset;\n"
        "  gl_Position = vec4(p, 0.0, 1.0);\n"
        "}\n";
    const char* fs =
        "precision mediump float;\n"
        "uniform vec4 uColor;\n"
        "void main() { gl_FragColor = uColor; }\n";
    GLuint vsh = mobile_compile_shader(GL_VERTEX_SHADER, vs);
    GLuint fsh = mobile_compile_shader(GL_FRAGMENT_SHADER, fs);
    GLint linked = 0;
    if (!vsh || !fsh) {
        return 0;
    }
    g_color_program = glCreateProgram();
    glAttachShader(g_color_program, vsh);
    glAttachShader(g_color_program, fsh);
    glLinkProgram(g_color_program);
    glGetProgramiv(g_color_program, GL_LINK_STATUS, &linked);
    glDeleteShader(vsh);
    glDeleteShader(fsh);
    if (linked) {
        g_u_offset_loc = glGetUniformLocation(g_color_program, "uOffset");
        g_u_scale_loc  = glGetUniformLocation(g_color_program, "uScale");
        g_u_color_loc  = glGetUniformLocation(g_color_program, "uColor");
        g_a_pos_loc    = glGetAttribLocation(g_color_program, "aPos");
    }

    /* Arc shader: bounding-box quad + per-pixel signed-distance AA */
    const char* arc_vs =
        "attribute vec2 aPos;\n"
        "void main() {\n"
        "  gl_Position = vec4(aPos, 0.0, 1.0);\n"
        "}\n";
    const char* arc_fs =
        "precision mediump float;\n"
        "uniform vec2  uCenter;\n"
        "uniform float uInnerRSq;\n"
        "uniform float uOuterRSq;\n"
        "uniform float uStartRad;\n"
        "uniform float uSweepRad;\n"
        "uniform vec4  uColor;\n"
        "void main() {\n"
        "  vec2 d = gl_FragCoord.xy - uCenter;\n"
        "  float distSq = dot(d, d);\n"
        "  if (distSq < uInnerRSq - 1.5 || distSq > uOuterRSq + 1.5) discard;\n"
        "  float dist = sqrt(distSq);\n"
        "  float inner = sqrt(uInnerRSq);\n"
        "  float outer = sqrt(uOuterRSq);\n"
        "  float alpha = 1.0;\n"
        "  alpha *= clamp(dist - (inner - 1.0), 0.0, 1.0);\n"
        "  alpha *= clamp((outer + 1.0) - dist, 0.0, 1.0);\n"
        "  if (uSweepRad < 6.265) {\n"
        "    float angle = atan(d.y, d.x);\n"
        "    float da = mod(angle - uStartRad + 6.2832, 6.2832);\n"
        "    if (da > uSweepRad) discard;\n"
        "    float pixel_a = atan(1.0, max(dist, 1.0));\n"
        "    alpha *= clamp(da / pixel_a, 0.0, 1.0);\n"
        "    alpha *= clamp((uSweepRad - da) / pixel_a, 0.0, 1.0);\n"
        "  }\n"
        "  gl_FragColor = vec4(uColor.rgb, uColor.a * alpha);\n"
        "}\n";
    GLuint arc_vsh = mobile_compile_shader(GL_VERTEX_SHADER, arc_vs);
    GLuint arc_fsh = mobile_compile_shader(GL_FRAGMENT_SHADER, arc_fs);
    if (arc_vsh && arc_fsh) {
        g_arc_program = glCreateProgram();
        glAttachShader(g_arc_program, arc_vsh);
        glAttachShader(g_arc_program, arc_fsh);
        glLinkProgram(g_arc_program);
        glGetProgramiv(g_arc_program, GL_LINK_STATUS, &linked);
        glDeleteShader(arc_vsh);
        glDeleteShader(arc_fsh);
        if (linked) {
            g_arc_a_pos_loc       = glGetAttribLocation(g_arc_program, "aPos");
            g_arc_u_center_loc    = glGetUniformLocation(g_arc_program, "uCenter");
            g_arc_u_inner_r_sq_loc = glGetUniformLocation(g_arc_program, "uInnerRSq");
            g_arc_u_outer_r_sq_loc = glGetUniformLocation(g_arc_program, "uOuterRSq");
            g_arc_u_start_angle_loc = glGetUniformLocation(g_arc_program, "uStartRad");
            g_arc_u_sweep_loc     = glGetUniformLocation(g_arc_program, "uSweepRad");
            g_arc_u_color_loc     = glGetUniformLocation(g_arc_program, "uColor");
        }
    } else {
        if (arc_vsh) glDeleteShader(arc_vsh);
        if (arc_fsh) glDeleteShader(arc_fsh);
    }

    return 1;
}

static int mobile_egl_init(ANativeWindow* window) {
    const EGLint cfg_attribs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_BLUE_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_RED_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_NONE
    };
    const EGLint ctx_attribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    EGLConfig config;
    EGLint num_config = 0;

    if (!window) {
        return 0;
    }
    if (g_egl_ready) {
        return 1;
    }

    g_egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (g_egl_display == EGL_NO_DISPLAY) {
        return 0;
    }
    if (!eglInitialize(g_egl_display, NULL, NULL)) {
        return 0;
    }
    if (!eglChooseConfig(g_egl_display, cfg_attribs, &config, 1, &num_config) || num_config == 0) {
        return 0;
    }
    g_egl_surface = eglCreateWindowSurface(g_egl_display, config, window, NULL);
    if (g_egl_surface == EGL_NO_SURFACE) {
        return 0;
    }
    g_egl_context = eglCreateContext(g_egl_display, config, EGL_NO_CONTEXT, ctx_attribs);
    if (g_egl_context == EGL_NO_CONTEXT) {
        return 0;
    }
    if (!eglMakeCurrent(g_egl_display, g_egl_surface, g_egl_surface, g_egl_context)) {
        return 0;
    }

    eglSwapInterval(g_egl_display, 1);

    g_egl_window = window;
    g_window_w = ANativeWindow_getWidth(window);
    g_window_h = ANativeWindow_getHeight(window);
    if (!mobile_init_gl()) {
        return 0;
    }
    glViewport(0, 0, g_window_w, g_window_h);
    mobile_init_text_gl();
    g_egl_ready = 1;
    return 1;
}

static void mobile_egl_shutdown(void) {
    if (g_egl_display != EGL_NO_DISPLAY) {
        eglMakeCurrent(g_egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (g_egl_context != EGL_NO_CONTEXT) {
            eglDestroyContext(g_egl_display, g_egl_context);
            g_egl_context = EGL_NO_CONTEXT;
        }
        if (g_egl_surface != EGL_NO_SURFACE) {
            eglDestroySurface(g_egl_display, g_egl_surface);
            g_egl_surface = EGL_NO_SURFACE;
        }
        eglTerminate(g_egl_display);
        g_egl_display = EGL_NO_DISPLAY;
    }
    if (g_egl_window) {
        ANativeWindow_release(g_egl_window);
        g_egl_window = NULL;
    }
    g_color_program = 0;
    mobile_shutdown_text_gl();
    g_egl_ready = 0;
}

static void mobile_draw_rect_norm(float x, float y, float w, float h,
                                unsigned char r, unsigned char g,
                                unsigned char b, unsigned char a) {
    const float verts[] = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        0.0f, 1.0f,
        1.0f, 1.0f,
    };

    if (!g_egl_ready || g_color_program == 0 || w <= 0.0f || h <= 0.0f) {
        return;
    }

    glUseProgram(g_color_program);
    glUniform2f(g_u_offset_loc, -1.0f + (2.0f * x / (float)g_window_w),
                1.0f - (2.0f * (y + h) / (float)g_window_h));
    glUniform2f(g_u_scale_loc, 2.0f * w / (float)g_window_w, 2.0f * h / (float)g_window_h);
    glUniform4f(g_u_color_loc, r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
    glEnableVertexAttribArray((GLuint)g_a_pos_loc);
    glVertexAttribPointer((GLuint)g_a_pos_loc, 2, GL_FLOAT, GL_FALSE, 0, verts);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisableVertexAttribArray((GLuint)g_a_pos_loc);
}

static void mobile_fill_corner_phys(int cx, int cy, int radius, int quadrant,
                                    unsigned char r, unsigned char g,
                                    unsigned char b, unsigned char a) {
    float verts[262];
    int segments, vert_count;

    if (radius <= 0 || !g_egl_ready) return;

    segments = radius < 4 ? 4 : radius;
    if (segments > 128) segments = 128;
    vert_count = segments + 2;
    if (vert_count * 2 > 262) return;

    /* Center of fan = corner center */
    verts[0] = -1.0f + 2.0f * (float)cx / (float)g_window_w;
    verts[1] =  1.0f - 2.0f * (float)cy / (float)g_window_h;

    /* Arc vertices as triangle fan around the corner center.
     * Angles in screen coords: 0=right, π/2=down, π=left, 3π/2=up */
    for (int i = 0; i <= segments; i++) {
        float frac = (float)i / (float)segments;
        float a;
        switch (quadrant) {
        case 0: /* top-left:  left(π) → up(3π/2)   increasing */
            a = (float)M_PI + (float)M_PI * 0.5f * frac; break;
        case 1: /* top-right: up(3π/2) → right(2π)  increasing */
            a = (float)M_PI * 1.5f + (float)M_PI * 0.5f * frac; break;
        case 2: /* bottom-left: left(π) → down(π/2) decreasing */
            a = (float)M_PI - (float)M_PI * 0.5f * frac; break;
        default: /* bottom-right: down(π/2) → right(0) decreasing */
            a = (float)M_PI * 0.5f - (float)M_PI * 0.5f * frac; break;
        }
        float px = (float)cx + (float)radius * cosf(a);
        float py = (float)cy + (float)radius * sinf(a);
        verts[(i + 1) * 2 + 0] = -1.0f + 2.0f * px / (float)g_window_w;
        verts[(i + 1) * 2 + 1] =  1.0f - 2.0f * py / (float)g_window_h;
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(g_color_program);
    glUniform2f(g_u_offset_loc, 0.0f, 0.0f);
    glUniform2f(g_u_scale_loc, 1.0f, 1.0f);
    glUniform4f(g_u_color_loc, r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
    glEnableVertexAttribArray((GLuint)g_a_pos_loc);
    glVertexAttribPointer((GLuint)g_a_pos_loc, 2, GL_FLOAT, GL_FALSE, 0, verts);
    glDrawArrays(GL_TRIANGLE_FAN, 0, vert_count);
    glDisableVertexAttribArray((GLuint)g_a_pos_loc);
}

static void mobile_draw_rounded_rect_phys(float x, float y, float w, float h,
                                          int radius,
                                          unsigned char r, unsigned char g,
                                          unsigned char b, unsigned char a) {
    int ix;
    int iy;
    int iw;
    int ih;
    int rad;

    if (!g_egl_ready || w <= 0.0f || h <= 0.0f) {
        return;
    }

    ix = (int)(x + 0.5f);
    iy = (int)(y + 0.5f);
    iw = (int)(w + 0.5f);
    ih = (int)(h + 0.5f);
    rad = radius;

    if (rad <= 2) {
        mobile_draw_rect_norm(x, y, w, h, r, g, b, a);
        return;
    }
    if (rad > iw / 2) {
        rad = iw / 2;
    }
    if (rad > ih / 2) {
        rad = ih / 2;
    }
    if (rad <= 0) {
        mobile_draw_rect_norm(x, y, w, h, r, g, b, a);
        return;
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if (iw > 2 * rad) {
        mobile_draw_rect_norm(x + (float)rad, y, w - 2.0f * (float)rad, h, r, g, b, a);
    }
    if (ih > 2 * rad) {
        mobile_draw_rect_norm(x, y + (float)rad, (float)rad, h - 2.0f * (float)rad, r, g, b, a);
        mobile_draw_rect_norm(x + w - (float)rad, y + (float)rad,
                              (float)rad, h - 2.0f * (float)rad, r, g, b, a);
    }

    mobile_fill_corner_phys(ix + rad, iy + rad, rad, 0, r, g, b, a);
    mobile_fill_corner_phys(ix + iw - rad, iy + rad, rad, 1, r, g, b, a);
    mobile_fill_corner_phys(ix + rad, iy + ih - rad, rad, 2, r, g, b, a);
    mobile_fill_corner_phys(ix + iw - rad, iy + ih - rad, rad, 3, r, g, b, a);
}

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static void mobile_draw_pixel_phys(int px, int py,
                                   unsigned char r, unsigned char g,
                                   unsigned char b, unsigned char a) {
    mobile_draw_rect_norm((float)px, (float)py, 1.0f, 1.0f, r, g, b, a);
}

static void mobile_draw_line_phys(int x1, int y1, int x2, int y2,
                                  unsigned char r, unsigned char g,
                                  unsigned char b, unsigned char a) {
    float dx = (float)(x2 - x1);
    float dy = (float)(y2 - y1);
    float len = sqrtf(dx * dx + dy * dy);
    float perp_x, perp_y;
    float verts[8];
    float x1f, y1f, x2f, y2f;

    if (!g_egl_ready) return;

    if (len < 0.5f) {
        /* Single pixel */
        mobile_draw_rect_norm((float)x1, (float)y1, 1.0f, 1.0f, r, g, b, a);
        return;
    }

    /* Perpendicular unit vector for 1px line width */
    perp_x = -dy / len;
    perp_y =  dx / len;

    x1f = (float)x1;
    y1f = (float)y1;
    x2f = (float)x2;
    y2f = (float)y2;

    /* Two-triangle quad as triangle strip (pre-transformed to clip space) */
    verts[0] = -1.0f + 2.0f * (x1f + perp_x) / (float)g_window_w;
    verts[1] =  1.0f - 2.0f * (y1f + perp_y) / (float)g_window_h;
    verts[2] = -1.0f + 2.0f * (x1f - perp_x) / (float)g_window_w;
    verts[3] =  1.0f - 2.0f * (y1f - perp_y) / (float)g_window_h;
    verts[4] = -1.0f + 2.0f * (x2f + perp_x) / (float)g_window_w;
    verts[5] =  1.0f - 2.0f * (y2f + perp_y) / (float)g_window_h;
    verts[6] = -1.0f + 2.0f * (x2f - perp_x) / (float)g_window_w;
    verts[7] =  1.0f - 2.0f * (y2f - perp_y) / (float)g_window_h;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(g_color_program);
    glUniform2f(g_u_offset_loc, 0.0f, 0.0f);
    glUniform2f(g_u_scale_loc, 1.0f, 1.0f);
    glUniform4f(g_u_color_loc, r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
    glEnableVertexAttribArray((GLuint)g_a_pos_loc);
    glVertexAttribPointer((GLuint)g_a_pos_loc, 2, GL_FLOAT, GL_FALSE, 0, verts);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisableVertexAttribArray((GLuint)g_a_pos_loc);
}

static void mobile_draw_arc_phys(int center_x, int center_y, int radius,
                                 float start_angle, float end_angle,
                                 Color color, int line_width) {
    float half_w = (float)line_width * 0.5f;
    float r_inner = (float)radius - half_w;
    float r_outer = (float)radius + half_w;
    float sweep_deg = end_angle - start_angle;
    float start_rad, step_rad;
    int steps, pass, vert_count;
    float verts[1440];

    if (radius <= 0 || line_width <= 0 || !g_egl_ready) {
        return;
    }
    if (r_inner < 0.0f) r_inner = 0.0f;

    while (sweep_deg <= 0.0f) sweep_deg += 360.0f;

    steps = (int)(sweep_deg * 0.5f) + 1;
    if (steps < 4) steps = 4;
    if (steps > 360) steps = 360;

    step_rad = sweep_deg * (float)M_PI / 180.0f / (float)(steps - 1);
    start_rad = (start_angle - 90.0f) * (float)M_PI / 180.0f;

    vert_count = steps * 2;
    if (vert_count * 2 > 1440) return;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    /* Three passes for anti-aliasing:
     *  pass 0: inner AA edge (r_inner-1 → r_inner) at 40% alpha
     *  pass 1: main body    (r_inner   → r_outer)  at 100% alpha
     *  pass 2: outer AA edge (r_outer   → r_outer+1) at 40% alpha
     */
    for (pass = 0; pass < 3; pass++) {
        float ri, ro;
        unsigned char a;
        switch (pass) {
        case 0: ri = r_inner - 1.0f; ro = r_inner; a = (unsigned char)(color.a * 0.4f); break;
        case 1: ri = r_inner;        ro = r_outer; a = color.a; break;
        default:ri = r_outer;        ro = r_outer + 1.0f; a = (unsigned char)(color.a * 0.4f); break;
        }
        if (ri < 0.0f) ri = 0.0f;

        for (int i = 0; i < steps; i++) {
            float ang = start_rad + step_rad * (float)i;
            float cos_a = cosf(ang);
            float sin_a = sinf(ang);
            float ox = (float)center_x + ro * cos_a;
            float oy = (float)center_y + ro * sin_a;
            float ix = (float)center_x + ri * cos_a;
            float iy = (float)center_y + ri * sin_a;

            verts[i * 4 + 0] = -1.0f + 2.0f * ox / (float)g_window_w;
            verts[i * 4 + 1] =  1.0f - 2.0f * oy / (float)g_window_h;
            verts[i * 4 + 2] = -1.0f + 2.0f * ix / (float)g_window_w;
            verts[i * 4 + 3] =  1.0f - 2.0f * iy / (float)g_window_h;
        }

        glUseProgram(g_color_program);
        glUniform2f(g_u_offset_loc, 0.0f, 0.0f);
        glUniform2f(g_u_scale_loc, 1.0f, 1.0f);
        glUniform4f(g_u_color_loc, color.r / 255.0f, color.g / 255.0f,
                    color.b / 255.0f, a / 255.0f);
        glEnableVertexAttribArray((GLuint)g_a_pos_loc);
        glVertexAttribPointer((GLuint)g_a_pos_loc, 2, GL_FLOAT, GL_FALSE, 0, verts);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, vert_count);
        glDisableVertexAttribArray((GLuint)g_a_pos_loc);
    }
}

static void mobile_flip_rows(unsigned char* pixels, int w, int h, int stride) {
    unsigned char* row = (unsigned char*)malloc((size_t)stride);
    int y;
    if (!row) {
        return;
    }
    for (y = 0; y < h / 2; y++) {
        unsigned char* top = pixels + y * stride;
        unsigned char* bottom = pixels + (h - 1 - y) * stride;
        memcpy(row, top, (size_t)stride);
        memcpy(top, bottom, (size_t)stride);
        memcpy(bottom, row, (size_t)stride);
    }
    free(row);
}

static void mobile_box_blur(unsigned char* data, int w, int h, int stride, int radius) {
    unsigned char* tmp;
    int x;
    int y;
    int c;
    int k;

    if (radius <= 0) {
        return;
    }
    tmp = (unsigned char*)malloc((size_t)stride * (size_t)h);
    if (!tmp) {
        return;
    }

    for (y = 0; y < h; y++) {
        unsigned char* row = data + y * stride;
        unsigned char* trow = tmp + y * stride;
        for (x = 0; x < w; x++) {
            int sum[4] = {0, 0, 0, 0};
            int count = 0;
            for (k = -radius; k <= radius; k++) {
                int sx = x + k;
                if (sx < 0 || sx >= w) {
                    continue;
                }
                sum[0] += row[sx * 4 + 0];
                sum[1] += row[sx * 4 + 1];
                sum[2] += row[sx * 4 + 2];
                sum[3] += row[sx * 4 + 3];
                count++;
            }
            if (count > 0) {
                for (c = 0; c < 4; c++) {
                    trow[x * 4 + c] = (unsigned char)(sum[c] / count);
                }
            }
        }
    }
    memcpy(data, tmp, (size_t)stride * (size_t)h);

    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            int sum[4] = {0, 0, 0, 0};
            int count = 0;
            for (k = -radius; k <= radius; k++) {
                int sy = y + k;
                if (sy < 0 || sy >= h) {
                    continue;
                }
                unsigned char* src = data + sy * stride + x * 4;
                sum[0] += src[0];
                sum[1] += src[1];
                sum[2] += src[2];
                sum[3] += src[3];
                count++;
            }
            if (count > 0) {
                unsigned char* dst = tmp + y * stride + x * 4;
                for (c = 0; c < 4; c++) {
                    dst[c] = (unsigned char)(sum[c] / count);
                }
            }
        }
    }
    memcpy(data, tmp, (size_t)stride * (size_t)h);
    free(tmp);
}

static void mobile_apply_backdrop_tone(unsigned char* data, int count,
                                       float saturation, float brightness) {
    int i;
    float sat = saturation < 0.0f ? 0.0f : saturation;
    float bright = brightness < 0.0f ? 0.0f : brightness;

    for (i = 0; i < count; i++) {
        unsigned char* px = data + i * 4;
        float r = px[0] * bright;
        float g = px[1] * bright;
        float b = px[2] * bright;
        float gray = 0.299f * r + 0.587f * g + 0.114f * b;

        r = gray + (r - gray) * sat;
        g = gray + (g - gray) * sat;
        b = gray + (b - gray) * sat;
        if (r < 0.0f) r = 0.0f;
        if (g < 0.0f) g = 0.0f;
        if (b < 0.0f) b = 0.0f;
        if (r > 255.0f) r = 255.0f;
        if (g > 255.0f) g = 255.0f;
        if (b > 255.0f) b = 255.0f;
        px[0] = (unsigned char)r;
        px[1] = (unsigned char)g;
        px[2] = (unsigned char)b;
    }
}
#endif

#define MAX_UPDATE_CALLBACKS 16
#define MAX_TOUCHES 10
#define SWIPE_THRESHOLD_PX 32

static YuiRenderMode g_render_mode = YUI_RENDER_MODE_DIRTY;

void backend_set_render_mode(YuiRenderMode mode)
{
    g_render_mode = mode;
}

YuiRenderMode backend_get_render_mode(void)
{
    return g_render_mode;
}

float yui_density = 1.0f;

float backend_get_density(void) {
    return yui_density > 0.0f ? yui_density : 1.0f;
}

void backend_set_density(float density) {
    if (density > 0.0f) {
        yui_density = density;
    }
}

void backend_set_font_fallback_path(const char* path) {
#ifdef __ANDROID__
    mobile_set_font_fallback_path(path);
#endif
    (void)path;
}

int backend_has_font_fallback(void) {
#ifdef __ANDROID__
    return mobile_has_font_fallback();
#else
    return 0;
#endif
}

static float mobile_density(void) {
    return yui_density > 0.0f ? yui_density : 1.0f;
}

static void mobile_scale_rect(const Rect* src, Rect* dst) {
    float d = mobile_density();
    dst->x = (int)(src->x * d);
    dst->y = (int)(src->y * d);
    dst->w = (int)(src->w * d);
    dst->h = (int)(src->h * d);
}

#ifdef __ANDROID__
static Rect g_clip_rect;
static int g_clip_active = 0;

static void mobile_apply_clip_rect(const Rect* clip) {
    Rect physical;
    int sx;
    int sy;
    int sw;
    int sh;

    if (!g_egl_ready) {
        return;
    }
    if (!clip) {
        glDisable(GL_SCISSOR_TEST);
        return;
    }
    if (clip->w <= 0 || clip->h <= 0) {
        glEnable(GL_SCISSOR_TEST);
        glScissor(0, 0, 0, 0);
        return;
    }

    mobile_scale_rect(clip, &physical);
    sx = physical.x;
    sy = g_window_h - physical.y - physical.h;
    sw = physical.w;
    sh = physical.h;
    if (sx < 0) {
        sw += sx;
        sx = 0;
    }
    if (sy < 0) {
        sh += sy;
        sy = 0;
    }
    if (sx + sw > g_window_w) {
        sw = g_window_w - sx;
    }
    if (sy + sh > g_window_h) {
        sh = g_window_h - sy;
    }
    if (sw <= 0 || sh <= 0) {
        glEnable(GL_SCISSOR_TEST);
        glScissor(0, 0, 0, 0);
        return;
    }

    glEnable(GL_SCISSOR_TEST);
    glScissor(sx, sy, sw, sh);
}
#endif

Layer* g_ui_root = NULL;
static void* g_native_surface = NULL;
static UpdateCallback g_update_callbacks[MAX_UPDATE_CALLBACKS];
static int g_update_callback_count = 0;
static ResizeCallback g_resize_callback = NULL;

typedef struct {
    int active;
    int x;
    int y;
    int start_x;
    int start_y;
} MobileTouchState;

static MobileTouchState g_touches[MAX_TOUCHES];

static int mobile_active_touch_count(void) {
    int count = 0;
    int i;
    for (i = 0; i < MAX_TOUCHES; i++) {
        if (g_touches[i].active) {
            count++;
        }
    }
    return count;
}

static void mobile_emit_swipe_event(int x, int y, int dx, int dy) {
    PointerEvent swipe;
    int adx = dx < 0 ? -dx : dx;
    int ady = dy < 0 ? -dy : dy;

    if (!g_ui_root || adx < SWIPE_THRESHOLD_PX || adx < ady) {
        return;
    }

    memset(&swipe, 0, sizeof(swipe));
    swipe.device = POINTER_DEVICE_TOUCH;
    swipe.phase = POINTER_SWIPE;
    swipe.x = (int)(x / mobile_density());
    swipe.y = (int)(y / mobile_density());
    swipe.delta_x = (int)(dx / mobile_density());
    swipe.delta_y = (int)(dy / mobile_density());
    swipe.pointer_id = 0;
    swipe.finger_count = 1;
    handle_pointer_event(g_ui_root, &swipe);
}

void backend_mobile_set_native_surface(void* native_surface) {
    g_native_surface = native_surface;
#ifdef __ANDROID__
    if (native_surface) {
        mobile_egl_init((ANativeWindow*)native_surface);
    }
#elif defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
    if (native_surface) {
        ios_metal_init(native_surface);
    }
#endif
}

void backend_mobile_on_touch(int pointer_id, int x, int y, int phase) {
    PointerEvent event;
    MobileTouchState* touch;
    int id = pointer_id;

    if (id < 0 || id >= MAX_TOUCHES || !g_ui_root) {
        return;
    }

    touch = &g_touches[id];
    memset(&event, 0, sizeof(event));
    event.device = POINTER_DEVICE_TOUCH;
    event.x = (int)(x / mobile_density());
    event.y = (int)(y / mobile_density());

    if (phase == 0) {
        event.phase = POINTER_DOWN;
        touch->active = 1;
        touch->start_x = x;
        touch->start_y = y;
        touch->x = x;
        touch->y = y;
    } else if (phase == 1) {
        if (!touch->active) {
            return;
        }
        event.phase = POINTER_MOVE;
        event.delta_x = (int)((x - touch->x) / mobile_density());
        event.delta_y = (int)((y - touch->y) / mobile_density());
        touch->x = x;
        touch->y = y;
    } else {
        if (!touch->active) {
            return;
        }
        event.phase = POINTER_UP;
        mobile_emit_swipe_event(x, y, x - touch->start_x, y - touch->start_y);
        touch->active = 0;
    }

    event.pointer_id = id;
    event.finger_count = mobile_active_touch_count();

    handle_pointer_event(g_ui_root, &event);
}

int backend_init(void) {
    yui_component_registry_init();
    yui_components_register_builtin();
    (void)g_native_surface;
    return 0;
}

void backend_quit(void) {
#ifdef __ANDROID__
    mobile_egl_shutdown();
#elif defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
    ios_metal_shutdown();
#endif
    g_ui_root = NULL;
    g_native_surface = NULL;
    g_update_callback_count = 0;
    g_resize_callback = NULL;
    memset(g_touches, 0, sizeof(g_touches));
}

Texture* backend_load_texture(char* path) {
    (void)path;
    return NULL;
}

Texture* backend_load_texture_from_base64(const char* base64_data, size_t data_len) {
    (void)base64_data;
    (void)data_len;
    return NULL;
}

Texture* backend_render_texture(DFont* font, const char* text, Color color) {
#ifdef YUI_BACKEND_MOBILE
    return mobile_render_text_texture(font, text, color);
#else
    (void)font;
    (void)text;
    (void)color;
    return NULL;
#endif
}

int backend_measure_text_width(DFont* font, const char* text) {
    return mobile_measure_text_width(font, text);
}

void backend_render_fill_rect(Rect* rect, Color color) {
    backend_render_fill_rect_color(rect, color.r, color.g, color.b, color.a);
}

void backend_render_fill_rect_color(Rect* rect, unsigned char r, unsigned char g,
                                    unsigned char b, unsigned char a) {
#ifdef __ANDROID__
    if (rect) {
        Rect physical;
        mobile_scale_rect(rect, &physical);
        mobile_draw_rect_norm((float)physical.x, (float)physical.y,
                              (float)physical.w, (float)physical.h, r, g, b, a);
    }
#elif defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
    if (rect) {
        Rect physical;
        mobile_scale_rect(rect, &physical);
        ios_metal_draw_rect((float)physical.x, (float)physical.y,
                            (float)physical.w, (float)physical.h, r, g, b, a);
    }
#else
    (void)rect;
    (void)r;
    (void)g;
    (void)b;
    (void)a;
#endif
}

void backend_render_rect(Rect* rect, Color color) {
    backend_render_rect_color(rect, color.r, color.g, color.b, color.a);
}

void backend_render_rect_color(Rect* rect, unsigned char r, unsigned char g,
                               unsigned char b, unsigned char a) {
    (void)rect;
    (void)r;
    (void)g;
    (void)b;
    (void)a;
}

void backend_render_rounded_rect(Rect* rect, Color color, int radius) {
    backend_render_rounded_rect_color(rect, color.r, color.g, color.b, color.a, radius);
}

void backend_render_rounded_rect_color(Rect* rect, unsigned char r, unsigned char g,
                                       unsigned char b, unsigned char a, int radius) {
#ifdef __ANDROID__
    if (rect) {
        Rect physical;
        int phys_radius;
        mobile_scale_rect(rect, &physical);
        phys_radius = (int)(radius * mobile_density() + 0.5f);
        mobile_draw_rounded_rect_phys((float)physical.x, (float)physical.y,
                                      (float)physical.w, (float)physical.h,
                                      phys_radius, r, g, b, a);
    }
#elif defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
    if (rect) {
        Rect physical;
        mobile_scale_rect(rect, &physical);
        ios_metal_draw_rect((float)physical.x, (float)physical.y,
                            (float)physical.w, (float)physical.h, r, g, b, a);
    }
#else
    (void)rect;
    (void)r;
    (void)g;
    (void)b;
    (void)a;
    (void)radius;
#endif
}

void backend_render_rounded_rect_with_border(Rect* rect, Color bg_color, int radius,
                                             int border_width, Color border_color) {
    Rect inner;

    if (!rect) {
        return;
    }
    if (border_width <= 0) {
        backend_render_rounded_rect(rect, bg_color, radius);
        return;
    }

    backend_render_rounded_rect(rect, border_color, radius);
    inner.x = rect->x + border_width;
    inner.y = rect->y + border_width;
    inner.w = rect->w - 2 * border_width;
    inner.h = rect->h - 2 * border_width;
    if (inner.w > 0 && inner.h > 0) {
        int inner_r = radius - border_width;
        if (inner_r < 0) {
            inner_r = 0;
        }
        backend_render_rounded_rect(&inner, bg_color, inner_r);
    }
}

void backend_render_shadow(const Rect* rect, int radius,
                           int offset_x, int offset_y, int blur, int spread, Color color) {
    Rect r;
    (void)blur;
    if (!rect || color.a == 0) return;
    r = *rect;
    r.x += offset_x - spread;
    r.y += offset_y - spread;
    r.w += spread * 2;
    r.h += spread * 2;
    if (r.w > 0 && r.h > 0) {
        backend_render_rounded_rect(&r, color, radius + (spread > 0 ? spread : 0));
    }
}

void backend_render_rounded_gradient(const Rect* rect, int radius, int vertical,
                                     const Color* colors, int count) {
    (void)vertical;
    if (!rect || !colors || count <= 0) return;
    backend_render_rounded_rect((Rect*)rect, colors[0], radius);
}

void backend_render_line(int x1, int y1, int x2, int y2, Color color) {
#ifdef __ANDROID__
    float d = mobile_density();
    mobile_draw_line_phys((int)(x1 * d), (int)(y1 * d), (int)(x2 * d), (int)(y2 * d),
                          color.r, color.g, color.b, color.a);
#else
    (void)x1;
    (void)y1;
    (void)x2;
    (void)y2;
    (void)color;
#endif
}

void backend_render_arc(int center_x, int center_y, int radius, float start_angle,
                        float end_angle, Color color, int line_width) {
#ifdef __ANDROID__
    float d = mobile_density();
    int lw = (int)(line_width * d);
    if (lw < 1) {
        lw = 1;
    }
    mobile_draw_arc_phys((int)(center_x * d), (int)(center_y * d), (int)(radius * d),
                         start_angle, end_angle, color, lw);
#else
    (void)center_x;
    (void)center_y;
    (void)radius;
    (void)start_angle;
    (void)end_angle;
    (void)color;
    (void)line_width;
#endif
}

void backend_render_backdrop_filter(Rect* rect, int blur_radius, float saturation,
                                    float brightness) {
#ifdef __ANDROID__
    Rect physical;
    unsigned char* pixels;
    int stride;
    int radius = blur_radius;
    size_t bytes;

    if (!rect || blur_radius <= 0 || !g_egl_ready) {
        return;
    }
    if (radius > 12) {
        radius = 12;
    }

    mobile_scale_rect(rect, &physical);
    if (physical.w <= 0 || physical.h <= 0) {
        return;
    }

    stride = physical.w * 4;
    bytes = (size_t)stride * (size_t)physical.h;
    pixels = (unsigned char*)malloc(bytes);
    if (!pixels) {
        return;
    }

    glReadPixels(physical.x, g_window_h - physical.y - physical.h,
                 physical.w, physical.h, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    mobile_flip_rows(pixels, physical.w, physical.h, stride);
    mobile_box_blur(pixels, physical.w, physical.h, stride, radius);
    mobile_apply_backdrop_tone(pixels, physical.w * physical.h, saturation, brightness);
    mobile_blit_rgba_rect(pixels, physical.w, physical.h,
                          (float)physical.x, (float)physical.y,
                          (float)physical.w, (float)physical.h,
                          g_window_w, g_window_h);
    free(pixels);
#else
    (void)rect;
    (void)blur_radius;
    (void)saturation;
    (void)brightness;
#endif
}

void backend_get_windowsize(int* width, int* height) {
    float d = mobile_density();
    if (width) {
        *width = (int)(g_window_w / d);
    }
    if (height) {
        *height = (int)(g_window_h / d);
    }
}

void backend_set_windowsize(int width, int height) {
    if (width > 0) {
        g_window_w = width;
    }
    if (height > 0) {
        g_window_h = height;
    }
#ifdef __ANDROID__
    if (g_egl_ready) {
        glViewport(0, 0, g_window_w, g_window_h);
    }
#elif defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
    ios_metal_resize(g_window_w, g_window_h);
#endif
    if (g_ui_root && g_resize_callback) {
        g_resize_callback(g_ui_root, g_window_w, g_window_h);
    }
}

void backend_set_window_title(char* title) {
    (void)title;
}

void backend_set_resizable(int resizable) {
    (void)resizable;
}

void backend_set_minimum_windowsize(int width, int height) {
    (void)width;
    (void)height;
}

void backend_render_present(void) {
#ifdef __ANDROID__
    if (g_egl_ready && g_egl_display != EGL_NO_DISPLAY) {
        eglSwapBuffers(g_egl_display, g_egl_surface);
    }
#elif defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
    ios_metal_present();
#endif
}

void backend_delay(int delay) {
    (void)delay;
}

Uint32 backend_get_ticks(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0;
    }
    return (Uint32)((uint64_t)ts.tv_sec * 1000ull + (uint64_t)ts.tv_nsec / 1000000ull);
}

void backend_get_pointer_state(int* x, int* y) {
    if (x) {
        *x = 0;
    }
    if (y) {
        *y = 0;
    }
}

void backend_render_clear_color(unsigned char r, unsigned char g, unsigned char b,
                                unsigned char a) {
#ifdef __ANDROID__
    if (g_egl_ready) {
        glDisable(GL_SCISSOR_TEST);
        glClearColor(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
        glClear(GL_COLOR_BUFFER_BIT);
    }
#elif defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
    ios_metal_clear(r, g, b, a);
#else
    (void)r;
    (void)g;
    (void)b;
    (void)a;
#endif
}

DFont* backend_load_font(char* font_path, int size) {
    return backend_load_font_with_weight(font_path, size, "normal");
}

DFont* backend_load_font_with_weight(char* font_path, int size, const char* weight) {
#ifdef YUI_BACKEND_MOBILE
    return mobile_load_font(font_path, size, weight);
#else
    DFont* font;
    (void)font_path;
    (void)weight;
    font = (DFont*)calloc(1, sizeof(DFont));
    if (font) {
        font->size = size;
    }
    return font;
#endif
}

void backend_render_text_destroy(Texture* texture) {
#ifdef __ANDROID__
    mobile_destroy_text_texture(texture);
#else
    if (texture) {
        free(texture);
    }
#endif
}

void backend_render_text_copy(Texture* texture, const Rect* srcrect, const Rect* dstrect) {
#ifdef __ANDROID__
    if (dstrect) {
        Rect physical;
        mobile_scale_rect(dstrect, &physical);
        mobile_draw_text_texture(texture, srcrect, &physical);
    }
#else
    (void)texture;
    (void)srcrect;
    (void)dstrect;
#endif
}

void backend_render_texture_tinted(Texture* texture, const Rect* srcrect, const Rect* dstrect, Color tint) {
    (void)tint;
    backend_render_text_copy(texture, srcrect, dstrect);
}

void backend_render_get_clip_rect(Rect* prev_clip) {
    if (!prev_clip) {
        return;
    }
#ifdef __ANDROID__
    if (g_clip_active) {
        *prev_clip = g_clip_rect;
        return;
    }
#endif
    prev_clip->x = 0;
    prev_clip->y = 0;
    backend_get_windowsize(&prev_clip->w, &prev_clip->h);
}

void backend_render_set_clip_rect(Rect* clip) {
#ifdef __ANDROID__
    if (clip) {
        g_clip_active = 1;
        g_clip_rect = *clip;
    } else {
        g_clip_active = 0;
    }
    mobile_apply_clip_rect(clip);
#else
    (void)clip;
#endif
}

void backend_register_update_callback(UpdateCallback callback) {
    if (callback && g_update_callback_count < MAX_UPDATE_CALLBACKS) {
        g_update_callbacks[g_update_callback_count++] = callback;
    }
}

void backend_set_resize_callback(ResizeCallback callback) {
    g_resize_callback = callback;
}

void backend_tick(Layer* ui_root) {
    int i;

    if (!ui_root) {
        return;
    }

    g_ui_root = ui_root;

    for (i = 0; i < g_update_callback_count; i++) {
        if (g_update_callbacks[i]) {
            g_update_callbacks[i]();
        }
    }

    if (g_render_mode == YUI_RENDER_MODE_FULL) {
        backend_render_clear_color(30, 30, 30, 255);
    }
    perf_frame_begin();
    perf_render_tree_begin();
    render_layer(ui_root);
    perf_render_tree_end();
    render_inspect_overlay(ui_root);
    perf_draw_overlay(ui_root);
    popup_manager_render();
    perf_frame_end();
    backend_render_present();
}

void backend_run(Layer* ui_root) {
    g_ui_root = ui_root;
    backend_tick(ui_root);
}

void backend_set_auto_frames(int frames) { (void)frames; }
void backend_request_quit(int exit_code) { (void)exit_code; }
int backend_get_exit_code(void) { return 0; }
int backend_should_quit(void) { return 0; }
void backend_set_headless(int on) { (void)on; }
int backend_is_headless(void) { return 0; }

int backend_query_texture(Texture* texture, Uint32* format, int* access, int* w, int* h) {
    if (!texture) {
        return -1;
    }
    if (format) {
        *format = 0;
    }
    if (access) {
        *access = 0;
    }
    if (w) {
        *w = texture->w;
    }
    if (h) {
        *h = texture->h;
    }
    return 0;
}

char* backend_get_clipboard_text(void) {
#ifdef __ANDROID__
    char* text = android_clipboard_get_text();
    if (text) {
        return text;
    }
#endif
    return strdup("");
}

void backend_set_clipboard_text(const char* text) {
#ifdef __ANDROID__
    if (text) {
        android_clipboard_set_text(text);
    }
#else
    (void)text;
#endif
}

void backend_start_text_input(void) {}
void backend_stop_text_input(void) {}
void backend_set_text_input_rect(Rect* rect) { (void)rect; }

void backend_set_titlebar_color(Color bg, Color text) {
    (void)bg;
    (void)text;
}

void backend_set_window_icon(const char* path) {
    (void)path;
}

void backend_texture_cache_invalidate(void) {
#ifdef YUI_BACKEND_MOBILE
    mobile_text_cache_invalidate();
#endif
}
void backend_texture_cache_pin(DFont* font, const char* text, Color color) {
    (void)font;
    (void)text;
    (void)color;
}
void backend_texture_cache_warmup(DFont* font, const char** texts, int count, Color color) {
    (void)font;
    (void)texts;
    (void)count;
    (void)color;
}

int backend_screenshot(const char* path) {
#ifdef __ANDROID__
    unsigned char* pixels = NULL;
    unsigned char* row = NULL;
    int w;
    int h;
    int y;
    int row_bytes;

    if (!g_egl_ready || !g_ui_root || !path || !path[0]) {
        return -1;
    }

    w = g_window_w;
    h = g_window_h;
    if (w <= 0 || h <= 0) {
        return -2;
    }

    backend_render_clear_color(30, 30, 30, 255);
    render_layer(g_ui_root);
    render_inspect_overlay(g_ui_root);
    popup_manager_render();

    row_bytes = w * 4;
    pixels = (unsigned char*)malloc((size_t)row_bytes * (size_t)h);
    if (!pixels) {
        return -3;
    }

    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    row = (unsigned char*)malloc((size_t)row_bytes);
    if (!row) {
        free(pixels);
        return -3;
    }
    for (y = 0; y < h / 2; y++) {
        unsigned char* top = pixels + y * row_bytes;
        unsigned char* bottom = pixels + (h - 1 - y) * row_bytes;
        memcpy(row, top, (size_t)row_bytes);
        memcpy(top, bottom, (size_t)row_bytes);
        memcpy(bottom, row, (size_t)row_bytes);
    }
    free(row);

    {
        int rc = screenshot_save_png(path, pixels, w, h, row_bytes);
        free(pixels);
        return rc;
    }
#else
    (void)path;
    return -1;
#endif
}

void backend_set_ui_root(Layer* root) {
    g_ui_root = root;
}
