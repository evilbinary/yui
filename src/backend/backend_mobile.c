#include "backend.h"
#include "component_registry.h"
#include "event.h"
#include "popup_manager.h"
#include "render.h"
#include "perf/perf.h"
#include "ytype.h"

#ifdef __ANDROID__
#include "mobile_text.h"
#endif

#include <stdlib.h>
#include <string.h>

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
    return linked ? 1 : 0;
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
    GLint offset_loc;
    GLint scale_loc;
    GLint color_loc;
    GLint pos_loc;

    if (!g_egl_ready || g_color_program == 0 || w <= 0.0f || h <= 0.0f) {
        return;
    }

    offset_loc = glGetUniformLocation(g_color_program, "uOffset");
    scale_loc = glGetUniformLocation(g_color_program, "uScale");
    color_loc = glGetUniformLocation(g_color_program, "uColor");
    pos_loc = glGetAttribLocation(g_color_program, "aPos");

    glUseProgram(g_color_program);
    glUniform2f(offset_loc, -1.0f + (2.0f * x / (float)g_window_w),
                1.0f - (2.0f * (y + h) / (float)g_window_h));
    glUniform2f(scale_loc, 2.0f * w / (float)g_window_w, 2.0f * h / (float)g_window_h);
    glUniform4f(color_loc, r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
    glEnableVertexAttribArray((GLuint)pos_loc);
    glVertexAttribPointer((GLuint)pos_loc, 2, GL_FLOAT, GL_FALSE, 0, verts);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisableVertexAttribArray((GLuint)pos_loc);
}
#endif

#define MAX_UPDATE_CALLBACKS 16
#define MAX_TOUCHES 10

float scale = 1.0f;

static float mobile_density(void) {
    return scale > 0.0f ? scale : 1.0f;
}

static void mobile_scale_rect(const Rect* src, Rect* dst) {
    float d = mobile_density();
    dst->x = (int)(src->x * d);
    dst->y = (int)(src->y * d);
    dst->w = (int)(src->w * d);
    dst->h = (int)(src->h * d);
}

extern Layer* g_ui_root;
static void* g_native_surface = NULL;
static UpdateCallback g_update_callbacks[MAX_UPDATE_CALLBACKS];
static int g_update_callback_count = 0;
static ResizeCallback g_resize_callback = NULL;

typedef struct {
    int active;
    int x;
    int y;
} MobileTouchState;

static MobileTouchState g_touches[MAX_TOUCHES];

void backend_mobile_set_native_surface(void* native_surface) {
    g_native_surface = native_surface;
#ifdef __ANDROID__
    if (native_surface) {
        mobile_egl_init((ANativeWindow*)native_surface);
    }
#endif
}

void backend_mobile_on_touch(int pointer_id, int x, int y, int phase) {
    TouchEvent event;
    MobileTouchState* touch;
    int id = pointer_id;

    if (id < 0 || id >= MAX_TOUCHES || !g_ui_root) {
        return;
    }

    touch = &g_touches[id];
    memset(&event, 0, sizeof(event));
    event.x = (int)(x / mobile_density());
    event.y = (int)(y / mobile_density());

    if (phase == 0) {
        event.type = TOUCH_TYPE_START;
        touch->active = 1;
        touch->x = x;
        touch->y = y;
    } else if (phase == 1) {
        if (!touch->active) {
            return;
        }
        event.type = TOUCH_TYPE_MOVE;
        event.deltaX = (int)((x - touch->x) / mobile_density());
        event.deltaY = (int)((y - touch->y) / mobile_density());
        touch->x = x;
        touch->y = y;
    } else {
        if (!touch->active) {
            return;
        }
        event.type = TOUCH_TYPE_END;
        touch->active = 0;
    }

    handle_touch_event(g_ui_root, &event);
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
#ifdef __ANDROID__
    return mobile_render_text_texture(font, text, color);
#else
    (void)font;
    (void)text;
    (void)color;
    return NULL;
#endif
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
    (void)radius;
    if (rect) {
        Rect physical;
        mobile_scale_rect(rect, &physical);
        mobile_draw_rect_norm((float)physical.x, (float)physical.y,
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
    (void)rect;
    (void)bg_color;
    (void)radius;
    (void)border_width;
    (void)border_color;
}

void backend_render_line(int x1, int y1, int x2, int y2, Color color) {
    (void)x1;
    (void)y1;
    (void)x2;
    (void)y2;
    (void)color;
}

void backend_render_bezier_cubic(int x0, int y0, int cx1, int cy1, int cx2, int cy2,
                                 int x1, int y1, Color color, int width) {
    (void)x0;
    (void)y0;
    (void)cx1;
    (void)cy1;
    (void)cx2;
    (void)cy2;
    (void)x1;
    (void)y1;
    (void)color;
    (void)width;
}

void backend_render_arc(int center_x, int center_y, int radius, float start_angle,
                        float end_angle, Color color, int line_width) {
    (void)center_x;
    (void)center_y;
    (void)radius;
    (void)start_angle;
    (void)end_angle;
    (void)color;
    (void)line_width;
}

void backend_render_backdrop_filter(Rect* rect, int blur_radius, float saturation,
                                    float brightness) {
    (void)rect;
    (void)blur_radius;
    (void)saturation;
    (void)brightness;
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
#endif
    if (g_ui_root && g_resize_callback) {
        g_resize_callback(g_ui_root, g_window_w, g_window_h);
    }
}

void backend_set_window_size(char* title) {
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
#endif
}

void backend_delay(int delay) {
    (void)delay;
}

Uint32 backend_get_ticks(void) {
    return 0;
}

void backend_get_mouse_state(int* x, int* y) {
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
        glClearColor(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
        glClear(GL_COLOR_BUFFER_BIT);
    }
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
#ifdef __ANDROID__
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

void backend_render_get_clip_rect(Rect* prev_clip) {
    if (prev_clip) {
        float d = mobile_density();
        prev_clip->x = 0;
        prev_clip->y = 0;
        prev_clip->w = (int)(g_window_w / d);
        prev_clip->h = (int)(g_window_h / d);
    }
}

void backend_render_set_clip_rect(Rect* clip) {
    (void)clip;
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

#ifdef __ANDROID__
    if (g_egl_ready && g_egl_display != EGL_NO_DISPLAY) {
        eglMakeCurrent(g_egl_display, g_egl_surface, g_egl_surface, g_egl_context);
    }
#endif

    for (i = 0; i < g_update_callback_count; i++) {
        if (g_update_callbacks[i]) {
            g_update_callbacks[i]();
        }
    }

    backend_render_clear_color(30, 30, 30, 255);
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
    return strdup("");
}

void backend_set_clipboard_text(const char* text) {
    (void)text;
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

void backend_set_font_fallback_path(const char* path) {
    (void)path;
}

int backend_has_font_fallback(void) {
    return 0;
}

void backend_texture_cache_invalidate(void) {}
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
    (void)path;
    return -1;
}

void backend_set_ui_root(Layer* root) {
    g_ui_root = root;
}
