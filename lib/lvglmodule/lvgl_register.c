#include "lvgl_component.h"
#include "../../lib/lvgl/lvgl.h"
#include "../../lib/lvgl/lv_port.h"
#include "lvgl_widget.h"

void lvgl_component_sync_rect(LvglComponent* component)
{
    lv_coord_t x;
    lv_coord_t y;
    lv_coord_t w;
    lv_coord_t h;

    if (!component || !component->obj || !component->layer) {
        return;
    }

    x = (lv_coord_t)component->layer->rect.x;
    y = (lv_coord_t)component->layer->rect.y;
    w = (lv_coord_t)component->layer->rect.w;
    h = (lv_coord_t)component->layer->rect.h;

    if (w <= 0 || h <= 0) {
        return;
    }

    lv_obj_set_pos(component->obj, x, y);
    lv_obj_set_size(component->obj, w, h);
    lv_obj_refr_size(component->obj);
    lv_obj_refr_pos(component->obj);
    lv_obj_invalidate(component->obj);
}

static void lvgl_apply_layer_rects_walk(Layer* layer)
{
    int i;
    LvglComponent* component;

    if (!layer || layer->visible == IN_VISIBLE) {
        return;
    }

    component = lvgl_component_from_layer(layer);
    if (component) {
        lvgl_component_sync_rect(component);
    }

    if (layer->children) {
        for (i = 0; i < layer->child_count; i++) {
            if (layer->children[i]) {
                lvgl_apply_layer_rects_walk(layer->children[i]);
            }
        }
    }
    if (layer->sub) {
        lvgl_apply_layer_rects_walk(layer->sub);
    }
}

void lvgl_apply_layer_rects(Layer* root)
{
    lv_obj_t* lv_root;

    if (!root) {
        return;
    }

    lv_root = lv_port_get_root();
    if (lv_root) {
        lv_obj_refr_size(lv_root);
        lv_obj_refr_pos(lv_root);
    }

    lvgl_apply_layer_rects_walk(root);
}

void lvgl_dump_layer_rects(const Layer* layer, int depth)
{
    int i;
    int pad;
    LvglComponent* component;
    lv_area_t coords;

    if (!layer) {
        return;
    }

    for (pad = 0; pad < depth; pad++) {
        fprintf(stdout, "  ");
    }

    component = lvgl_component_from_layer((Layer*)layer);
    if (component && component->obj) {
        lv_obj_get_coords(component->obj, &coords);
        fprintf(stdout, "[%s] layer=(%d,%d %dx%d) lvgl=(%d,%d %dx%d)\n",
                layer->id ? layer->id : "(null)",
                layer->rect.x, layer->rect.y, layer->rect.w, layer->rect.h,
                coords.x1, coords.y1,
                coords.x2 - coords.x1 + 1, coords.y2 - coords.y1 + 1);
    } else {
        fprintf(stdout, "[%s] layer=(%d,%d %dx%d)\n",
                layer->id ? layer->id : "(null)",
                layer->rect.x, layer->rect.y, layer->rect.w, layer->rect.h);
    }

    if (layer->children) {
        for (i = 0; i < layer->child_count; i++) {
            if (layer->children[i]) {
                lvgl_dump_layer_rects(layer->children[i], depth + 1);
            }
        }
    }
    if (layer->sub) {
        lvgl_dump_layer_rects(layer->sub, depth + 1);
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
