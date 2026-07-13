#include "lvgl_component.h"
#include "component_registry.h"
#include "../../lib/lvgl/lvgl.h"
#include "../../lib/lvgl/lv_port.h"
#include "cJSON.h"

#include <stdlib.h>

void lvgl_component_sync_rect(LvglComponent* component)
{
    if (!component || !component->obj || !component->layer) {
        return;
    }

    lv_obj_set_pos(component->obj, component->layer->rect.x, component->layer->rect.y);
    lv_obj_set_size(component->obj, component->layer->rect.w, component->layer->rect.h);
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

#if LV_USE_LABEL

static void* lvgl_label_create(Layer* layer, cJSON* json)
{
    (void)json;
    LvglComponent* component = (LvglComponent*)calloc(1, sizeof(LvglComponent));
    if (!component) {
        return NULL;
    }

    component->layer = layer;
    component->obj = lv_label_create(lv_port_get_root());
    if (layer->text && layer->text[0]) {
        lv_label_set_text(component->obj, layer->text);
    }
    lvgl_component_sync_rect(component);
    return component;
}

static void lvgl_label_destroy(Layer* layer)
{
    if (!layer || !layer->component) {
        return;
    }
    lvgl_component_destroy((LvglComponent*)layer->component);
    layer->component = NULL;
}

static void lvgl_label_layout(Layer* layer)
{
    if (!layer || !layer->component) {
        return;
    }
    lvgl_component_sync_rect((LvglComponent*)layer->component);
}

#endif

void lvglmodule_register_all(void)
{
#if LV_USE_LABEL
    yui_component_register("LvLabel", &(YuiComponentOps){
        .create       = lvgl_label_create,
        .destroy      = lvgl_label_destroy,
        .on_layout    = lvgl_label_layout,
        .flags        = YUI_COMP_LVGL_WIDGET,
        .backend_mask = YUI_BACKEND_LVGL,
    });
#endif
}
