#ifndef YUI_LAYER_LIFECYCLE_H
#define YUI_LAYER_LIFECYCLE_H

typedef struct Layer Layer;
struct cJSON;

// JSON 声明（低 4 位）+ 运行时状态（高 2 位）
#define LIFECYCLE_ON_LOAD    (1u << 0)
#define LIFECYCLE_ON_SHOW    (1u << 1)
#define LIFECYCLE_ON_HIDE    (1u << 2)
#define LIFECYCLE_ON_UNLOAD  (1u << 3)
#define LIFECYCLE_LOADED     (1u << 4)
#define LIFECYCLE_SHOWN      (1u << 5)
#define LIFECYCLE_DECL_MASK  (LIFECYCLE_ON_LOAD | LIFECYCLE_ON_SHOW | \
                              LIFECYCLE_ON_HIDE | LIFECYCLE_ON_UNLOAD)

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
