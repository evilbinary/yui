/**
 * @file lv_port_indev.h
 */

#ifndef LV_PORT_INDEV_H
#define LV_PORT_INDEV_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../lvgl.h"

void lv_port_indev_init(void);
void lv_port_indev_deinit(void);
void lv_port_indev_poll(void);
int lv_port_indev_should_quit(void);
void lv_port_indev_request_quit(void);

typedef void (*lv_port_sdl_event_fn)(const SDL_Event* event, void* user_data);
void lv_port_indev_set_sdl_event_hook(lv_port_sdl_event_fn fn, void* user_data);

#ifdef __cplusplus
}
#endif

#endif /*LV_PORT_INDEV_H*/
