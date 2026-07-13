#ifndef LV_PORT_H
#define LV_PORT_H

#ifdef __cplusplus
extern "C" {
#endif

struct _lv_obj_t;
typedef struct _lv_obj_t lv_obj_t;

int  lv_port_init(void);
void lv_port_deinit(void);
void lv_port_input_poll(void);
void lv_port_flush(void);
void* lv_port_get_draw_buf(void);
int lv_port_get_width(void);
int lv_port_get_height(void);
int lv_port_get_stride(void);
lv_obj_t* lv_port_get_root(void);
void lv_port_tick_inc(void);

#ifdef __cplusplus
}
#endif

#endif
