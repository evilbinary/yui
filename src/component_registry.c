#include "component_registry.h"
#include "log.h"
#include <stdio.h>
#include <string.h>

static int g_registry_ready = 0;

static YuiComponentOps g_builtin_ops[LAYER_TYPE_BUILTIN_MAX];
static char g_builtin_names[LAYER_TYPE_BUILTIN_MAX][32];
static int g_builtin_registered[LAYER_TYPE_BUILTIN_MAX];

typedef struct {
    int type_id;
    char type_name[64];
    YuiComponentOps ops;
} DynamicTypeEntry;

static DynamicTypeEntry g_dynamic[YUI_MAX_TYPES];
static int g_dynamic_count = 0;
static int g_next_dynamic_id = LAYER_TYPE_USER_BASE;

uint32_t yui_get_current_backend_mask(void)
{
#if defined(YUI_BACKEND_LVGL)
    return YUI_BACKEND_LVGL;
#elif defined(YUI_BACKEND_SDL)
    return YUI_BACKEND_SDL;
#else
    return YUI_BACKEND_ALL;
#endif
}

void yui_component_registry_init(void)
{
    if (g_registry_ready) {
        return;
    }

    memset(g_builtin_ops, 0, sizeof(g_builtin_ops));
    memset(g_builtin_names, 0, sizeof(g_builtin_names));
    memset(g_builtin_registered, 0, sizeof(g_builtin_registered));
    memset(g_dynamic, 0, sizeof(g_dynamic));
    g_dynamic_count = 0;
    g_next_dynamic_id = LAYER_TYPE_USER_BASE;
    g_registry_ready = 1;
}

int yui_component_register_builtin(int type_id, const char* type_name,
                                 const YuiComponentOps* ops)
{
    if (!g_registry_ready) {
        yui_component_registry_init();
    }
    if (!type_name || !ops) {
        return -1;
    }
    if (type_id < 0 || type_id >= LAYER_TYPE_BUILTIN_MAX) {
        LOGE("registry", "invalid builtin type_id %d", type_id);
        return -1;
    }

    g_builtin_ops[type_id] = *ops;
    strncpy(g_builtin_names[type_id], type_name, sizeof(g_builtin_names[type_id]) - 1);
    g_builtin_names[type_id][sizeof(g_builtin_names[type_id]) - 1] = '\0';
    g_builtin_registered[type_id] = 1;
    return 0;
}

int yui_component_register(const char* type_name, const YuiComponentOps* ops)
{
    if (!g_registry_ready) {
        yui_component_registry_init();
    }
    if (!type_name || !ops) {
        return -1;
    }
    if (g_dynamic_count >= YUI_MAX_TYPES) {
        LOGW("registry", "dynamic type table full, cannot register '%s'", type_name);
        return -1;
    }

    for (int i = 0; i < g_dynamic_count; i++) {
        if (strcmp(g_dynamic[i].type_name, type_name) == 0) {
            g_dynamic[i].ops = *ops;
            return g_dynamic[i].type_id;
        }
    }

    int type_id = g_next_dynamic_id++;
    DynamicTypeEntry* entry = &g_dynamic[g_dynamic_count++];
    entry->type_id = type_id;
    strncpy(entry->type_name, type_name, sizeof(entry->type_name) - 1);
    entry->type_name[sizeof(entry->type_name) - 1] = '\0';
    entry->ops = *ops;
    return type_id;
}

static const YuiComponentOps* lookup_ops(int type_id)
{
    if (type_id >= 0 && type_id < LAYER_TYPE_BUILTIN_MAX && g_builtin_registered[type_id]) {
        return &g_builtin_ops[type_id];
    }

    for (int i = 0; i < g_dynamic_count; i++) {
        if (g_dynamic[i].type_id == type_id) {
            return &g_dynamic[i].ops;
        }
    }
    return NULL;
}

