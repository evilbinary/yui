#include "backend_common.h"
#include "../backend.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#if defined(__unix__) || defined(__APPLE__)
#include <sys/wait.h>
#endif

/* 启动外部程序（阻塞到程序退出），返回其退出码；失败返回 -1，并打印原因。
 * system() 是标准库函数，所有平台都用它；POSIX 下额外解析 wait status 得到真实退出码。 */
int backend_spawn(const char* cmd) {
    int rc;
    if (!cmd || !cmd[0]) {
        fprintf(stderr, "backend_spawn: empty command\n");
        return -1;
    }
    rc = system(cmd);
    if (rc == -1) {
        fprintf(stderr, "backend_spawn: system failed: %s\n", strerror(errno));
        return -1;
    }
#if defined(__unix__) || defined(__APPLE__)
    if (WIFEXITED(rc)) {
        int code = WEXITSTATUS(rc);
        if (code == 127) {
            fprintf(stderr, "backend_spawn: command not found: %s\n", cmd);
        } else if (code != 0) {
            fprintf(stderr, "backend_spawn: command exited with code %d: %s\n", code, cmd);
        }
        return code;
    }
    if (WIFSIGNALED(rc)) {
        fprintf(stderr, "backend_spawn: killed by signal %d: %s\n", WTERMSIG(rc), cmd);
        return -1;
    }
#endif
    return rc;
}

typedef struct {
    float x;
    float y;
} BezierPoint;

static BezierPoint bezier_cubic_eval(float t, BezierPoint p0, BezierPoint p1,
                                     BezierPoint p2, BezierPoint p3)
{
    float u = 1.0f - t;
    float uu = u * u;
    float uuu = uu * u;
    float tt = t * t;
    float ttt = tt * t;
    BezierPoint result;

    result.x = uuu * p0.x + 3.0f * uu * t * p1.x + 3.0f * u * tt * p2.x + ttt * p3.x;
    result.y = uuu * p0.y + 3.0f * uu * t * p1.y + 3.0f * u * tt * p2.y + ttt * p3.y;
    return result;
}

static int bezier_segment_count(int x0, int y0, int x1, int y1)
{
    int dx = x1 - x0;
    int dy = y1 - y0;
    int length = (int)sqrtf((float)(dx * dx + dy * dy));
    int segments = length / 8;

    if (segments < 12) {
        segments = 12;
    }
    if (segments > 64) {
        segments = 64;
    }
    return segments;
}

void backend_render_bezier_cubic(int x0, int y0,
                                 int cx1, int cy1, int cx2, int cy2,
                                 int x1, int y1, Color color, int width)
{
    BezierPoint p0 = {(float)x0, (float)y0};
    BezierPoint p1 = {(float)cx1, (float)cy1};
    BezierPoint p2 = {(float)cx2, (float)cy2};
    BezierPoint p3 = {(float)x1, (float)y1};
    int segments = bezier_segment_count(x0, y0, x1, y1);
    int prev_x = x0;
    int prev_y = y0;
    int i;

    (void)width;

    for (i = 1; i <= segments; i++) {
        float t = (float)i / (float)segments;
        BezierPoint point = bezier_cubic_eval(t, p0, p1, p2, p3);
        int ix = (int)(point.x + 0.5f);
        int iy = (int)(point.y + 0.5f);

        backend_render_line(prev_x, prev_y, ix, iy, color);
        prev_x = ix;
        prev_y = iy;
    }
}

#define BACKEND_CLIP_STACK_MAX 32

static Rect g_clip_stack[BACKEND_CLIP_STACK_MAX];
static int g_clip_depth = 0;

uint32_t backend_get_caps(void)
{
#if defined(YUI_USE_LVGL_BACKEND)
    return BACKEND_CAP_CLIP | BACKEND_CAP_ROUND_RECT;
#else
    return BACKEND_CAP_CLIP | BACKEND_CAP_ROUND_RECT | BACKEND_CAP_BACKDROP |
           BACKEND_CAP_CLIPBOARD | BACKEND_CAP_IME | BACKEND_CAP_SCREENSHOT;
#endif
}

uint32_t backend_color_to_pixel(Color c, int color_depth)
{
    if (color_depth == 32) {
        return ((uint32_t)c.a << 24) | ((uint32_t)c.r << 16) |
               ((uint32_t)c.g << 8) | (uint32_t)c.b;
    }
    if (color_depth == 16) {
        uint16_t r = (uint16_t)(c.r >> 3);
        uint16_t g = (uint16_t)(c.g >> 2);
        uint16_t b = (uint16_t)(c.b >> 3);
        return (uint16_t)((r << 11) | (g << 5) | b);
    }
    return c.r;
}

void backend_clip_push(Rect* clip)
{
    if (!clip || g_clip_depth >= BACKEND_CLIP_STACK_MAX) {
        return;
    }
    g_clip_stack[g_clip_depth++] = *clip;
}

void backend_clip_pop(Rect* prev)
{
    if (g_clip_depth <= 0) {
        if (prev) {
            memset(prev, 0, sizeof(*prev));
        }
        return;
    }
    g_clip_depth--;
    if (prev) {
        *prev = g_clip_stack[g_clip_depth];
    }
}
