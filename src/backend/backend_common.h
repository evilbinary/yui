#ifndef YUI_BACKEND_COMMON_H
#define YUI_BACKEND_COMMON_H

#include "../ytype.h"

typedef enum {
    BACKEND_CAP_CLIP       = 1u << 0,
    BACKEND_CAP_ROUND_RECT = 1u << 1,
    BACKEND_CAP_BACKDROP   = 1u << 2,
    BACKEND_CAP_CLIPBOARD  = 1u << 3,
    BACKEND_CAP_IME        = 1u << 4,
    BACKEND_CAP_SCREENSHOT = 1u << 5,
} BackendCaps;

uint32_t backend_get_caps(void);
uint32_t backend_color_to_pixel(Color c, int color_depth);
void backend_clip_push(Rect* clip);
void backend_clip_pop(Rect* prev);

#endif
