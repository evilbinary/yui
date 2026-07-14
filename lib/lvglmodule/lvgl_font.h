#ifndef LVGL_FONT_H
#define LVGL_FONT_H

#include "../../src/ytype.h"

struct _lv_font_t;
typedef struct _lv_font_t lv_font_t;

const lv_font_t* lvgl_font_get(Layer* layer);

#endif
