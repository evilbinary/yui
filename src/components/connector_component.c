#include "connector_component.h"
#include "draggable_component.h"
#include "../backend.h"
#include "../layer.h"
#include "../util.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

extern Layer* g_ui_root;

static void connector_layer_destroy(Layer* layer)
{
    if (!layer || !layer->component) {
        return;
    }
    connector_component_destroy((ConnectorComponent*)layer->component);
    layer->component = NULL;
}

static ConnectorAnchor connector_parse_anchor(const char* value)
{
    if (!value) {
        return CONNECTOR_ANCHOR_CENTER;
    }
    if (strcmp(value, "top") == 0) {
        return CONNECTOR_ANCHOR_TOP;
    }
    if (strcmp(value, "bottom") == 0) {
        return CONNECTOR_ANCHOR_BOTTOM;
    }
    if (strcmp(value, "left") == 0) {
        return CONNECTOR_ANCHOR_LEFT;
    }
    if (strcmp(value, "right") == 0) {
        return CONNECTOR_ANCHOR_RIGHT;
    }
    return CONNECTOR_ANCHOR_CENTER;
}

static void connector_parse_endpoint(cJSON* endpoint, char* id_out,
                                     ConnectorAnchor* anchor_out)
{
    if (!endpoint || !id_out || !anchor_out) {
        return;
    }

    id_out[0] = '\0';
    *anchor_out = CONNECTOR_ANCHOR_CENTER;

    if (cJSON_IsString(endpoint)) {
        strncpy(id_out, endpoint->valuestring, MAX_TEXT - 1);
        id_out[MAX_TEXT - 1] = '\0';
        return;
    }

    if (!cJSON_IsObject(endpoint)) {
        return;
    }

    cJSON* id_item = cJSON_GetObjectItem(endpoint, "id");
    if (id_item && cJSON_IsString(id_item)) {
        strncpy(id_out, id_item->valuestring, MAX_TEXT - 1);
        id_out[MAX_TEXT - 1] = '\0';
    }

    cJSON* anchor_item = cJSON_GetObjectItem(endpoint, "anchor");
    if (anchor_item && cJSON_IsString(anchor_item)) {
        *anchor_out = connector_parse_anchor(anchor_item->valuestring);
    }
}

static void connector_get_anchor_point(Layer* layer, ConnectorAnchor anchor,
                                       int* out_x, int* out_y)
{
    Rect rect = layer->rect;

    if (!out_x || !out_y) {
        return;
    }

    switch (anchor) {
        case CONNECTOR_ANCHOR_TOP:
            *out_x = rect.x + rect.w / 2;
            *out_y = rect.y;
            break;
        case CONNECTOR_ANCHOR_BOTTOM:
            *out_x = rect.x + rect.w / 2;
            *out_y = rect.y + rect.h;
            break;
        case CONNECTOR_ANCHOR_LEFT:
            *out_x = rect.x;
            *out_y = rect.y + rect.h / 2;
            break;
        case CONNECTOR_ANCHOR_RIGHT:
            *out_x = rect.x + rect.w;
            *out_y = rect.y + rect.h / 2;
            break;
        case CONNECTOR_ANCHOR_CENTER:
        default:
            *out_x = rect.x + rect.w / 2;
            *out_y = rect.y + rect.h / 2;
            break;
    }
}

static void connector_auto_control_points(int x0, int y0, int x1, int y1,
                                          ConnectorCurveMode mode,
                                          int* cx1, int* cy1, int* cx2, int* cy2)
{
    int dx = x1 - x0;
    int dy = y1 - y0;
    int offset;

    if (mode == CONNECTOR_CURVE_AUTO_VERTICAL) {
        offset = abs(dy) / 2;
        if (offset < 40) {
            offset = 40;
        }
        *cx1 = x0;
        *cy1 = y0 + (dy >= 0 ? offset : -offset);
        *cx2 = x1;
        *cy2 = y1 - (dy >= 0 ? offset : -offset);
        return;
    }

    offset = abs(dx) / 2;
    if (offset < 40) {
        offset = 40;
    }
    *cx1 = x0 + (dx >= 0 ? offset : -offset);
    *cy1 = y0;
    *cx2 = x1 - (dx >= 0 ? offset : -offset);
    *cy2 = y1;
}

