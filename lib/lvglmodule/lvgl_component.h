#ifndef LVGL_COMPONENT_H
#define LVGL_COMPONENT_H

#include "../../src/ytype.h"

struct _lv_obj_t;
typedef struct _lv_obj_t lv_obj_t;

typedef struct LvglComponent {
    Layer* layer;
    lv_obj_t* obj;
    void* widget_data;
    EventHandler on_change;
    char on_change_name[MAX_PATH];
} LvglComponent;

void lvgl_component_sync_rect(LvglComponent* component);
void lvgl_apply_layer_rects(Layer* root);
void lvgl_dump_layer_rects(const Layer* layer, int depth);
void lvgl_component_destroy(LvglComponent* component);
void lvgl_widgets_register_all(void);
void lvglmodule_register_all(void);
void lvgl_module_init_layer(Layer* root);

#endif
