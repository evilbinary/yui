#ifndef YUI_LOADING_COMPONENT_H
#define YUI_LOADING_COMPONENT_H

#include "../ytype.h"

typedef enum {
    LOADING_VARIANT_SPINNER,
    LOADING_VARIANT_DOTS,
} LoadingVariant;

typedef struct {
    Layer* layer;
    LoadingVariant variant;
    Color color;
    Color track_color;
    int stroke_width;
    float speed;
} LoadingComponent;

LoadingComponent* loading_component_create(Layer* layer);
LoadingComponent* loading_component_create_from_json(Layer* layer, cJSON* json_obj);
void loading_component_destroy(LoadingComponent* component);
void loading_component_render(Layer* layer);

#endif
