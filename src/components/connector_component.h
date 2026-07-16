#ifndef YUI_CONNECTOR_COMPONENT_H
#define YUI_CONNECTOR_COMPONENT_H

#include "../layer.h"
#include "../ytype.h"

#ifdef __cplusplus
extern "C" {
#endif

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
    int show_dot;
    int dot_size;
} ConnectorComponent;

ConnectorComponent* connector_component_create(Layer* layer);
ConnectorComponent* connector_component_create_from_json(Layer* layer, cJSON* json_obj);
void connector_component_destroy(ConnectorComponent* component);
void connector_component_render(Layer* layer);

#ifdef __cplusplus
}
#endif

#endif
