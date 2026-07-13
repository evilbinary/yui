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

#ifdef __cplusplus
}
#endif

#endif /*LV_PORT_INDEV_H*/
