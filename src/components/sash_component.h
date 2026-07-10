#ifndef YUI_SASH_COMPONENT_H
#define YUI_SASH_COMPONENT_H

#include "../ytype.h"

typedef struct Layer Layer;
typedef struct MouseEvent MouseEvent;

typedef struct {
    Layer* layer;
    int dragging;
    int drag_start_y;
    int initial_height;
    Layer* target;       // layer above sash to resize (lazy-resolved from target_id)
    char target_id[MAX_TEXT];
    int min_size;
    int hover;
} SashComponent;

SashComponent* sash_component_create(Layer* layer);
SashComponent* sash_component_create_from_json(Layer* layer, cJSON* json_obj);
void sash_component_destroy(SashComponent* component);
void sash_component_render(Layer* layer);
int sash_component_handle_mouse_event(Layer* layer, MouseEvent* event);

#endif
