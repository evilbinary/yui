/**
 * @file lv_port_disp.c
 */

/*********************
 *      INCLUDES
 *********************/
#include "lv_port_disp.h"
#include "../lvgl.h"

#ifdef D_SDL
#include <SDL2/SDL.h>
#else
#include <SDL.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

/*********************
 *      DEFINES
 *********************/
#ifndef LV_PORT_DISP_HOR_RES
#define LV_PORT_DISP_HOR_RES 390
#endif
#ifndef LV_PORT_DISP_VER_RES
#define LV_PORT_DISP_VER_RES 390
#endif
#ifndef LV_PORT_DRAW_BUF_LINES
#define LV_PORT_DRAW_BUF_LINES 40
#endif

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void disp_init(void);
static void disp_flush(lv_disp_drv_t* disp_drv, const lv_area_t* area, lv_color_t* color_p);

/**********************
 *  STATIC VARIABLES
 **********************/
static SDL_Window* g_window = NULL;
static SDL_Renderer* g_renderer = NULL;
static SDL_Texture* g_texture = NULL;

static lv_disp_drv_t g_disp_drv;
static lv_disp_draw_buf_t g_draw_buf;
static lv_color_t* g_lv_buf1 = NULL;
static lv_color_t* g_lv_buf2 = NULL;
static uint32_t* g_framebuffer = NULL;

static int g_fb_w = LV_PORT_DISP_HOR_RES;
static int g_fb_h = LV_PORT_DISP_VER_RES;
static int g_fb_stride = 0;
static int g_disp_ready = 0;

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_port_disp_init(void)
{
    if (g_disp_ready) {
        return;
    }

    disp_init();

    uint32_t buf_size = (uint32_t)g_fb_w * LV_PORT_DRAW_BUF_LINES;
    g_lv_buf1 = (lv_color_t*)calloc(buf_size, sizeof(lv_color_t));
    g_lv_buf2 = (lv_color_t*)calloc(buf_size, sizeof(lv_color_t));
    size_t px_count = (size_t)g_fb_w * (size_t)g_fb_h;
    g_framebuffer = (uint32_t*)calloc(px_count, sizeof(uint32_t));
    if (!g_lv_buf1 || !g_lv_buf2 || !g_framebuffer) {
        fprintf(stderr, "lv_port_disp_init: buffer allocation failed\n");
        return;
    }

    lv_disp_draw_buf_init(&g_draw_buf, g_lv_buf1, g_lv_buf2, buf_size);

    lv_disp_drv_init(&g_disp_drv);
    g_disp_drv.hor_res = g_fb_w;
    g_disp_drv.ver_res = g_fb_h;
    g_disp_drv.flush_cb = disp_flush;
    g_disp_drv.draw_buf = &g_draw_buf;
    lv_disp_drv_register(&g_disp_drv);

    g_disp_ready = 1;
}

void lv_port_disp_deinit(void)
{
    if (!g_disp_ready) {
        return;
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
    g_disp_ready = 0;
}

void lv_port_disp_flush(void)
{
    if (!g_disp_ready || !g_texture || !g_renderer || !g_framebuffer) {
        return;
    }

    SDL_UpdateTexture(g_texture, NULL, g_framebuffer, g_fb_stride * (int)sizeof(uint32_t));
    SDL_RenderClear(g_renderer);
    SDL_RenderCopy(g_renderer, g_texture, NULL, NULL);
    SDL_RenderPresent(g_renderer);
}

SDL_Renderer* lv_port_disp_get_renderer(void)
{
    return g_renderer;
}

void* lv_port_disp_get_draw_buf(void)
{
    return g_framebuffer;
}

int lv_port_disp_get_width(void)
{
    return g_fb_w;
}

int lv_port_disp_get_height(void)
{
    return g_fb_h;
}

int lv_port_disp_get_stride(void)
{
    return g_fb_stride;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void disp_init(void)
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "lv_port_disp_init: SDL_Init failed: %s\n", SDL_GetError());
        return;
    }

    g_window = SDL_CreateWindow(
        "YUI LVGL",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        g_fb_w,
        g_fb_h,
        SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!g_window) {
        fprintf(stderr, "lv_port_disp_init: SDL_CreateWindow failed: %s\n", SDL_GetError());
        return;
    }

    g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!g_renderer) {
        fprintf(stderr, "lv_port_disp_init: SDL_CreateRenderer failed: %s\n", SDL_GetError());
        return;
    }

    g_texture = SDL_CreateTexture(
        g_renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        g_fb_w,
        g_fb_h);
    if (!g_texture) {
        fprintf(stderr, "lv_port_disp_init: SDL_CreateTexture failed: %s\n", SDL_GetError());
        return;
    }

    g_fb_stride = g_fb_w;
}

static uint32_t blend_pixel(uint32_t dst, uint32_t src)
{
    uint8_t sa = (uint8_t)(src >> 24);
    if (sa == 0) {
        return dst;
    }
    if (sa == 255) {
        return src;
    }

    uint8_t sr = (uint8_t)((src >> 16) & 0xFF);
    uint8_t sg = (uint8_t)((src >> 8) & 0xFF);
    uint8_t sb = (uint8_t)(src & 0xFF);

    uint8_t dr = (uint8_t)((dst >> 16) & 0xFF);
    uint8_t dg = (uint8_t)((dst >> 8) & 0xFF);
    uint8_t db = (uint8_t)(dst & 0xFF);

    uint8_t inv = (uint8_t)(255 - sa);
    uint8_t r = (uint8_t)((sr * sa + dr * inv) / 255);
    uint8_t g = (uint8_t)((sg * sa + dg * inv) / 255);
    uint8_t b = (uint8_t)((sb * sa + db * inv) / 255);
    return ((uint32_t)255 << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

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
            uint32_t pixel = lv_color_to32(*src++);
#if LV_COLOR_SCREEN_TRANSP
            *dst = blend_pixel(*dst, pixel);
            dst++;
#else
            *dst++ = pixel;
#endif
        }
        color_p += (area->x2 - area->x1 + 1);
    }

    lv_disp_flush_ready(disp_drv);
}
