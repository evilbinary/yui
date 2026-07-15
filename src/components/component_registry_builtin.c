#include "../component_registry.h"
#include "button_component.h"
#include "input_component.h"
#include "label_component.h"
#include "image_component.h"
#include "list_component.h"
#include "progress_component.h"
#include "checkbox_component.h"
#include "radiobox_component.h"
#include "text_component.h"
#include "treeview_component.h"
#include "tab_component.h"
#include "slider_component.h"
#include "select_component.h"
#include "scrollbar_component.h"
#include "menu_component.h"
#include "dialog_component.h"
#include "clock_component.h"
#include "sash_component.h"
#include "table_component.h"
#include "pagination_component.h"
#include "loading_component.h"
#include "grid_component.h"

static void scrollbar_after_create(Layer* layer, cJSON* json);
static void table_after_create(Layer* layer, cJSON* json);

static const struct {
    int                 type_id;
    const char*         name;
    YuiComponentCreateFn create;
    uint32_t            flags;
    void                (*after_create)(Layer*, cJSON*);
} g_builtin[] = {
    { VIEW,        "View",       NULL, YUI_COMP_NATIVE_RENDER, NULL },
    { BUTTON,      "Button",     (YuiComponentCreateFn)button_component_create_from_json,
      YUI_COMP_NATIVE_RENDER | YUI_COMP_FOCUSABLE, NULL },
    { INPUT_FIELD, "Input",      (YuiComponentCreateFn)input_component_create_from_json,
      YUI_COMP_NATIVE_RENDER | YUI_COMP_FOCUSABLE, NULL },
    { LABEL,       "Label",      (YuiComponentCreateFn)label_component_create_from_json,
      YUI_COMP_NATIVE_RENDER, NULL },
    { IMAGE,       "Image",      image_component_create_from_json, YUI_COMP_NATIVE_RENDER, NULL },
    { LAYER_LIST,  "List",       (YuiComponentCreateFn)list_component_create_from_json,
      YUI_COMP_NATIVE_RENDER | YUI_COMP_FOCUSABLE | YUI_COMP_CUSTOM_CHILDREN, NULL },
    { GRID,        "Grid",       (YuiComponentCreateFn)grid_component_create_from_json,
      YUI_COMP_NATIVE_RENDER, NULL },
    { PROGRESS,    "Progress",   (YuiComponentCreateFn)progress_component_create_from_json,
      YUI_COMP_NATIVE_RENDER, NULL },
    { CHECKBOX,    "Checkbox",   (YuiComponentCreateFn)checkbox_component_create_from_json,
      YUI_COMP_NATIVE_RENDER, NULL },
    { RADIOBOX,    "Radiobox",   (YuiComponentCreateFn)radiobox_component_create_from_json,
      YUI_COMP_NATIVE_RENDER, NULL },
    { TEXT,        "Text",       (YuiComponentCreateFn)text_component_create_from_json,
      YUI_COMP_NATIVE_RENDER | YUI_COMP_FOCUSABLE, NULL },
    { TREEVIEW,    "Treeview",   (YuiComponentCreateFn)treeview_component_create_from_json,
      YUI_COMP_NATIVE_RENDER, NULL },
    { TAB,         "Tab",        (YuiComponentCreateFn)tab_component_create_from_json,
      YUI_COMP_NATIVE_RENDER, NULL },
    { SLIDER,      "Slider",     (YuiComponentCreateFn)slider_component_create_from_json,
      YUI_COMP_NATIVE_RENDER, NULL },
    { SELECT,      "Select",     (YuiComponentCreateFn)select_component_create_from_json,
      YUI_COMP_NATIVE_RENDER | YUI_COMP_FOCUSABLE, NULL },
    { SCROLLBAR,   "Scrollbar",  scrollbar_component_create_from_json_adapter,
      YUI_COMP_SKIP_CHILDREN, scrollbar_after_create },
    { MENU,        "Menu",       (YuiComponentCreateFn)menu_component_create_from_json,
      YUI_COMP_NATIVE_RENDER, NULL },
    { DIALOG,      "Dialog",     (YuiComponentCreateFn)dialog_component_create_from_json,
      YUI_COMP_NATIVE_RENDER, NULL },
    { CLOCK,       "Clock",      (YuiComponentCreateFn)clock_component_create_from_json,
      YUI_COMP_NATIVE_RENDER, NULL },
    { SASH,        "Sash",       (YuiComponentCreateFn)sash_component_create_from_json,
      YUI_COMP_NATIVE_RENDER, NULL },
    { TABLE,       "Table",      (YuiComponentCreateFn)table_component_create_from_json,
      YUI_COMP_NATIVE_RENDER | YUI_COMP_FOCUSABLE | YUI_COMP_CUSTOM_CHILDREN, table_after_create },
    { PAGINATION,  "Pagination", (YuiComponentCreateFn)pagination_component_create_from_json,
      YUI_COMP_NATIVE_RENDER | YUI_COMP_CUSTOM_CHILDREN, NULL },
    { LOADING,     "Loading",    (YuiComponentCreateFn)loading_component_create_from_json,
      YUI_COMP_NATIVE_RENDER, NULL },
};

static void scrollbar_after_create(Layer* layer, cJSON* json)
{
    (void)json;
    layer->child_count = 0;
    if (layer->children) {
        free(layer->children);
        layer->children = NULL;
    }
}

static void table_after_create(Layer* layer, cJSON* json)
{
    (void)json;
    if (layer->data && layer->data->json && layer->on_data_update) {
        layer->on_data_update(layer, layer->data->json);
    }
}

void yui_components_register_builtin(void)
{
    yui_component_registry_init();

    for (size_t i = 0; i < sizeof(g_builtin) / sizeof(g_builtin[0]); i++) {
        const YuiComponentOps ops = {
            .create       = g_builtin[i].create,
            .after_create = g_builtin[i].after_create,
            .flags        = g_builtin[i].flags,
            .backend_mask = YUI_BACKEND_ALL,
        };
        yui_component_register_builtin(
            g_builtin[i].type_id,
            g_builtin[i].name,
            &ops);
    }
}
