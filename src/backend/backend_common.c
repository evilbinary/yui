#include "backend_common.h"
#include <string.h>

#define BACKEND_CLIP_STACK_MAX 32

static Rect g_clip_stack[BACKEND_CLIP_STACK_MAX];
static int g_clip_depth = 0;

uint32_t backend_get_caps(void)
{
#if defined(YUI_BACKEND_LVGL)
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
