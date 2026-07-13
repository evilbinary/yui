/**
 * @file lv_port_indev.c
 */

/*********************
 *      INCLUDES
 *********************/
#include "lv_port_indev.h"

#ifdef D_SDL
#include <SDL2/SDL.h>
#else
#include <SDL.h>
#endif

#include <stdbool.h>

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void mouse_init(void);
static void mouse_read(lv_indev_drv_t* indev_drv, lv_indev_data_t* data);
static bool mouse_is_pressed(void);
static void mouse_get_xy(lv_coord_t* x, lv_coord_t* y);

/**********************
 *  STATIC VARIABLES
 **********************/
lv_indev_t* indev_mouse;

static lv_point_t g_mouse_point;
static bool g_mouse_pressed = false;
static int g_should_quit = 0;

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_port_indev_init(void)
{
    static lv_indev_drv_t indev_drv;

    mouse_init();

    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = mouse_read;
    indev_mouse = lv_indev_drv_register(&indev_drv);

    /* Use SDL system cursor; LVGL software cursor partial refresh erases YUI framebuffer. */
    SDL_ShowCursor(SDL_ENABLE);
}

void lv_port_indev_deinit(void)
{
    indev_mouse = NULL;
    g_mouse_pressed = false;
    g_should_quit = 0;
}

void lv_port_indev_poll(void)
{
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            g_should_quit = 1;
            continue;
        }
        if (event.type == SDL_MOUSEMOTION) {
            g_mouse_point.x = event.motion.x;
            g_mouse_point.y = event.motion.y;
        } else if (event.type == SDL_MOUSEBUTTONDOWN) {
            g_mouse_point.x = event.button.x;
            g_mouse_point.y = event.button.y;
            g_mouse_pressed = true;
        } else if (event.type == SDL_MOUSEBUTTONUP) {
            g_mouse_point.x = event.button.x;
            g_mouse_point.y = event.button.y;
            g_mouse_pressed = false;
        }
    }
}

int lv_port_indev_should_quit(void)
{
    return g_should_quit;
}

void lv_port_indev_request_quit(void)
{
    g_should_quit = 1;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void mouse_init(void)
{
    g_mouse_point.x = 0;
    g_mouse_point.y = 0;
    g_mouse_pressed = false;
}

static void mouse_read(lv_indev_drv_t* indev_drv, lv_indev_data_t* data)
{
    (void)indev_drv;

    mouse_get_xy(&data->point.x, &data->point.y);
    if (mouse_is_pressed()) {
        data->state = LV_INDEV_STATE_PR;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

static bool mouse_is_pressed(void)
{
    return g_mouse_pressed;
}

static void mouse_get_xy(lv_coord_t* x, lv_coord_t* y)
{
    if (x) {
        *x = g_mouse_point.x;
    }
    if (y) {
        *y = g_mouse_point.y;
    }
}
