#ifndef YUI_LAYOUT_H
#define YUI_LAYOUT_H

#include "layer.h"

typedef struct Layer Layer;


void layout_layer(Layer* layer);
void layout_resize(Layer* layer, int width, int height);

#endif