#include "lvgl_component.h"
#include "../../lib/lvgl/lvgl.h"
#include "lvgl_widget.h"

void lvgl_component_sync_rect(LvglComponent* component)
{
    lv_coord_t x;
    lv_coord_t y;
    lv_coord_t w;
    lv_coord_t h;
    int changed = 0;

    if (!component || !component->obj || !component->layer) {
        return;
    }

    x = (lv_coord_t)component->layer->rect.x;
    y = (lv_coord_t)component->layer->rect.y;
    w = (lv_coord_t)component->layer->rect.w;
    h = (lv_coord_t)component->layer->rect.h;

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

#if LV_USE_CALENDAR && LV_USE_CALENDAR_HEADER_ARROW
__attribute__((used)) static const void* lvgl_force_link_calendar_header_arrow =
    &lv_calendar_header_arrow_class;
#endif
#if LV_USE_CALENDAR && LV_USE_CALENDAR_HEADER_DROPDOWN
__attribute__((used)) static const void* lvgl_force_link_calendar_header_dropdown =
    &lv_calendar_header_dropdown_class;
#endif
#if LV_USE_MSGBOX
__attribute__((used)) static const void* lvgl_force_link_msgbox_class = &lv_msgbox_class;
__attribute__((used)) static const void* lvgl_force_link_msgbox_backdrop_class =
    &lv_msgbox_backdrop_class;
#endif

void lvglmodule_register_all(void)
{
    lvgl_widgets_register_all();
}

void lvgl_module_init_layer(Layer* root)
{
    if (!root) {
        return;
    }
    lvgl_bind_all_layer_events(root);
}
