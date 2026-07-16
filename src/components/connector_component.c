#include "connector_component.h"
#include "../backend.h"
#include "../layer.h"
#include "../util.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

extern Layer* g_ui_root;

static void connector_merge_anchor_entry(ConnectorAnchorEntry* entries,
                                         int* entry_count, int max_entries,
                                         Layer* layer, ConnectorAnchor anchor)
{
    int i;

    if (!entries || !entry_count || !layer || anchor < CONNECTOR_ANCHOR_CENTER ||
        anchor > CONNECTOR_ANCHOR_RIGHT) {
        return;
    }

    for (i = 0; i < *entry_count; i++) {
        if (entries[i].layer == layer) {
            entries[i].anchor_mask |= (1u << anchor);
            return;
        }
    }

    if (*entry_count < max_entries) {
        entries[*entry_count].layer = layer;
        entries[*entry_count].anchor_mask = (1u << anchor);
        (*entry_count)++;
    }
}

Layer* connector_resolve_endpoint(const char* id_or_path)
{
    if (!id_or_path || !id_or_path[0] || !g_ui_root) {
        return NULL;
    }

    if (strchr(id_or_path, '.')) {
        return layer_resolve_path(g_ui_root, id_or_path);
    }

    return find_layer_by_id(g_ui_root, id_or_path);
}

int connector_layer_is_connectable(Layer* layer)
{
    if (!layer) {
        return 0;
    }

    if (layer->type == DRAGGABLE) {
        return 1;
    }

    return layer->connectable != 0;
}

Layer* connector_find_draggable_host(Layer* layer)
{
    Layer* current = layer;

    while (current) {
        if (current->type == DRAGGABLE) {
            return current;
        }
        current = current->parent;
    }

    return NULL;
}

static void connector_collect_endpoint_for_draggable(Layer* root, Layer* draggable,
                                                     ConnectorAnchorEntry* entries,
                                                     int* entry_count, int max_entries,
                                                     const char* endpoint_id,
                                                     ConnectorAnchor anchor)
{
    Layer* endpoint;

    if (!endpoint_id || !endpoint_id[0]) {
        return;
    }

    endpoint = connector_resolve_endpoint(endpoint_id);
    if (!endpoint || !connector_layer_is_connectable(endpoint)) {
        return;
    }

    if (connector_find_draggable_host(endpoint) != draggable) {
        return;
    }

    connector_merge_anchor_entry(entries, entry_count, max_entries, endpoint, anchor);
}

static void connector_walk_connectors_for_draggable(Layer* root, Layer* draggable,
                                                    ConnectorAnchorEntry* entries,
                                                    int* entry_count, int max_entries)
{
    ConnectorComponent* component;
    int i;

    if (!root) {
        return;
    }

    if (root->type == CONNECTOR && root->component) {
        component = (ConnectorComponent*)root->component;
        connector_collect_endpoint_for_draggable(root, draggable, entries, entry_count,
                                               max_entries, component->from_id,
                                               component->from_anchor);
        connector_collect_endpoint_for_draggable(root, draggable, entries, entry_count,
                                               max_entries, component->to_id,
                                               component->to_anchor);
    }

    if (root->children) {
        for (i = 0; i < root->child_count; i++) {
            if (root->children[i]) {
                connector_walk_connectors_for_draggable(root->children[i], draggable,
                                                        entries, entry_count, max_entries);
            }
        }
    }

    if (root->sub) {
        connector_walk_connectors_for_draggable(root->sub, draggable, entries,
                                                entry_count, max_entries);
    }
}

void connector_collect_anchors_for_draggable_tree(Layer* root, Layer* draggable,
                                                  ConnectorAnchorEntry* entries,
                                                  int* entry_count,
                                                  int max_entries)
{
    if (!draggable || !entries || !entry_count || max_entries <= 0) {
        return;
    }

    *entry_count = 0;
    if (!root) {
        return;
    }

    connector_walk_connectors_for_draggable(root, draggable, entries, entry_count,
                                            max_entries);
}

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

void connector_get_layer_anchor_point(Layer* layer, ConnectorAnchor anchor,
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

void connector_render_dot(int cx, int cy, int radius, Color color)
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

void connector_render_dots_for_layer(Layer* layer, unsigned int anchor_mask,
                                     int dot_size, Color dot_color)
{
    int anchor;
    int x;
    int y;

    if (!layer || !anchor_mask || dot_size <= 0) {
        return;
    }

    for (anchor = CONNECTOR_ANCHOR_CENTER; anchor <= CONNECTOR_ANCHOR_RIGHT; anchor++) {
        if (!(anchor_mask & (1u << anchor))) {
            continue;
        }
        connector_get_layer_anchor_point(layer, (ConnectorAnchor)anchor, &x, &y);
        connector_render_dot(x, y, dot_size, dot_color);
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

    if (!layer || !layer->component || !g_ui_root) {
        return;
    }

    component = (ConnectorComponent*)layer->component;
    if (!component->from_id[0] || !component->to_id[0]) {
        return;
    }

    from_layer = connector_resolve_endpoint(component->from_id);
    to_layer = connector_resolve_endpoint(component->to_id);
    if (!from_layer || !to_layer ||
        !connector_layer_is_connectable(from_layer) ||
        !connector_layer_is_connectable(to_layer)) {
        return;
    }

    connector_get_layer_anchor_point(from_layer, component->from_anchor, &x0, &y0);
    connector_get_layer_anchor_point(to_layer, component->to_anchor, &x1, &y1);

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
}

void connector_collect_anchors_for_layer(Layer* root, const char* layer_id,
                                         unsigned int* anchor_mask)
{
    ConnectorComponent* component;
    Layer* target_layer;
    Layer* from_layer;
    Layer* to_layer;
    int i;

    if (!root || !layer_id || !layer_id[0] || !anchor_mask) {
        return;
    }

    target_layer = connector_resolve_endpoint(layer_id);
    if (!target_layer) {
        return;
    }

    if (root->type == CONNECTOR && root->component) {
        component = (ConnectorComponent*)root->component;
        from_layer = connector_resolve_endpoint(component->from_id);
        to_layer = connector_resolve_endpoint(component->to_id);
        if (from_layer == target_layer) {
            *anchor_mask |= (1u << component->from_anchor);
        }
        if (to_layer == target_layer) {
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
