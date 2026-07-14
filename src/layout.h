#ifndef YUI_LAYOUT_H
#define YUI_LAYOUT_H

#include "layer.h"

typedef struct Layer Layer;
typedef struct ResizeEvent ResizeEvent;

void layout_layer(Layer* layer);
void layout_capture_base(Layer* layer);
void layout_resize(Layer* layer, int width, int height);
void layout_dispatch_resize_events(Layer* layer, const ResizeEvent* event);
void layer_dump(const Layer* layer, int depth);

#endif