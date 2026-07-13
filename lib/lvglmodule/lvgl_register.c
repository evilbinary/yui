#include "lvgl_component.h"
#include "../../lib/lvgl/lvgl.h"
#include "lvgl_widget.h"

void lvgl_component_sync_rect(LvglComponent* component)
{
    if (!component || !component->obj || !component->layer) {
        return;
    }

    lv_coord_t x = (lv_coord_t)component->layer->rect.x;
    lv_coord_t y = (lv_coord_t)component->layer->rect.y;
    lv_coord_t w = (lv_coord_t)component->layer->rect.w;
    lv_coord_t h = (lv_coord_t)component->layer->rect.h;
    int changed = 0;

    if (lv_obj_get_x(component->obj) != x || lv_obj_get_y(component->obj) != y) {
        lv_obj_set_pos(component->obj, x, y);
        changed = 1;
    }
    if (lv_obj_get_width(component->obj) != w || lv_obj_get_height(component->obj) != h) {
        lv_obj_set_size(component->obj, w, h);
        changed = 1;
    }
    if (changed) {
        lv_obj_invalidate(component->obj);
    }
}

void lvgl_component_destroy(LvglComponent* component)
{
    if (!component) {
        return;
    }
    if (component->obj) {
        lv_obj_del(component->obj);
        component->obj = NULL;
    }
    free(component);
}

void lvglmodule_register_all(void)
{
    lvgl_widgets_register_all();
}
