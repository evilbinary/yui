#ifndef YUI_COMPONENT_REGISTRY_H
#define YUI_COMPONENT_REGISTRY_H

#include "ytype.h"
#include "cJSON.h"

#define YUI_COMP_FOCUSABLE       (1u << 0)
#define YUI_COMP_CUSTOM_CHILDREN (1u << 1)
#define YUI_COMP_SKIP_CHILDREN   (1u << 2)
#define YUI_COMP_NATIVE_RENDER   (1u << 3)
#define YUI_COMP_LVGL_WIDGET     (1u << 4)

#define YUI_BACKEND_SDL  (1u << 0)
#define YUI_BACKEND_LVGL (1u << 1)
#define YUI_BACKEND_ALL  (YUI_BACKEND_SDL | YUI_BACKEND_LVGL)

#define YUI_MAX_TYPES 128

typedef void* (*YuiComponentCreateFn)(Layer* layer, cJSON* json);

typedef struct YuiComponentOps {
    YuiComponentCreateFn create;
    void (*destroy)(Layer* layer);
    void (*on_layout)(Layer* layer);
    int  (*handle_mouse)(Layer* layer, MouseEvent* e);
    int  (*handle_key)(Layer* layer, KeyEvent* e);
    void (*after_create)(Layer* layer, cJSON* json);
    uint32_t flags;
    uint32_t backend_mask;
} YuiComponentOps;

typedef struct YuiComponentTypeEntry {
    int                    type_id;
    const char*            type_name;
    const YuiComponentOps* ops;
} YuiComponentTypeEntry;

void yui_component_registry_init(void);

int yui_component_register_builtin(int type_id,
                                 const char* type_name,
                                 const YuiComponentOps* ops);

int yui_component_register(const char* type_name, const YuiComponentOps* ops);

int         yui_type_resolve(const char* type_name);
const char* yui_type_name(int type_id);
const YuiComponentOps* yui_type_get_ops(int type_id);
int         yui_type_count(void);
const YuiComponentTypeEntry* yui_type_entry_at(int index);

uint32_t yui_get_current_backend_mask(void);

int yui_component_instantiate(Layer* layer, cJSON* json, Layer* parent,
                              int* out_custom_children);

void yui_components_register_builtin(void);

#endif
