#include "grid_component.h"
#include <stdlib.h>
#include <string.h>

static void grid_ensure_layout_manager(Layer* layer)
{
    if (!layer) {
        return;
    }
    if (!layer->layout_manager) {
        layer->layout_manager = (LayoutManager*)calloc(1, sizeof(LayoutManager));
    }
    if (layer->layout_manager) {
        layer->layout_manager->type = LAYOUT_GRID;
    }
}

static int grid_read_int(cJSON* obj, const char* key, int* out)
{
    if (!obj || !key || !out) {
        return 0;
    }
    cJSON* item = cJSON_GetObjectItem(obj, key);
    if (!item || !cJSON_IsNumber(item)) {
        return 0;
    }
    *out = item->valueint;
    return 1;
}

void grid_apply_config_from_json(Layer* layer, cJSON* json_obj)
{
    if (!layer || !json_obj) {
        return;
    }

    grid_ensure_layout_manager(layer);
    if (!layer->layout_manager) {
        return;
    }

    layer->layout_manager->type = LAYOUT_GRID;

    int columns = 0;
    cJSON* layout = cJSON_GetObjectItem(json_obj, "layout");
    if (grid_read_int(json_obj, "columns", &columns) ||
        (layout && grid_read_int(layout, "columns", &columns))) {
        layer->layout_manager->columns = columns > 0 ? columns : 1;
    } else if (layer->layout_manager->columns <= 0) {
        layer->layout_manager->columns = 1;
    }

    int gap = 0;
    if (grid_read_int(json_obj, "gap", &gap) ||
        (layout && grid_read_int(layout, "gap", &gap))) {
        layer->layout_manager->spacing = gap;
    }
}

int grid_layer_uses_grid_layout(const Layer* layer)
{
    if (!layer) {
        return 0;
    }
    if (layer->type == GRID) {
        return 1;
    }
    return layer->layout_manager && layer->layout_manager->type == LAYOUT_GRID;
}

static void grid_layer_destroy(Layer* layer)
{
    if (!layer || !layer->component) {
        return;
    }
    grid_component_destroy((GridComponent*)layer->component);
    layer->component = NULL;
}

GridComponent* grid_component_create(Layer* layer)
{
    if (!layer) {
        return NULL;
    }

    GridComponent* component = (GridComponent*)calloc(1, sizeof(GridComponent));
    if (!component) {
        return NULL;
    }

    component->layer = layer;
    layer->component = component;
    layer->on_destroy = grid_layer_destroy;

    grid_ensure_layout_manager(layer);
    if (layer->layout_manager && layer->layout_manager->columns <= 0) {
        layer->layout_manager->columns = 1;
    }

    return component;
}

GridComponent* grid_component_create_from_json(Layer* layer, cJSON* json_obj)
{
    GridComponent* component = grid_component_create(layer);
    if (!component) {
        return NULL;
    }
    grid_apply_config_from_json(layer, json_obj);
    return component;
}

void grid_component_destroy(GridComponent* component)
{
    if (component) {
        free(component);
    }
}
