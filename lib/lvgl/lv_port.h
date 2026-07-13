/**
 * @file lv_port.h
 *
 * YUI unified LVGL port API.
 * Implementations live under port_sdl / port_yiyiya (stm32) / future ports.
 * Build selects one port via YUI_LVGL_PORT_SDL or YUI_LVGL_PORT_STM32.
 */

#ifndef YUI_LV_PORT_H
#define YUI_LV_PORT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

int  lv_port_init(void);
void lv_port_deinit(void);
void lv_port_input_poll(void);
void lv_port_flush(void);
void* lv_port_get_draw_buf(void);
int  lv_port_get_width(void);
int  lv_port_get_height(void);
int  lv_port_get_stride(void);
lv_obj_t* lv_port_get_root(void);
void lv_port_tick_inc(void);
int  lv_port_should_quit(void);
void lv_port_request_quit(void);

#if defined(YUI_LVGL_PORT_SDL)
#ifdef D_SDL
#include <SDL2/SDL.h>
#else
#include <SDL.h>
#endif
typedef struct SDL_Renderer SDL_Renderer;
typedef void (*lv_port_sdl_event_fn)(const SDL_Event* event, void* user_data);
SDL_Renderer* lv_port_get_renderer(void);
void lv_port_set_sdl_event_hook(lv_port_sdl_event_fn fn, void* user_data);
#endif

#ifdef __cplusplus
}
#endif

#endif /*YUI_LV_PORT_H*/
