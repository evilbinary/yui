#include "layer_lifecycle.h"

#include "cJSON.h"
#include "layer.h"
#include "layer_update.h"
#include "ytype.h"

#include <stdio.h>
#include <string.h>

static LayerLifecycleDispatchFn g_lifecycle_dispatch = NULL;

int layer_lifecycle_is_event(const char* event_name) {
    if (!event_name) return 0;
    return strcmp(event_name, "onLoad") == 0 ||
           strcmp(event_name, "onShow") == 0 ||
           strcmp(event_name, "onHide") == 0 ||
           strcmp(event_name, "onUnload") == 0;
}

void layer_lifecycle_bind_events(Layer* layer, cJSON* events) {
    if (!layer || !events || !cJSON_IsObject(events)) return;

    cJSON* event = events->child;
    while (event) {
        if (layer_lifecycle_is_event(event->string)) {
            layer->lifecycle_enabled = 1;
            return;
        }
        event = event->next;
    }
}

void layer_lifecycle_set_dispatch(LayerLifecycleDispatchFn fn) {
    g_lifecycle_dispatch = fn;
}

static void layer_lifecycle_dispatch(Layer* layer, const char* event_type) {
    if (!layer || !event_type || !event_type[0] || !layer->lifecycle_enabled) return;
    if (!g_lifecycle_dispatch) {
        printf("LayerLifecycle: %s.%s (no JS dispatch)\n", layer->id, event_type);
        return;
    }
    g_lifecycle_dispatch(layer, event_type);
}

void layer_lifecycle_on_show(Layer* layer) {
    if (!layer || !layer->lifecycle_enabled) return;

    if (!layer->lifecycle_loaded) {
        layer_lifecycle_dispatch(layer, "onLoad");
        layer->lifecycle_loaded = 1;
    }
    if (!layer->lifecycle_shown) {
        layer_lifecycle_dispatch(layer, "onShow");
        layer->lifecycle_shown = 1;
    }
}

void layer_lifecycle_on_hide(Layer* layer) {
    if (!layer || !layer->lifecycle_enabled || !layer->lifecycle_shown) return;
    layer_lifecycle_dispatch(layer, "onHide");
    layer->lifecycle_shown = 0;
}

void layer_lifecycle_before_destroy(Layer* layer) {
    if (!layer || !layer->lifecycle_enabled) return;
    if (layer->lifecycle_shown) {
        layer_lifecycle_on_hide(layer);
    }
    if (layer->lifecycle_loaded) {
        layer_lifecycle_dispatch(layer, "onUnload");
        layer->lifecycle_loaded = 0;
    }
}

static void layer_lifecycle_init_layer(Layer* layer) {
    if (!layer) return;

    if (layer->lifecycle_enabled && layer->visible == VISIBLE) {
        layer_lifecycle_on_show(layer);
    }

    if (layer->children) {
        for (int i = 0; i < layer->child_count; i++) {
            layer_lifecycle_init_layer(layer->children[i]);
        }
    }
    if (layer->sub) {
        layer_lifecycle_init_layer(layer->sub);
    }
}

void layer_lifecycle_init_tree(Layer* root) {
    layer_lifecycle_init_layer(root);
}

void layer_set_visible(Layer* layer, int visible) {
    if (!layer) return;

    int was_visible = (layer->visible == VISIBLE);
    int will_visible = visible ? VISIBLE : IN_VISIBLE;
    if (was_visible == will_visible) return;

    if (was_visible) {
        layer_lifecycle_on_hide(layer);
    }

    if (layer->set_visible) {
        layer->set_visible(layer, will_visible);
    } else {
        layer->visible = will_visible;
        mark_layer_dirty(layer, DIRTY_VISIBLE);
    }

    if (!was_visible && will_visible) {
        layer_lifecycle_on_show(layer);
    }
}
