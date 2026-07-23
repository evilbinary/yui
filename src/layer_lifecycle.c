#include "layer_lifecycle.h"

#include "cJSON.h"
#include "layout.h"
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

static void lifecycle_copy_handler_name(char* dest, size_t dest_size, const char* src) {
    if (!dest || dest_size == 0) return;
    dest[0] = '\0';
    if (!src || !src[0]) return;
    if (src[0] == '@') src++;
    strncpy(dest, src, dest_size - 1);
    dest[dest_size - 1] = '\0';
}

void layer_lifecycle_bind_events(Layer* layer, cJSON* events) {
    if (!layer || !events || !cJSON_IsObject(events)) return;

    cJSON* event = events->child;
    while (event) {
        if (!event->string || !cJSON_IsString(event)) {
            event = event->next;
            continue;
        }
        if (strcmp(event->string, "onLoad") == 0) {
            layer->lifecycle_flags |= LIFECYCLE_ON_LOAD;
            lifecycle_copy_handler_name(layer->lifecycle_on_load,
                                        sizeof(layer->lifecycle_on_load),
                                        event->valuestring);
        } else if (strcmp(event->string, "onShow") == 0) {
            layer->lifecycle_flags |= LIFECYCLE_ON_SHOW;
            lifecycle_copy_handler_name(layer->lifecycle_on_show,
                                        sizeof(layer->lifecycle_on_show),
                                        event->valuestring);
        } else if (strcmp(event->string, "onHide") == 0) {
            layer->lifecycle_flags |= LIFECYCLE_ON_HIDE;
            lifecycle_copy_handler_name(layer->lifecycle_on_hide,
                                        sizeof(layer->lifecycle_on_hide),
                                        event->valuestring);
        } else if (strcmp(event->string, "onUnload") == 0) {
            layer->lifecycle_flags |= LIFECYCLE_ON_UNLOAD;
            lifecycle_copy_handler_name(layer->lifecycle_on_unload,
                                        sizeof(layer->lifecycle_on_unload),
                                        event->valuestring);
        }
        event = event->next;
    }
}

void layer_lifecycle_set_dispatch(LayerLifecycleDispatchFn fn) {
    g_lifecycle_dispatch = fn;
}

static void layer_lifecycle_dispatch(Layer* layer, const char* event_type) {
    if (!layer || !event_type || !event_type[0]) return;
    if (!(layer->lifecycle_flags & LIFECYCLE_DECL_MASK)) return;
    if (!g_lifecycle_dispatch) {
        printf("LayerLifecycle: %s.%s (no JS dispatch)\n", layer->id, event_type);
        return;
    }
    g_lifecycle_dispatch(layer, event_type);
}

void layer_lifecycle_on_show(Layer* layer) {
    if (!layer || !(layer->lifecycle_flags & LIFECYCLE_DECL_MASK)) return;

    if (!(layer->lifecycle_flags & LIFECYCLE_LOADED) &&
        (layer->lifecycle_flags & LIFECYCLE_ON_LOAD)) {
        layer_lifecycle_dispatch(layer, "onLoad");
        layer->lifecycle_flags |= LIFECYCLE_LOADED;
    }
    if (!(layer->lifecycle_flags & LIFECYCLE_SHOWN) &&
        (layer->lifecycle_flags & LIFECYCLE_ON_SHOW)) {
        layer_lifecycle_dispatch(layer, "onShow");
    }
    layer->lifecycle_flags |= LIFECYCLE_SHOWN;
}

void layer_lifecycle_on_hide(Layer* layer) {
    if (!layer || !(layer->lifecycle_flags & LIFECYCLE_DECL_MASK)) return;
    if (layer->lifecycle_flags & LIFECYCLE_SHOWN) {
        if (layer->lifecycle_flags & LIFECYCLE_ON_HIDE) {
            layer_lifecycle_dispatch(layer, "onHide");
        }
        layer->lifecycle_flags &= ~LIFECYCLE_SHOWN;
    }
}

static void layer_lifecycle_on_hide_visible(Layer* layer) {
    if (!layer || !(layer->lifecycle_flags & LIFECYCLE_DECL_MASK)) return;
    if (layer->lifecycle_flags & LIFECYCLE_ON_HIDE) {
        layer_lifecycle_dispatch(layer, "onHide");
    }
    layer->lifecycle_flags &= ~LIFECYCLE_SHOWN;
}

void layer_lifecycle_before_destroy(Layer* layer) {
    if (!layer || !(layer->lifecycle_flags & LIFECYCLE_DECL_MASK)) return;
    if (layer->visible == VISIBLE) {
        layer_lifecycle_on_hide_visible(layer);
    } else if (layer->lifecycle_flags & LIFECYCLE_SHOWN) {
        layer_lifecycle_on_hide(layer);
    }
    if ((layer->lifecycle_flags & LIFECYCLE_LOADED) &&
        (layer->lifecycle_flags & LIFECYCLE_ON_UNLOAD)) {
        layer_lifecycle_dispatch(layer, "onUnload");
        layer->lifecycle_flags &= ~LIFECYCLE_LOADED;
    }
}

static void layer_lifecycle_walk(Layer* layer,
                                 void (*visit)(Layer* layer)) {
    if (!layer || !visit) return;

    if (layer->children) {
        for (int i = 0; i < layer->child_count; i++) {
            layer_lifecycle_walk(layer->children[i], visit);
        }
    }
    if (layer->sub && layer->sub->parent == layer) {
        layer_lifecycle_walk(layer->sub, visit);
    }
    visit(layer);
}

static void layer_lifecycle_startup_visit(Layer* layer) {
    if ((layer->lifecycle_flags & LIFECYCLE_DECL_MASK) &&
        layer->visible == VISIBLE) {
        layer_lifecycle_on_show(layer);
    }
}

void layer_lifecycle_init_tree(Layer* root) {
  // 仅遍历子树；等 JS 注册完成后再统一派发可见页面的 onLoad/onShow
    layer_lifecycle_walk(root, layer_lifecycle_startup_visit);
}

void layer_set_visible(Layer* layer, int visible) {
    if (!layer) return;

    VisibleType new_visible = (VisibleType)visible;

    if (layer->visible == new_visible) {
        if (new_visible != VISIBLE && (layer->lifecycle_flags & LIFECYCLE_SHOWN)) {
            layer->lifecycle_flags &= ~LIFECYCLE_SHOWN;
        }
        return;
    }

    int was_visible = (layer->visible == VISIBLE);

    if (was_visible) {
        layer_lifecycle_on_hide_visible(layer);
    }

    if (layer->set_visible) {
        layer->set_visible(layer, new_visible);
    } else {
        layer->visible = new_visible;
        mark_layer_dirty(layer, DIRTY_VISIBLE);
    }

    if (!was_visible && new_visible == VISIBLE) {
        layer_lifecycle_on_show(layer);
    }

    if (layer->parent && !yui_update_is_batching()) {
        layout_layer(layer->parent);
    }
}
