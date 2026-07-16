#ifndef YUI_CONNECTOR_COMPONENT_H
#define YUI_CONNECTOR_COMPONENT_H

#include "../layer.h"
#include "../ytype.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CONNECTOR_MAX_ANCHOR_ENTRIES 32

typedef enum {
    CONNECTOR_ANCHOR_CENTER = 0,
    CONNECTOR_ANCHOR_TOP,
    CONNECTOR_ANCHOR_BOTTOM,
    CONNECTOR_ANCHOR_LEFT,
    CONNECTOR_ANCHOR_RIGHT,
} ConnectorAnchor;

typedef enum {
    CONNECTOR_CURVE_AUTO = 0,
    CONNECTOR_CURVE_AUTO_VERTICAL,
    CONNECTOR_CURVE_MANUAL,
} ConnectorCurveMode;

typedef struct ConnectorComponent {
    Layer* layer;
    char from_id[MAX_TEXT];
    char to_id[MAX_TEXT];
    ConnectorAnchor from_anchor;
    ConnectorAnchor to_anchor;
    ConnectorCurveMode curve_mode;
    int ctrl1_x;
    int ctrl1_y;
    int ctrl2_x;
    int ctrl2_y;
    int has_ctrl1;
    int has_ctrl2;
    Color stroke_color;
    int stroke_width;
} ConnectorComponent;

typedef struct ConnectorAnchorEntry {
    Layer* layer;
    unsigned int anchor_mask;
} ConnectorAnchorEntry;

ConnectorComponent* connector_component_create(Layer* layer);
ConnectorComponent* connector_component_create_from_json(Layer* layer, cJSON* json_obj);
void connector_component_destroy(ConnectorComponent* component);
void connector_component_render(Layer* layer);
Layer* connector_resolve_endpoint(const char* id_or_path);
Layer* connector_find_canvas(Layer* layer);
int connector_layer_is_connectable(Layer* layer);
void connector_get_layer_anchor_point(Layer* layer, ConnectorAnchor anchor,
                                      int* out_x, int* out_y);
void connector_render_dot(int cx, int cy, int radius, Color color);
void connector_render_dots_for_layer(Layer* layer, unsigned int anchor_mask,
                                     int dot_size, Color dot_color);
void connector_collect_port_anchors_for_draggable_tree(Layer* root, Layer* draggable,
                                                       ConnectorAnchorEntry* entries,
                                                       int* entry_count,
                                                       int max_entries);
int connector_hit_test_draggable_ports(Layer* draggable, int x, int y, int dot_size,
                                       Layer** out_layer, ConnectorAnchor* out_anchor);
int connector_interaction_start(Layer* from_layer, ConnectorAnchor from_anchor,
                                int dot_size, int mouse_x, int mouse_y);
int connector_try_remove_at(Layer* canvas, int x, int y, int dot_size);

#ifdef __cplusplus
}
#endif

#endif