int yui_type_resolve(const char* type_name)
{
    if (!type_name || !type_name[0]) {
        return VIEW;
    }
    if (!g_registry_ready) {
        yui_component_registry_init();
    }

    if (strcmp(type_name, "main") == 0 || strcmp(type_name, "app") == 0) {
        return VIEW;
    }

    for (int i = 0; i < LAYER_TYPE_BUILTIN_MAX; i++) {
        if (g_builtin_registered[i] && strcmp(g_builtin_names[i], type_name) == 0) {
            return i;
        }
    }

    for (int i = 0; i < g_dynamic_count; i++) {
        if (strcmp(g_dynamic[i].type_name, type_name) == 0) {
            return g_dynamic[i].type_id;
        }
    }

    LOGW("registry", "unknown component type '%s', fallback to View", type_name);
    return VIEW;
}

const char* yui_type_name(int type_id)
{
    if (!g_registry_ready) {
        yui_component_registry_init();
    }

    if (type_id >= 0 && type_id < LAYER_TYPE_BUILTIN_MAX && g_builtin_registered[type_id]) {
        return g_builtin_names[type_id];
    }

    for (int i = 0; i < g_dynamic_count; i++) {
        if (g_dynamic[i].type_id == type_id) {
            return g_dynamic[i].type_name;
        }
    }

    return "Unknown";
}

const YuiComponentOps* yui_type_get_ops(int type_id)
{
    if (!g_registry_ready) {
        yui_component_registry_init();
    }
    return lookup_ops(type_id);
}

int yui_type_count(void)
{
    if (!g_registry_ready) {
        yui_component_registry_init();
    }
    return LAYER_TYPE_BUILTIN_MAX + g_dynamic_count;
}

const YuiComponentTypeEntry* yui_type_entry_at(int index)
{
    static YuiComponentTypeEntry entry;
    if (!g_registry_ready) {
        yui_component_registry_init();
    }

    if (index < 0 || index >= yui_type_count()) {
        return NULL;
    }

    if (index < LAYER_TYPE_BUILTIN_MAX) {
        if (!g_builtin_registered[index]) {
            return NULL;
        }
        entry.type_id = index;
        entry.type_name = g_builtin_names[index];
        entry.ops = &g_builtin_ops[index];
        return &entry;
    }

    int dyn_index = index - LAYER_TYPE_BUILTIN_MAX;
    if (dyn_index < 0 || dyn_index >= g_dynamic_count) {
        return NULL;
    }

    entry.type_id = g_dynamic[dyn_index].type_id;
    entry.type_name = g_dynamic[dyn_index].type_name;
    entry.ops = &g_dynamic[dyn_index].ops;
    return &entry;
}

int yui_component_instantiate(Layer* layer, cJSON* json, Layer* parent,
                              int* out_custom_children)
{
    (void)parent;

    if (out_custom_children) {
        *out_custom_children = 0;
    }
    if (!layer) {
        return 0;
    }

    const YuiComponentOps* ops = yui_type_get_ops(layer->type);
    if (!ops) {
        return 0;
    }

    if (!(ops->backend_mask & yui_get_current_backend_mask())) {
        LOGW("registry", "type '%s' not supported on current backend",
             yui_type_name(layer->type));
        return -1;
    }

    if (!ops->create) {
        return 0;
    }

    layer->component = ops->create(layer, json);

    if (ops->handle_mouse) {
        layer->handle_mouse_event = ops->handle_mouse;
    }
    if (ops->handle_key) {
        layer->handle_key_event = ops->handle_key;
    }
    if (ops->on_layout) {
        layer->layout = ops->on_layout;
    }
    if (ops->after_create) {
        ops->after_create(layer, json);
    }

    if (ops->flags & YUI_COMP_FOCUSABLE) {
        layer->focusable = 1;
    }

    if (out_custom_children && (ops->flags & YUI_COMP_CUSTOM_CHILDREN)) {
        *out_custom_children = 1;
    }

    return layer->component ? 1 : 0;
}
