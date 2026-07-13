#ifndef LVGL_COMPONENT_H
#define LVGL_COMPONENT_H

#include "../../src/ytype.h"

struct _lv_obj_t;
typedef struct _lv_obj_t lv_obj_t;

typedef struct LvglComponent {
    Layer* layer;
    lv_obj_t* obj;
    void* widget_data;
} LvglComponent;

void lvgl_component_sync_rect(LvglComponent* component);
void lvgl_component_destroy(LvglComponent* component);
void lvglmodule_register_all(void);

#endif
