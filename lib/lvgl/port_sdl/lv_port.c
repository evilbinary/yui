/**
 * @file lv_port.c
 *
 * YUI unified port API — delegates to lv_port_disp / lv_port_indev.
 */

#include "../lv_port.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"

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
    lv_obj_set_pos(g_root, 0, 0);
    lv_obj_set_style_pad_all(g_root, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_row(g_root, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_column(g_root, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(lv_scr_act(), 0, 0);
    lv_obj_clear_flag(lv_scr_act(), LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(g_root, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(g_root, 0, 0);
    lv_obj_clear_flag(g_root, LV_OBJ_FLAG_SCROLLABLE);

    lv_disp_t* disp = lv_disp_get_default();
    if (disp) {
        lv_disp_set_bg_opa(disp, LV_OPA_TRANSP);
    }

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

int lv_port_resize(int width, int height)
{
    if (lv_port_disp_resize(width, height) != 0) {
        return -1;
    }
    if (g_root) {
        lv_obj_set_size(g_root, (lv_coord_t)width, (lv_coord_t)height);
    }
    return 0;
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

SDL_Renderer* lv_port_get_renderer(void)
{
    return lv_port_disp_get_renderer();
}

void lv_port_set_sdl_event_hook(lv_port_sdl_event_fn fn, void* user_data)
{
    lv_port_indev_set_sdl_event_hook(fn, user_data);
}
