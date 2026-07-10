#ifndef YUI_LAYER_LIFECYCLE_H
#define YUI_LAYER_LIFECYCLE_H

typedef struct Layer Layer;
struct cJSON;

typedef void (*LayerLifecycleDispatchFn)(Layer* layer, const char* event_type);

int layer_lifecycle_is_event(const char* event_name);
void layer_lifecycle_bind_events(Layer* layer, struct cJSON* events);

void layer_lifecycle_set_dispatch(LayerLifecycleDispatchFn fn);

void layer_lifecycle_on_show(Layer* layer);
void layer_lifecycle_on_hide(Layer* layer);
void layer_lifecycle_before_destroy(Layer* layer);
void layer_lifecycle_init_tree(Layer* root);

void layer_set_visible(Layer* layer, int visible);

#endif
