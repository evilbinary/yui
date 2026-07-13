/**
 * @file lv_port.c
 *
 * YUI unified port API — STM32 / YiYiYa board (delegates to lv_port_disp / lv_port_indev).
 */

#include "../lv_port.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"

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
    lv_port_indev_init();

    g_root = lv_obj_create(lv_scr_act());
    lv_obj_set_size(g_root, lv_port_get_width(), lv_port_get_height());
    lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(lv_scr_act(), 0, 0);
    lv_obj_clear_flag(lv_scr_act(), LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(g_root, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(g_root, 0, 0);
    lv_obj_clear_flag(g_root, LV_OBJ_FLAG_SCROLLABLE);

    {
        lv_disp_t* disp = lv_disp_get_default();
        if (disp) {
            lv_disp_set_bg_opa(disp, LV_OPA_TRANSP);
        }
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

    g_port_ready = 0;
}

void lv_port_input_poll(void)
{
    /* Board input is polled inside lv_port_indev read callbacks. */
}

void lv_port_flush(void)
{
    /* Display flush happens in disp_flush; optional board present hook here. */
}

void* lv_port_get_draw_buf(void)
{
    return NULL;
}

int lv_port_get_width(void)
{
    lv_disp_t* disp = lv_disp_get_default();
    if (!disp || !disp->driver) {
        return 0;
    }
    return (int)disp->driver->hor_res;
}

int lv_port_get_height(void)
{
    lv_disp_t* disp = lv_disp_get_default();
    if (!disp || !disp->driver) {
        return 0;
    }
    return (int)disp->driver->ver_res;
}

int lv_port_get_stride(void)
{
    return lv_port_get_width();
}

lv_obj_t* lv_port_get_root(void)
{
    return g_root;
}

void lv_port_tick_inc(void)
{
    /* Board tick should call lv_tick_inc() from ISR or HAL; no-op here. */
}

int lv_port_should_quit(void)
{
    return 0;
}

void lv_port_request_quit(void)
{
}
