/**
 * @file lv_port.c
 *
 * YUI unified port API — delegates to lv_port_disp / lv_port_indev.
 */

#include "lv_port.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"
#include "../lvgl.h"

#ifdef D_SDL
#include <SDL2/SDL.h>
#else
#include <SDL.h>
#endif

#include <stdio.h>

static lv_obj_t* g_root = NULL;
static int g_port_ready = 0;

int lv_port_init(void)
{
    if (g_port_ready) {
        return 0;
    }

    lv_init();
    lv_port_disp_init();

    if (!lv_port_disp_get_draw_buf()) {
        fprintf(stderr, "lv_port_init: display init failed\n");
        return -1;
    }

    lv_port_indev_init();

    g_root = lv_obj_create(lv_scr_act());
    lv_obj_set_size(g_root, lv_port_disp_get_width(), lv_port_disp_get_height());

    g_port_ready = 1;
    return 0;
}

void lv_port_deinit(void)
{
    if (!g_port_ready) {
        return;
    }

    if (g_root) {
        lv_obj_del(g_root);
        g_root = NULL;
    }

    lv_port_indev_deinit();
    lv_port_disp_deinit();
    g_port_ready = 0;
}

void lv_port_input_poll(void)
{
    lv_port_indev_poll();
}

void lv_port_flush(void)
{
    lv_port_disp_flush();
}

void* lv_port_get_draw_buf(void)
{
    return lv_port_disp_get_draw_buf();
}

int lv_port_get_width(void)
{
    return lv_port_disp_get_width();
}

int lv_port_get_height(void)
{
    return lv_port_disp_get_height();
}

int lv_port_get_stride(void)
{
    return lv_port_disp_get_stride();
}

lv_obj_t* lv_port_get_root(void)
{
    return g_root;
}

void lv_port_tick_inc(void)
{
    static Uint32 last_tick = 0;
    Uint32 now = SDL_GetTicks();
    if (last_tick == 0) {
        last_tick = now;
        return;
    }
    Uint32 diff = now - last_tick;
    last_tick = now;
    lv_tick_inc(diff);
}

int lv_port_should_quit(void)
{
    return lv_port_indev_should_quit();
}

void lv_port_request_quit(void)
{
    lv_port_indev_request_quit();
}