static void connector_render_dot(int cx, int cy, int radius, Color color)
{
    Rect rect;

    if (radius < 1) {
        radius = 1;
    }

    rect.x = cx - radius;
    rect.y = cy - radius;
    rect.w = radius * 2;
    rect.h = radius * 2;
    backend_render_rounded_rect(&rect, color, radius);
}

static void connector_resolve_endpoint_dot(ConnectorComponent* component, Layer* endpoint,
                                           int* show_dots, int* dot_size, Color* dot_color)
{
    int drag_show = 1;
    int drag_size = 4;
    Color drag_color = {137, 180, 250, 255};

    if (draggable_component_get_dot_style(endpoint, &drag_show, &drag_size, &drag_color)) {
        /* use draggable defaults as fallback */
    }

    if (show_dots) {
        *show_dots = component->has_show_dots ? component->show_dots : drag_show;
    }
    if (dot_size) {
        *dot_size = component->has_dot_size ? component->dot_size : drag_size;
    }
    if (dot_color) {
        *dot_color = component->has_dot_color ? component->dot_color : drag_color;
    }
}

ConnectorComponent* connector_component_create(Layer* layer)
{
    ConnectorComponent* component;

    if (!layer) {
        return NULL;
    }

    component = (ConnectorComponent*)calloc(1, sizeof(ConnectorComponent));
    if (!component) {
        return NULL;
    }

    component->layer = layer;
    component->from_anchor = CONNECTOR_ANCHOR_RIGHT;
    component->to_anchor = CONNECTOR_ANCHOR_LEFT;
    component->curve_mode = CONNECTOR_CURVE_AUTO;
    component->stroke_color = (Color){136, 136, 136, 255};
    component->stroke_width = 2;

    layer->component = component;
    layer->render = connector_component_render;
    layer->on_destroy = connector_layer_destroy;

    return component;
}

ConnectorComponent* connector_component_create_from_json(Layer* layer, cJSON* json_obj)
{
    ConnectorComponent* component = connector_component_create(layer);
    cJSON* style;
    cJSON* curve_item;
    cJSON* control_points;

    if (!component) {
        return NULL;
    }

    connector_parse_endpoint(cJSON_GetObjectItem(json_obj, "from"),
                             component->from_id, &component->from_anchor);
    connector_parse_endpoint(cJSON_GetObjectItem(json_obj, "to"),
                             component->to_id, &component->to_anchor);

    curve_item = cJSON_GetObjectItem(json_obj, "curve");
    if (curve_item && cJSON_IsString(curve_item)) {
        if (strcmp(curve_item->valuestring, "auto-vertical") == 0) {
            component->curve_mode = CONNECTOR_CURVE_AUTO_VERTICAL;
        } else if (strcmp(curve_item->valuestring, "manual") == 0) {
            component->curve_mode = CONNECTOR_CURVE_MANUAL;
        } else {
            component->curve_mode = CONNECTOR_CURVE_AUTO;
        }
    }

    control_points = cJSON_GetObjectItem(json_obj, "controlPoints");
    if (control_points && cJSON_IsArray(control_points) && cJSON_GetArraySize(control_points) >= 2) {
        cJSON* p1 = cJSON_GetArrayItem(control_points, 0);
        cJSON* p2 = cJSON_GetArrayItem(control_points, 1);
        if (p1 && cJSON_IsArray(p1) && cJSON_GetArraySize(p1) >= 2) {
            component->ctrl1_x = cJSON_GetArrayItem(p1, 0)->valueint;
            component->ctrl1_y = cJSON_GetArrayItem(p1, 1)->valueint;
            component->has_ctrl1 = 1;
        }
        if (p2 && cJSON_IsArray(p2) && cJSON_GetArraySize(p2) >= 2) {
            component->ctrl2_x = cJSON_GetArrayItem(p2, 0)->valueint;
            component->ctrl2_y = cJSON_GetArrayItem(p2, 1)->valueint;
            component->has_ctrl2 = 1;
        }
        component->curve_mode = CONNECTOR_CURVE_MANUAL;
    }

    style = cJSON_GetObjectItem(json_obj, "style");
    if (style) {
        cJSON* color_item = cJSON_GetObjectItem(style, "color");
        if (!color_item) {
            color_item = cJSON_GetObjectItem(style, "strokeColor");
        }
        if (color_item && cJSON_IsString(color_item)) {
            parse_color(color_item->valuestring, &component->stroke_color);
        }

        if (cJSON_HasObjectItem(style, "strokeWidth")) {
            component->stroke_width = cJSON_GetObjectItem(style, "strokeWidth")->valueint;
        }

        {
            cJSON* dots_item = cJSON_GetObjectItem(style, "dots");
            if (dots_item) {
                component->has_show_dots = 1;
                if (cJSON_IsBool(dots_item)) {
                    component->show_dots = cJSON_IsTrue(dots_item);
                } else if (cJSON_IsNumber(dots_item)) {
                    component->show_dots = dots_item->valueint != 0;
                }
            }

            if (cJSON_HasObjectItem(style, "dotSize")) {
                component->has_dot_size = 1;
                component->dot_size = cJSON_GetObjectItem(style, "dotSize")->valueint;
            }

            if (cJSON_HasObjectItem(style, "dotColor")) {
                cJSON* dot_color_item = cJSON_GetObjectItem(style, "dotColor");
                if (dot_color_item && cJSON_IsString(dot_color_item)) {
                    component->has_dot_color = 1;
                    parse_color(dot_color_item->valuestring, &component->dot_color);
                }
            }
        }
    }

    return component;
}

