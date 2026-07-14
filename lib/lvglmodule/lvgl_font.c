#include "lvgl_font.h"
#include "../../lib/lvgl/lvgl.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    int size;
    const lv_font_t* font;
} LvglBuiltinFont;

static const LvglBuiltinFont g_builtin_fonts[] = {
#if LV_FONT_MONTSERRAT_12
    {12, &lv_font_montserrat_12},
#endif
#if LV_FONT_MONTSERRAT_14
    {14, &lv_font_montserrat_14},
#endif
#if LV_FONT_MONTSERRAT_16
    {16, &lv_font_montserrat_16},
#endif
#if LV_FONT_MONTSERRAT_18
    {18, &lv_font_montserrat_18},
#endif
#if LV_FONT_MONTSERRAT_20
    {20, &lv_font_montserrat_20},
#endif
#if LV_FONT_MONTSERRAT_22
    {22, &lv_font_montserrat_22},
#endif
#if LV_FONT_MONTSERRAT_24
    {24, &lv_font_montserrat_24},
#endif
};

static int lvgl_font_weight_adjust(const char* weight)
{
    if (!weight || !weight[0]) {
        return 0;
    }
    if (strcmp(weight, "bold") == 0 || strcmp(weight, "700") == 0
        || strcmp(weight, "800") == 0 || strcmp(weight, "900") == 0) {
        return 2;
    }
    if (strcmp(weight, "light") == 0 || strcmp(weight, "300") == 0
        || strcmp(weight, "200") == 0 || strcmp(weight, "100") == 0) {
        return -2;
    }
    return 0;
}

static int lvgl_font_target_size(Layer* layer)
{
    int size = 14;
    int adjust = 0;

    if (layer && layer->font && layer->font->size > 0) {
        size = layer->font->size;
    }
    if (layer && layer->font) {
        adjust = lvgl_font_weight_adjust(layer->font->weight);
    }
    size += adjust;
    if (size < 12) {
        size = 12;
    }
    return size;
}

static const lv_font_t* lvgl_font_pick_nearest(int target_size)
{
    const lv_font_t* best = LV_FONT_DEFAULT;
    int best_dist = 0x7fffffff;
    size_t i;

    for (i = 0; i < sizeof(g_builtin_fonts) / sizeof(g_builtin_fonts[0]); i++) {
        int dist = abs(g_builtin_fonts[i].size - target_size);
        if (dist < best_dist) {
            best_dist = dist;
            best = g_builtin_fonts[i].font;
        }
    }
    return best;
}

const lv_font_t* lvgl_font_get(Layer* layer)
{
    return lvgl_font_pick_nearest(lvgl_font_target_size(layer));
}
