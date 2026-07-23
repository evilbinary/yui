#ifndef YUI_LAYOUT_H
#define YUI_LAYOUT_H

#include <stdio.h>
#include "layer.h"
#include "cJSON.h"

typedef struct Layer Layer;
typedef struct ResizeEvent ResizeEvent;

/* Optional fields for layer_to_json / layer_dump_json */
#define LAYER_JSON_STYLE   (1 << 0)
#define LAYER_JSON_EVENTS  (1 << 1)

void layout_layer(Layer* layer);
int layout_after_append_child(Layer* layer);
void layout_capture_base(Layer* layer);
void layout_resize(Layer* layer, int width, int height);
void layout_dispatch_resize_events(Layer* layer, const ResizeEvent* event);
int layout_scroll_vertical(Layer* layer, int delta_y);
void layer_dump(const Layer* layer, int depth);

/* Build a cJSON tree for the layer subtree. Caller must cJSON_Delete. */
cJSON* layer_to_json(const Layer* layer, int flags);
/* Pretty-print layer JSON to out (defaults to stdout when out is NULL). */
void layer_dump_json(const Layer* layer, FILE* out, int flags);

#endif