void connector_component_destroy(ConnectorComponent* component)
{
    if (component) {
        free(component);
    }
}

void connector_component_render(Layer* layer)
{
    ConnectorComponent* component;
    Layer* from_layer;
    Layer* to_layer;
    int x0;
    int y0;
    int x1;
    int y1;
    int cx1;
    int cy1;
    int cx2;
    int cy2;
    int from_show_dots;
    int to_show_dots;
    int from_dot_size;
    int to_dot_size;
    Color from_dot_color;
    Color to_dot_color;

    if (!layer || !layer->component || !g_ui_root) {
        return;
    }

    component = (ConnectorComponent*)layer->component;
    if (!component->from_id[0] || !component->to_id[0]) {
        return;
    }

    from_layer = find_layer_by_id(g_ui_root, component->from_id);
    to_layer = find_layer_by_id(g_ui_root, component->to_id);
    if (!from_layer || !to_layer) {
        return;
    }

    connector_get_anchor_point(from_layer, component->from_anchor, &x0, &y0);
    connector_get_anchor_point(to_layer, component->to_anchor, &x1, &y1);

    if (component->curve_mode == CONNECTOR_CURVE_MANUAL &&
        component->has_ctrl1 && component->has_ctrl2) {
        cx1 = component->ctrl1_x;
        cy1 = component->ctrl1_y;
        cx2 = component->ctrl2_x;
        cy2 = component->ctrl2_y;
    } else {
        connector_auto_control_points(x0, y0, x1, y1, component->curve_mode,
                                      &cx1, &cy1, &cx2, &cy2);
    }

    backend_render_bezier_cubic(x0, y0, cx1, cy1, cx2, cy2, x1, y1,
                                component->stroke_color,
                                component->stroke_width);

    connector_resolve_endpoint_dot(component, from_layer,
                                   &from_show_dots, &from_dot_size, &from_dot_color);
    connector_resolve_endpoint_dot(component, to_layer,
                                   &to_show_dots, &to_dot_size, &to_dot_color);

    if (from_show_dots && from_dot_size > 0) {
        connector_render_dot(x0, y0, from_dot_size, from_dot_color);
    }
    if (to_show_dots && to_dot_size > 0) {
        connector_render_dot(x1, y1, to_dot_size, to_dot_color);
    }
}

void connector_collect_anchors_for_layer(Layer* root, const char* layer_id,
                                         unsigned int* anchor_mask)
{
    ConnectorComponent* component;
    int i;

    if (!root || !layer_id || !layer_id[0] || !anchor_mask) {
        return;
    }

    if (root->type == CONNECTOR && root->component) {
        component = (ConnectorComponent*)root->component;
        if (strcmp(component->from_id, layer_id) == 0) {
            *anchor_mask |= (1u << component->from_anchor);
        }
        if (strcmp(component->to_id, layer_id) == 0) {
            *anchor_mask |= (1u << component->to_anchor);
        }
    }

    if (root->children) {
        for (i = 0; i < root->child_count; i++) {
            if (root->children[i]) {
                connector_collect_anchors_for_layer(root->children[i], layer_id,
                                                    anchor_mask);
            }
        }
    }

    if (root->sub) {
        connector_collect_anchors_for_layer(root->sub, layer_id, anchor_mask);
    }
}
