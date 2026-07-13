/**
 * @file lv_port_disp.h
 */

#ifndef LV_PORT_DISP_H
#define LV_PORT_DISP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../lvgl.h"

void lv_port_disp_init(void);
void lv_port_disp_deinit(void);
void lv_port_disp_flush(void);

void* lv_port_disp_get_draw_buf(void);
int lv_port_disp_get_width(void);
int lv_port_disp_get_height(void);
int lv_port_disp_get_stride(void);

#ifdef __cplusplus
}
#endif

#endif /*LV_PORT_DISP_H*/
