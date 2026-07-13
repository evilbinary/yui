#include "lv_port.h"
#include "../lvgl.h"

#ifdef D_SDL
#include <SDL2/SDL.h>
#else
#include <SDL.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#ifndef LV_PORT_DEFAULT_WIDTH
#define LV_PORT_DEFAULT_WIDTH 390
#endif
#ifndef LV_PORT_DEFAULT_HEIGHT
#define LV_PORT_DEFAULT_HEIGHT 390
#endif

static SDL_Window* g_window = NULL;
static SDL_Renderer* g_renderer = NULL;
static SDL_Texture* g_texture = NULL;

static lv_disp_drv_t g_disp_drv;
static lv_disp_draw_buf_t g_draw_buf;
static lv_color_t* g_lv_buf1 = NULL;
static lv_color_t* g_lv_buf2 = NULL;
static uint32_t* g_framebuffer = NULL;

static int g_fb_w = LV_PORT_DEFAULT_WIDTH;
static int g_fb_h = LV_PORT_DEFAULT_HEIGHT;
static int g_fb_stride = 0;
static int g_port_ready = 0;

static lv_indev_drv_t g_mouse_drv;
static lv_indev_t* g_mouse_indev = NULL;
static lv_point_t g_mouse_point;
static bool g_mouse_pressed = false;

static lv_obj_t* g_root = NULL;

static void disp_flush(lv_disp_drv_t* disp_drv, const lv_area_t* area, lv_color_t* color_p)
{
    if (!g_framebuffer) {
        lv_disp_flush_ready(disp_drv);
        return;
    }

    int32_t x;
    int32_t y;
    for (y = area->y1; y <= area->y2; y++) {
        uint32_t* dst = g_framebuffer + (y * g_fb_stride) + area->x1;
        lv_color_t* src = color_p;
        for (x = area->x1; x <= area->x2; x++) {
            lv_color_t c = *src++;
            *dst++ = lv_color_to32(c);
        }
        color_p += (area->x2 - area->x1 + 1);
    }

    lv_disp_flush_ready(disp_drv);
}

static void mouse_read(lv_indev_drv_t* drv, lv_indev_data_t* data)
{
    (void)drv;
    data->point = g_mouse_point;
    data->state = g_mouse_pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

int lv_port_init(void)
{
    if (g_port_ready) {
        return 0;
    }

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "lv_port_init: SDL_Init failed: %s\n", SDL_GetError());
        return -1;
    }

    g_window = SDL_CreateWindow(
        "YUI LVGL",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        g_fb_w,
        g_fb_h,
        SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!g_window) {
        fprintf(stderr, "lv_port_init: SDL_CreateWindow failed: %s\n", SDL_GetError());
        return -1;
    }

    g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!g_renderer) {
        fprintf(stderr, "lv_port_init: SDL_CreateRenderer failed: %s\n", SDL_GetError());
        return -1;
    }

    g_texture = SDL_CreateTexture(
        g_renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        g_fb_w,
        g_fb_h);
    if (!g_texture) {
        fprintf(stderr, "lv_port_init: SDL_CreateTexture failed: %s\n", SDL_GetError());
        return -1;
    }

    g_fb_stride = g_fb_w;
    size_t px_count = (size_t)g_fb_w * (size_t)g_fb_h;
    g_framebuffer = (uint32_t*)calloc(px_count, sizeof(uint32_t));
    g_lv_buf1 = (lv_color_t*)calloc(px_count, sizeof(lv_color_t));
    g_lv_buf2 = (lv_color_t*)calloc(px_count, sizeof(lv_color_t));
    if (!g_framebuffer || !g_lv_buf1 || !g_lv_buf2) {
        fprintf(stderr, "lv_port_init: framebuffer allocation failed\n");
        return -1;
    }

    lv_init();
    lv_disp_draw_buf_init(&g_draw_buf, g_lv_buf1, g_lv_buf2, (uint32_t)px_count);

    lv_disp_drv_init(&g_disp_drv);
    g_disp_drv.hor_res = g_fb_w;
    g_disp_drv.ver_res = g_fb_h;
    g_disp_drv.flush_cb = disp_flush;
    g_disp_drv.draw_buf = &g_draw_buf;
    lv_disp_drv_register(&g_disp_drv);

    lv_indev_drv_init(&g_mouse_drv);
    g_mouse_drv.type = LV_INDEV_TYPE_POINTER;
    g_mouse_drv.read_cb = mouse_read;
    g_mouse_indev = lv_indev_drv_register(&g_mouse_drv);

    g_root = lv_obj_create(lv_scr_act());
    lv_obj_set_size(g_root, g_fb_w, g_fb_h);

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

    free(g_framebuffer);
    free(g_lv_buf1);
    free(g_lv_buf2);
    g_framebuffer = NULL;
    g_lv_buf1 = NULL;
    g_lv_buf2 = NULL;

    if (g_texture) {
        SDL_DestroyTexture(g_texture);
        g_texture = NULL;
    }
    if (g_renderer) {
        SDL_DestroyRenderer(g_renderer);
        g_renderer = NULL;
    }
    if (g_window) {
        SDL_DestroyWindow(g_window);
        g_window = NULL;
    }

    SDL_Quit();
    g_port_ready = 0;
}

void lv_port_input_poll(void)
{
    if (!g_port_ready) {
        return;
    }

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
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

void lv_port_flush(void)
{
    if (!g_port_ready || !g_texture || !g_renderer || !g_framebuffer) {
        return;
    }

    SDL_UpdateTexture(g_texture, NULL, g_framebuffer, g_fb_stride * (int)sizeof(uint32_t));
    SDL_RenderClear(g_renderer);
    SDL_RenderCopy(g_renderer, g_texture, NULL, NULL);
    SDL_RenderPresent(g_renderer);
}

void* lv_port_get_draw_buf(void)
{
    return g_framebuffer;
}

int lv_port_get_width(void)
{
    return g_fb_w;
}

int lv_port_get_height(void)
{
    return g_fb_h;
}

int lv_port_get_stride(void)
{
    return g_fb_stride;
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
