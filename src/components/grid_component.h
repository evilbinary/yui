#ifndef YUI_GRID_COMPONENT_H
#define YUI_GRID_COMPONENT_H

#include "../ytype.h"

typedef struct {
    Layer* layer;
} GridComponent;

GridComponent* grid_component_create(Layer* layer);
GridComponent* grid_component_create_from_json(Layer* layer, cJSON* json_obj);
void grid_component_destroy(GridComponent* component);
void grid_apply_config_from_json(Layer* layer, cJSON* json_obj);
int grid_layer_uses_grid_layout(const Layer* layer);

#endif
