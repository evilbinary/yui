#include "connector_component.h"
#include "draggable_component.h"
#include "../backend.h"
#include "../event.h"
#include "../layout.h"
#include "../layer.h"
#include "../util.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern Layer* g_ui_root;

typedef enum {
    CONNECTOR_DRAG_CREATE = 0,
    CONNECTOR_DRAG_MODIFY = 1,
} ConnectorDragMode;

typedef struct ConnectorDragState {
    int active;
    ConnectorDragMode mode;
    Layer* canvas;
    Layer* capture_layer;
    Layer* modify_layer;
    int modify_from_end;
    Layer* from_layer;
    ConnectorAnchor from_anchor;
    int dot_size;
    int mouse_x;
    int mouse_y;
    Layer* hover_layer;
    ConnectorAnchor hover_anchor;
} ConnectorDragState;

static ConnectorDragState g_connector_drag;

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

static const char* connector_anchor_name(ConnectorAnchor anchor)
{
    switch (anchor) {
        case CONNECTOR_ANCHOR_TOP:
            return "top";
        case CONNECTOR_ANCHOR_BOTTOM:
            return "bottom";
        case CONNECTOR_ANCHOR_LEFT:
            return "left";
        case CONNECTOR_ANCHOR_RIGHT:
            return "right";
        default:
            return "center";
    }
}

static void connector_dispatch_connect_change(Layer* layer, const char* detail,
                                              const char* handler_name)
{
    EventHandler handler;

    if (!layer || !detail || !handler_name || !handler_name[0]) {
        return;
    }

    handler = find_event_by_name(handler_name);
    if (!handler) {
        return;
    }

    if (!layer->event) {
        layer->event = (Event*)calloc(1, sizeof(Event));
    }
    if (layer->event) {
        strncpy(layer->event->click_name, handler_name,
                sizeof(layer->event->click_name) - 1);
        layer->event->click_name[sizeof(layer->event->click_name) - 1] = '\0';
    }

    layer_set_text(layer, detail);
    EVENT_INVOKE(handler, layer);
}

static void connector_emit_node_connect_change(Layer* draggable_host,
                                               ConnectorComponent* component,
                                               const char* action,
                                               Layer* port_layer,
                                               ConnectorAnchor port_anchor)
{
    char detail[640];
    const char* port_id;

    if (!draggable_host || draggable_host->type != DRAGGABLE || !component || !action) {
        return;
    }

    port_id = port_layer && port_layer->id[0] ? port_layer->id : "";
    snprintf(detail, sizeof(detail),
             "{\"action\":\"%s\",\"from\":{\"id\":\"%s\",\"anchor\":\"%s\"},"
             "\"to\":{\"id\":\"%s\",\"anchor\":\"%s\"},"
             "\"port\":{\"id\":\"%s\",\"anchor\":\"%s\"}}",
             action,
             component->from_id,
             connector_anchor_name(component->from_anchor),
             component->to_id,
             connector_anchor_name(component->to_anchor),
             port_id,
             connector_anchor_name(port_anchor));
    draggable_component_emit_connect_change(draggable_host, detail);
}

static void connector_emit_connect_change(Layer* layer, ConnectorComponent* component,
                                          const char* action)
{
    char detail[512];
    Layer* from_layer;
    Layer* to_layer;
    Layer* from_host;
    Layer* to_host;

    if (!layer || !component || !action) {
        return;
    }

    snprintf(detail, sizeof(detail),
             "{\"action\":\"%s\",\"from\":{\"id\":\"%s\",\"anchor\":\"%s\"},"
             "\"to\":{\"id\":\"%s\",\"anchor\":\"%s\"}}",
             action,
             component->from_id,
             connector_anchor_name(component->from_anchor),
             component->to_id,
             connector_anchor_name(component->to_anchor));

    connector_dispatch_connect_change(layer, detail, component->on_connect_change_name);

    from_layer = connector_resolve_endpoint(component->from_id);
    to_layer = connector_resolve_endpoint(component->to_id);
    from_host = from_layer ? connector_find_draggable_host(from_layer) : NULL;
    to_host = to_layer ? connector_find_draggable_host(to_layer) : NULL;

    if (from_host) {
        connector_emit_node_connect_change(from_host, component, action,
                                           from_layer, component->from_anchor);
    }
    if (to_host && to_host != from_host) {
        connector_emit_node_connect_change(to_host, component, action,
                                           to_layer, component->to_anchor);
    }
}

static int connector_component_register_event(Layer* layer, const char* event_name,
                                              const char* event_func_name,
                                              EventHandler event_handler)
{
    ConnectorComponent* component;

    (void)event_handler;
    if (!layer || !layer->component || !event_func_name) {
        return -1;
    }
    if (strcmp(event_name, "onConnectChange") != 0 &&
        strcmp(event_name, "connectChange") != 0) {
        return -1;
    }

    component = (ConnectorComponent*)layer->component;
    if (event_func_name[0] == '@') {
        event_func_name++;
    }
    strncpy(component->on_connect_change_name, event_func_name,
            sizeof(component->on_connect_change_name) - 1);
    component->on_connect_change_name[sizeof(component->on_connect_change_name) - 1] = '\0';
    return 0;
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

static int connector_get_component_geometry(ConnectorComponent* component,
                                            int* x0, int* y0, int* x1, int* y1,
                                            int* cx1, int* cy1, int* cx2, int* cy2);
static void connector_apply_endpoint_fanout(Layer* canvas, Layer* ep_layer,
                                            ConnectorAnchor ep_anchor,
                                            int at_from_end, ConnectorComponent* self,
                                            int pending_extra, int* x, int* y);

static int connector_same_endpoint(Layer* layer_a, ConnectorAnchor anchor_a,
                                   Layer* layer_b, ConnectorAnchor anchor_b)
{
    return layer_a && layer_b && layer_a == layer_b && anchor_a == anchor_b;
}

static int connector_count_endpoint_usage(Layer* canvas, Layer* ep_layer,
                                          ConnectorAnchor ep_anchor,
                                          int at_from_end, ConnectorComponent* self,
                                          int* out_index, int* out_total)
{
    int i;
    int total = 0;
    int index = 0;

    if (!canvas || !ep_layer || !out_index || !out_total) {
        return 0;
    }

    *out_index = 0;
    *out_total = 0;

    for (i = 0; i < canvas->child_count; i++) {
        Layer* child = canvas->children[i];
        ConnectorComponent* other;
        Layer* from_layer;
        Layer* to_layer;

        if (!child || child->type != CONNECTOR || !child->component) {
            continue;
        }

        other = (ConnectorComponent*)child->component;
        from_layer = connector_resolve_endpoint(other->from_id);
        to_layer = connector_resolve_endpoint(other->to_id);
        if (at_from_end) {
            if (!connector_same_endpoint(from_layer, other->from_anchor, ep_layer,
                                         ep_anchor)) {
                continue;
            }
        } else if (!connector_same_endpoint(to_layer, other->to_anchor, ep_layer,
                                             ep_anchor)) {
            continue;
        }

        if (self && other == self) {
            index = total;
        }
        total++;
    }

    *out_index = index;
    *out_total = total;
    return total;
}

static void connector_apply_endpoint_fanout(Layer* canvas, Layer* ep_layer,
                                            ConnectorAnchor ep_anchor,
                                            int at_from_end, ConnectorComponent* self,
                                            int pending_extra, int* x, int* y)
{
    int total;
    int index;
    int spacing;
    int off;

    if (!canvas || !ep_layer || !x || !y) {
        return;
    }

    connector_count_endpoint_usage(canvas, ep_layer, ep_anchor, at_from_end, self,
                                   &index, &total);
    if (pending_extra) {
        index = total;
        total++;
    }
    if (total <= 1) {
        return;
    }

    spacing = 12;
    off = index * spacing - (total - 1) * spacing / 2;
    switch (ep_anchor) {
        case CONNECTOR_ANCHOR_LEFT:
        case CONNECTOR_ANCHOR_RIGHT:
            *y += off;
            break;
        case CONNECTOR_ANCHOR_TOP:
        case CONNECTOR_ANCHOR_BOTTOM:
            *x += off;
            break;
        default:
            break;
    }
}

static int connector_get_component_geometry(ConnectorComponent* component,
                                            int* x0, int* y0, int* x1, int* y1,
                                            int* cx1, int* cy1, int* cx2, int* cy2)
{
    Layer* from_layer;
    Layer* to_layer;

    if (!component || !g_ui_root) {
        return 0;
    }

    from_layer = connector_resolve_endpoint(component->from_id);
    to_layer = connector_resolve_endpoint(component->to_id);
    if (!from_layer || !to_layer ||
        !connector_layer_is_connectable(from_layer) ||
        !connector_layer_is_connectable(to_layer)) {
        return 0;
    }

    connector_get_layer_anchor_point(from_layer, component->from_anchor, x0, y0);
    connector_get_layer_anchor_point(to_layer, component->to_anchor, x1, y1);

    if (component->layer && component->layer->parent) {
        connector_apply_endpoint_fanout(component->layer->parent, from_layer,
                                        component->from_anchor, 1, component, 0,
                                        x0, y0);
        connector_apply_endpoint_fanout(component->layer->parent, to_layer,
                                        component->to_anchor, 0, component, 0,
                                        x1, y1);
    }

    if (component->curve_mode == CONNECTOR_CURVE_MANUAL &&
        component->has_ctrl1 && component->has_ctrl2) {
        *cx1 = component->ctrl1_x;
        *cy1 = component->ctrl1_y;
        *cx2 = component->ctrl2_x;
        *cy2 = component->ctrl2_y;
    } else {
        connector_auto_control_points(*x0, *y0, *x1, *y1, component->curve_mode,
                                      cx1, cy1, cx2, cy2);
    }

    return 1;
}

static int connector_dist_sq(int ax, int ay, int bx, int by)
{
    int dx = ax - bx;
    int dy = ay - by;
    return dx * dx + dy * dy;
}

static void connector_bezier_point(int x0, int y0, int cx1, int cy1,
                                   int cx2, int cy2, int x1, int y1,
                                   float t, int* out_x, int* out_y)
{
    float u = 1.0f - t;
    float tt = t * t;
    float uu = u * u;
    float uuu = uu * u;
    float ttt = tt * t;

    *out_x = (int)(uuu * x0 + 3.0f * uu * t * cx1 + 3.0f * u * tt * cx2 + ttt * x1);
    *out_y = (int)(uuu * y0 + 3.0f * uu * t * cy1 + 3.0f * u * tt * cy2 + ttt * y1);
}

static void connector_remove_child_layer(Layer* parent, Layer* child);
static void connector_refresh_canvas_draggbles(Layer* canvas);

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
    layer->register_event = connector_component_register_event;
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

    {
        cJSON* events = cJSON_GetObjectItem(json_obj, "events");
        cJSON* on_connect = events ? cJSON_GetObjectItem(events, "onConnectChange") : NULL;
        if (on_connect && cJSON_IsString(on_connect) && on_connect->valuestring[0]) {
            const char* name = on_connect->valuestring;
            if (name[0] == '@') {
                name++;
            }
            strncpy(component->on_connect_change_name, name,
                    sizeof(component->on_connect_change_name) - 1);
            component->on_connect_change_name[sizeof(component->on_connect_change_name) - 1] =
                '\0';
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

    if (g_connector_drag.active && g_connector_drag.mode == CONNECTOR_DRAG_MODIFY &&
        g_connector_drag.modify_layer == layer) {
        return;
    }

    component = (ConnectorComponent*)layer->component;
    if (!component->from_id[0] || !component->to_id[0]) {
        return;
    }

    if (!connector_get_component_geometry(component, &x0, &y0, &x1, &y1,
                                          &cx1, &cy1, &cx2, &cy2)) {
        return;
    }

    backend_render_bezier_cubic(x0, y0, cx1, cy1, cx2, cy2, x1, y1,
                                component->stroke_color,
                                component->stroke_width);
}

static int connector_is_dot_overlay_layer(const Layer* layer)
{
    size_t len;

    if (!layer || !layer->id[0]) {
        return 0;
    }

    len = strlen(layer->id);
    if (len >= 5 && strcmp(layer->id + len - 5, "_dots") == 0) {
        return 1;
    }

    return 0;
}

static void connector_add_default_ports(Layer* layer, ConnectorAnchorEntry* entries,
                                        int* entry_count, int max_entries)
{
    if (!layer || !connector_layer_is_connectable(layer)) {
        return;
    }

    if (layer->type == DRAGGABLE || layer->connectable) {
        connector_merge_anchor_entry(entries, entry_count, max_entries, layer,
                                     CONNECTOR_ANCHOR_LEFT);
        connector_merge_anchor_entry(entries, entry_count, max_entries, layer,
                                     CONNECTOR_ANCHOR_RIGHT);
    }
}

static void connector_walk_connectable_ports_in_subtree(Layer* layer,
                                                        ConnectorAnchorEntry* entries,
                                                        int* entry_count,
                                                        int max_entries)
{
    int i;

    if (!layer || layer->visible == IN_VISIBLE || connector_is_dot_overlay_layer(layer)) {
        return;
    }

    connector_add_default_ports(layer, entries, entry_count, max_entries);

    if (layer->children) {
        for (i = 0; i < layer->child_count; i++) {
            if (layer->children[i]) {
                connector_walk_connectable_ports_in_subtree(layer->children[i], entries,
                                                            entry_count, max_entries);
            }
        }
    }

    if (layer->sub) {
        connector_walk_connectable_ports_in_subtree(layer->sub, entries, entry_count,
                                                    max_entries);
    }
}

static void connector_merge_anchors_for_draggable_tree(Layer* root, Layer* draggable,
                                                       ConnectorAnchorEntry* entries,
                                                       int* entry_count, int max_entries)
{
    ConnectorComponent* component;
    int i;

    if (!root || !draggable) {
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
                connector_merge_anchors_for_draggable_tree(root->children[i], draggable,
                                                             entries, entry_count,
                                                             max_entries);
            }
        }
    }

    if (root->sub) {
        connector_merge_anchors_for_draggable_tree(root->sub, draggable, entries,
                                                   entry_count, max_entries);
    }
}

static void connector_merge_port_anchors_for_draggable_tree(Layer* root, Layer* draggable,
                                                            ConnectorAnchorEntry* entries,
                                                            int* entry_count,
                                                            int max_entries)
{
    if (!draggable || !entries || !entry_count || max_entries <= 0) {
        return;
    }

    connector_walk_connectable_ports_in_subtree(draggable, entries, entry_count, max_entries);
    if (root) {
        connector_merge_anchors_for_draggable_tree(root, draggable, entries, entry_count,
                                                   max_entries);
    }
}

void connector_collect_port_anchors_for_draggable_tree(Layer* root, Layer* draggable,
                                                       ConnectorAnchorEntry* entries,
                                                       int* entry_count,
                                                       int max_entries)
{
    if (!entries || !entry_count || max_entries <= 0) {
        return;
    }

    *entry_count = 0;
    connector_merge_port_anchors_for_draggable_tree(root, draggable, entries, entry_count,
                                                    max_entries);
}

void connector_layer_endpoint_id(Layer* layer, char* id_out, size_t id_size)
{
    if (!id_out || id_size == 0) {
        return;
    }

    id_out[0] = '\0';
    if (!layer || !layer->id[0]) {
        return;
    }

    strncpy(id_out, layer->id, id_size - 1);
    id_out[id_size - 1] = '\0';
}

Layer* connector_find_canvas(Layer* layer)
{
    Layer* current = layer;

    while (current) {
        Layer* parent = current->parent;
        int i;

        if (parent) {
            for (i = 0; i < parent->child_count; i++) {
                if (parent->children[i] && parent->children[i]->type == DRAGGABLE) {
                    return parent;
                }
            }
        }

        current = parent;
    }

    return NULL;
}

static int connector_hit_test_entries(int x, int y, int dot_size,
                                      const ConnectorAnchorEntry* entries, int entry_count,
                                      Layer** out_layer, ConnectorAnchor* out_anchor)
{
    int hit_radius;
    int best_dist = -1;
    int i;
    int anchor;
    int ax;
    int ay;
    int dx;
    int dy;
    int dist;

    if (!entries || entry_count <= 0) {
        return 0;
    }

    hit_radius = dot_size + 6;
    if (hit_radius < 8) {
        hit_radius = 8;
    }

    for (i = 0; i < entry_count; i++) {
        for (anchor = CONNECTOR_ANCHOR_CENTER; anchor <= CONNECTOR_ANCHOR_RIGHT; anchor++) {
            if (!(entries[i].anchor_mask & (1u << anchor))) {
                continue;
            }

            connector_get_layer_anchor_point(entries[i].layer, (ConnectorAnchor)anchor,
                                             &ax, &ay);
            dx = x - ax;
            dy = y - ay;
            dist = dx * dx + dy * dy;
            if (dist > hit_radius * hit_radius) {
                continue;
            }

            if (best_dist < 0 || dist < best_dist) {
                best_dist = dist;
                if (out_layer) {
                    *out_layer = entries[i].layer;
                }
                if (out_anchor) {
                    *out_anchor = (ConnectorAnchor)anchor;
                }
            }
        }
    }

    return best_dist >= 0;
}

int connector_hit_test_draggable_ports(Layer* draggable, int x, int y, int dot_size,
                                       Layer** out_layer, ConnectorAnchor* out_anchor)
{
    ConnectorAnchorEntry entries[CONNECTOR_MAX_ANCHOR_ENTRIES];
    int entry_count = 0;

    if (!draggable || draggable->type != DRAGGABLE) {
        return 0;
    }

    connector_collect_port_anchors_for_draggable_tree(g_ui_root, draggable, entries,
                                                      &entry_count,
                                                      CONNECTOR_MAX_ANCHOR_ENTRIES);
    return connector_hit_test_entries(x, y, dot_size, entries, entry_count,
                                      out_layer, out_anchor);
}

static void connector_collect_canvas_port_entries(Layer* canvas,
                                                  ConnectorAnchorEntry* entries,
                                                  int* entry_count, int max_entries)
{
    int i;

    if (!canvas || !entries || !entry_count) {
        return;
    }

    *entry_count = 0;
    for (i = 0; i < canvas->child_count; i++) {
        Layer* child = canvas->children[i];
        if (child && child->type == DRAGGABLE) {
            connector_merge_port_anchors_for_draggable_tree(g_ui_root, child, entries,
                                                            entry_count, max_entries);
        }
    }
}

int connector_hit_test_canvas_ports(Layer* canvas, int x, int y, int dot_size,
                                    Layer** out_layer, ConnectorAnchor* out_anchor)
{
    ConnectorAnchorEntry entries[CONNECTOR_MAX_ANCHOR_ENTRIES];
    int entry_count = 0;

    if (!canvas) {
        return 0;
    }

    connector_collect_canvas_port_entries(canvas, entries, &entry_count,
                                          CONNECTOR_MAX_ANCHOR_ENTRIES);
    return connector_hit_test_entries(x, y, dot_size, entries, entry_count,
                                      out_layer, out_anchor);
}

static int connector_hit_test_curve(Layer* canvas, int x, int y, int threshold,
                                    Layer** out_connector)
{
    int i;
    int best_dist = -1;
    Layer* best_layer = NULL;
    int x0;
    int y0;
    int x1;
    int y1;
    int cx1;
    int cy1;
    int cx2;
    int cy2;
    int j;
    int px;
    int py;
    int dist;
    int limit;

    if (!canvas || threshold < 1) {
        return 0;
    }

    limit = threshold * threshold;

    for (i = 0; i < canvas->child_count; i++) {
        Layer* child = canvas->children[i];
        ConnectorComponent* component;

        if (!child || child->type != CONNECTOR || !child->component) {
            continue;
        }

        component = (ConnectorComponent*)child->component;
        if (!connector_get_component_geometry(component, &x0, &y0, &x1, &y1,
                                              &cx1, &cy1, &cx2, &cy2)) {
            continue;
        }

        for (j = 0; j <= 24; j++) {
            connector_bezier_point(x0, y0, cx1, cy1, cx2, cy2, x1, y1,
                                   j / 24.0f, &px, &py);
            dist = connector_dist_sq(x, y, px, py);
            if (dist <= limit && (best_dist < 0 || dist < best_dist)) {
                best_dist = dist;
                best_layer = child;
            }
        }
    }

    if (!best_layer) {
        return 0;
    }

    if (out_connector) {
        *out_connector = best_layer;
    }

    return 1;
}

static int connector_endpoint_matches(ConnectorComponent* component,
                                      Layer* endpoint, ConnectorAnchor anchor)
{
    Layer* from_layer;
    Layer* to_layer;

    if (!component || !endpoint) {
        return 0;
    }

    from_layer = connector_resolve_endpoint(component->from_id);
    to_layer = connector_resolve_endpoint(component->to_id);
    return (from_layer == endpoint && component->from_anchor == anchor) ||
           (to_layer == endpoint && component->to_anchor == anchor);
}

static int connector_remove_at_endpoint(Layer* canvas, Layer* endpoint,
                                        ConnectorAnchor anchor)
{
    int i;
    int removed = 0;

    if (!canvas || !endpoint) {
        return 0;
    }

    for (i = canvas->child_count - 1; i >= 0; i--) {
        Layer* child = canvas->children[i];
        ConnectorComponent* component;

        if (!child || child->type != CONNECTOR || !child->component) {
            continue;
        }

        component = (ConnectorComponent*)child->component;
        if (!connector_endpoint_matches(component, endpoint, anchor)) {
            continue;
        }

        connector_remove_child_layer(canvas, child);
        removed = 1;
    }

    if (removed) {
        layout_layer(canvas);
        connector_refresh_canvas_draggbles(canvas);
    }

    return removed;
}

int connector_try_remove_at(Layer* canvas, int x, int y, int dot_size)
{
    Layer* hit_layer;
    ConnectorAnchor hit_anchor;
    Layer* connector_layer;

    if (!canvas) {
        return 0;
    }

    if (connector_hit_test_canvas_ports(canvas, x, y, dot_size,
                                        &hit_layer, &hit_anchor)) {
        return connector_remove_at_endpoint(canvas, hit_layer, hit_anchor);
    }

    if (connector_hit_test_curve(canvas, x, y, 8, &connector_layer)) {
        connector_remove_child_layer(canvas, connector_layer);
        layout_layer(canvas);
        connector_refresh_canvas_draggbles(canvas);
        return 1;
    }

    return 0;
}

static void connector_render_drag_preview(void);
static void connector_destroy_capture_layer(void);
static int connector_create_capture_layer(void);
static int connector_capture_handle_mouse(Layer* layer, MouseEvent* event);
static void connector_capture_render(Layer* layer);
static void connector_sync_pick_layer(Layer* canvas);
static int connector_pick_handle_mouse(Layer* layer, MouseEvent* event);
static int connector_interaction_start_modify(Layer* connector_layer, int x, int y,
                                              int dot_size);

static void connector_refresh_canvas_draggbles(Layer* canvas)
{
    int i;

    if (!canvas) {
        return;
    }

    for (i = 0; i < canvas->child_count; i++) {
        Layer* child = canvas->children[i];
        if (child && child->type == DRAGGABLE && child->layout) {
            child->layout(child);
        }
    }

    connector_sync_pick_layer(canvas);
}

static void connector_move_child_to_end(Layer* parent, Layer* child)
{
    int i;
    int from_index = -1;
    Layer* moved;

    if (!parent || !child || !parent->children) {
        return;
    }

    for (i = 0; i < parent->child_count; i++) {
        if (parent->children[i] == child) {
            from_index = i;
            break;
        }
    }

    if (from_index < 0 || from_index == parent->child_count - 1) {
        return;
    }

    moved = parent->children[from_index];
    for (i = from_index; i < parent->child_count - 1; i++) {
        parent->children[i] = parent->children[i + 1];
    }
    parent->children[parent->child_count - 1] = moved;
}

static void connector_sync_pick_layer(Layer* canvas)
{
    Layer* pick = NULL;
    int i;
    Layer** children;

    if (!canvas) {
        return;
    }

    for (i = 0; i < canvas->child_count; i++) {
        if (canvas->children[i] &&
            strcmp(canvas->children[i]->id, "_connector_pick") == 0) {
            pick = canvas->children[i];
            break;
        }
    }

    if (!pick) {
        pick = layer_create(canvas, 0, 0, canvas->rect.w, canvas->rect.h);
        if (!pick) {
            return;
        }

        pick->type = VIEW;
        strncpy(pick->id, "_connector_pick", sizeof(pick->id) - 1);
        pick->id[sizeof(pick->id) - 1] = '\0';
        pick->bg_color.a = 0;
        pick->focusable = 0;
        pick->handle_mouse_event = connector_pick_handle_mouse;
        pick->parent = canvas;

        children = (Layer**)realloc(canvas->children,
                                    (size_t)(canvas->child_count + 1) * sizeof(Layer*));
        if (!children) {
            destroy_layer(pick);
            return;
        }

        canvas->children = children;
        canvas->children[canvas->child_count++] = pick;
    }

    connector_move_child_to_end(canvas, pick);
    pick->rect.x = canvas->rect.x;
    pick->rect.y = canvas->rect.y;
    pick->rect.w = canvas->rect.w;
    pick->rect.h = canvas->rect.h;
}

static int connector_pick_handle_mouse(Layer* layer, MouseEvent* event)
{
    Layer* canvas;
    Layer* connector_layer;

    if (!layer || !event || g_connector_drag.active) {
        return 0;
    }

    if (event->state != SDL_PRESSED || event->button != SDL_BUTTON_LEFT) {
        return 0;
    }

    canvas = layer->parent;
    if (!canvas) {
        return 0;
    }

    if (connector_hit_test_canvas_ports(canvas, event->x, event->y, 4, NULL, NULL)) {
        return 0;
    }

    if (!connector_hit_test_curve(canvas, event->x, event->y, 8, &connector_layer)) {
        return 0;
    }

    return connector_interaction_start_modify(connector_layer, event->x, event->y, 4);
}

static int connector_canvas_insert_layer(Layer* canvas, Layer* connector_layer)
{
    Layer** children;
    int count;

    if (!canvas || !connector_layer) {
        return -1;
    }

    count = canvas->child_count;
    children = (Layer**)realloc(canvas->children, (size_t)(count + 1) * sizeof(Layer*));
    if (!children) {
        return -1;
    }

    if (count > 0) {
        memmove(&children[1], &children[0], (size_t)count * sizeof(Layer*));
    }

    children[0] = connector_layer;
    canvas->children = children;
    canvas->child_count = count + 1;
    connector_layer->parent = canvas;
    return 0;
}

Layer* connector_create_link(Layer* canvas, Layer* from_layer, ConnectorAnchor from_anchor,
                             Layer* to_layer, ConnectorAnchor to_anchor)
{
    static int s_edge_counter = 0;
    ConnectorComponent* component;
    char edge_id[64];
    Layer* layer;

    if (!canvas || !from_layer || !to_layer ||
        !connector_layer_is_connectable(from_layer) ||
        !connector_layer_is_connectable(to_layer)) {
        return NULL;
    }

    layer = layer_create(canvas, 0, 0, 0, 0);
    if (!layer) {
        return NULL;
    }

    snprintf(edge_id, sizeof(edge_id), "edge_drag_%d", ++s_edge_counter);
    strncpy(layer->id, edge_id, sizeof(layer->id) - 1);
    layer->id[sizeof(layer->id) - 1] = '\0';
    layer->type = CONNECTOR;
    layer->visible = VISIBLE;
    layer->focusable = 0;

    component = connector_component_create(layer);
    if (!component) {
        destroy_layer(layer);
        return NULL;
    }

    connector_layer_endpoint_id(from_layer, component->from_id, sizeof(component->from_id));
    connector_layer_endpoint_id(to_layer, component->to_id, sizeof(component->to_id));
    component->from_anchor = from_anchor;
    component->to_anchor = to_anchor;
    component->stroke_color = (Color){137, 180, 250, 255};
    component->stroke_width = 2;

    if (connector_canvas_insert_layer(canvas, layer) != 0) {
        destroy_layer(layer);
        return NULL;
    }

    layout_layer(canvas);
    connector_refresh_canvas_draggbles(canvas);
    connector_emit_connect_change(layer, component, "attach");
    return layer;
}

static void connector_remove_child_layer(Layer* parent, Layer* child)
{
    int i;
    int j;

    if (!parent || !child || !parent->children) {
        return;
    }

    for (i = 0; i < parent->child_count; i++) {
        if (parent->children[i] != child) {
            continue;
        }
        if (child->type == CONNECTOR && child->component) {
            connector_emit_connect_change(child,
                                          (ConnectorComponent*)child->component,
                                          "detach");
        }
        for (j = i; j < parent->child_count - 1; j++) {
            parent->children[j] = parent->children[j + 1];
        }
        parent->child_count--;
        destroy_layer(child);
        return;
    }
}

static void connector_destroy_capture_layer(void)
{
    if (!g_connector_drag.capture_layer || !g_connector_drag.canvas) {
        g_connector_drag.capture_layer = NULL;
        return;
    }

    connector_remove_child_layer(g_connector_drag.canvas, g_connector_drag.capture_layer);
    g_connector_drag.capture_layer = NULL;
}

static void connector_capture_render(Layer* layer)
{
    (void)layer;
    connector_render_drag_preview();
}

static int connector_capture_handle_mouse(Layer* layer, MouseEvent* event)
{
    Layer* to_layer;
    ConnectorAnchor to_anchor;

    (void)layer;

    if (!g_connector_drag.active || !event) {
        return 0;
    }

    g_connector_drag.mouse_x = event->x;
    g_connector_drag.mouse_y = event->y;
    g_connector_drag.hover_layer = NULL;

    if (event->state == SDL_MOUSEMOTION) {
        if (connector_hit_test_canvas_ports(g_connector_drag.canvas, event->x, event->y,
                                            g_connector_drag.dot_size, &to_layer,
                                            &to_anchor)) {
            g_connector_drag.hover_layer = to_layer;
            g_connector_drag.hover_anchor = to_anchor;
        }
        return 1;
    }

    if (event->state == SDL_RELEASED && event->button == SDL_BUTTON_LEFT) {
        if (g_connector_drag.mode == CONNECTOR_DRAG_MODIFY &&
            g_connector_drag.modify_layer && g_connector_drag.modify_layer->component) {
            if (connector_hit_test_canvas_ports(g_connector_drag.canvas, event->x, event->y,
                                                g_connector_drag.dot_size, &to_layer,
                                                &to_anchor)) {
                ConnectorComponent* component =
                    (ConnectorComponent*)g_connector_drag.modify_layer->component;
                Layer* fixed_layer;
                ConnectorAnchor fixed_anchor;
                int valid = 1;

                if (g_connector_drag.modify_from_end) {
                    fixed_layer = connector_resolve_endpoint(component->to_id);
                    fixed_anchor = component->to_anchor;
                    if (fixed_layer && to_layer == fixed_layer &&
                        to_anchor == fixed_anchor) {
                        valid = 0;
                    } else {
                        connector_layer_endpoint_id(to_layer, component->from_id,
                                                    sizeof(component->from_id));
                        component->from_anchor = to_anchor;
                    }
                } else {
                    fixed_layer = connector_resolve_endpoint(component->from_id);
                    fixed_anchor = component->from_anchor;
                    if (fixed_layer && to_layer == fixed_layer &&
                        to_anchor == fixed_anchor) {
                        valid = 0;
                    } else {
                        connector_layer_endpoint_id(to_layer, component->to_id,
                                                    sizeof(component->to_id));
                        component->to_anchor = to_anchor;
                    }
                }

                if (valid) {
                    layout_layer(g_connector_drag.canvas);
                    connector_refresh_canvas_draggbles(g_connector_drag.canvas);
                    connector_emit_connect_change(g_connector_drag.modify_layer,
                                                  component, "rebind");
                }
            }
        } else if (connector_hit_test_canvas_ports(g_connector_drag.canvas, event->x, event->y,
                                                   g_connector_drag.dot_size, &to_layer,
                                                   &to_anchor) &&
                   to_layer && (to_layer != g_connector_drag.from_layer ||
                                to_anchor != g_connector_drag.from_anchor)) {
            connector_create_link(g_connector_drag.canvas, g_connector_drag.from_layer,
                                g_connector_drag.from_anchor, to_layer, to_anchor);
        }

        connector_destroy_capture_layer();
        memset(&g_connector_drag, 0, sizeof(g_connector_drag));
        return 1;
    }

    return 1;
}

static int connector_create_capture_layer(void)
{
    Layer* canvas;
    Layer* capture;
    Layer** children;

    canvas = g_connector_drag.canvas;
    if (!canvas) {
        return 0;
    }

    capture = layer_create(canvas, 0, 0, canvas->rect.w, canvas->rect.h);
    if (!capture) {
        return 0;
    }

    capture->type = VIEW;
    strncpy(capture->id, "_connector_capture", sizeof(capture->id) - 1);
    capture->id[sizeof(capture->id) - 1] = '\0';
    capture->bg_color.a = 0;
    capture->focusable = 0;
    capture->render = connector_capture_render;
    capture->handle_mouse_event = connector_capture_handle_mouse;
    capture->parent = canvas;
    capture->rect.x = canvas->rect.x;
    capture->rect.y = canvas->rect.y;
    capture->rect.w = canvas->rect.w;
    capture->rect.h = canvas->rect.h;

    children = (Layer**)realloc(canvas->children,
                                (size_t)(canvas->child_count + 1) * sizeof(Layer*));
    if (!children) {
        destroy_layer(capture);
        return 0;
    }

    canvas->children = children;
    canvas->children[canvas->child_count++] = capture;
    g_connector_drag.capture_layer = capture;
    return 1;
}

int connector_interaction_start(Layer* from_layer, ConnectorAnchor from_anchor,
                                int dot_size, int mouse_x, int mouse_y)
{
    if (g_connector_drag.active || !from_layer || !connector_layer_is_connectable(from_layer)) {
        return 0;
    }

    memset(&g_connector_drag, 0, sizeof(g_connector_drag));
    g_connector_drag.active = 1;
    g_connector_drag.mode = CONNECTOR_DRAG_CREATE;
    g_connector_drag.from_layer = from_layer;
    g_connector_drag.from_anchor = from_anchor;
    g_connector_drag.canvas = connector_find_canvas(from_layer);
    g_connector_drag.dot_size = dot_size > 0 ? dot_size : 4;
    g_connector_drag.mouse_x = mouse_x;
    g_connector_drag.mouse_y = mouse_y;
    if (!g_connector_drag.canvas) {
        g_connector_drag.active = 0;
        return 0;
    }

    if (!connector_create_capture_layer()) {
        memset(&g_connector_drag, 0, sizeof(g_connector_drag));
        return 0;
    }

    return 1;
}

static int connector_interaction_start_modify(Layer* connector_layer, int x, int y,
                                              int dot_size)
{
    ConnectorComponent* component;
    int x0;
    int y0;
    int x1;
    int y1;
    int cx1;
    int cy1;
    int cx2;
    int cy2;

    if (g_connector_drag.active || !connector_layer || connector_layer->type != CONNECTOR ||
        !connector_layer->component) {
        return 0;
    }

    component = (ConnectorComponent*)connector_layer->component;
    if (!component->from_id[0] || !component->to_id[0] ||
        !connector_get_component_geometry(component, &x0, &y0, &x1, &y1,
                                          &cx1, &cy1, &cx2, &cy2)) {
        return 0;
    }

    memset(&g_connector_drag, 0, sizeof(g_connector_drag));
    g_connector_drag.active = 1;
    g_connector_drag.mode = CONNECTOR_DRAG_MODIFY;
    g_connector_drag.modify_layer = connector_layer;
    g_connector_drag.modify_from_end =
        connector_dist_sq(x, y, x0, y0) <= connector_dist_sq(x, y, x1, y1);
    g_connector_drag.canvas = connector_layer->parent;
    g_connector_drag.dot_size = dot_size > 0 ? dot_size : 4;
    g_connector_drag.mouse_x = x;
    g_connector_drag.mouse_y = y;
    if (!g_connector_drag.canvas) {
        g_connector_drag.active = 0;
        return 0;
    }

    if (!connector_create_capture_layer()) {
        memset(&g_connector_drag, 0, sizeof(g_connector_drag));
        return 0;
    }

    return 1;
}

static void connector_render_drag_preview(void)
{
    int x0;
    int y0;
    int x1;
    int y1;
    int cx1;
    int cy1;
    int cx2;
    int cy2;
    Color preview_color;
    Color hover_color;
    int ax;
    int ay;
    ConnectorComponent* component;

    if (!g_connector_drag.active) {
        return;
    }

    Color dot_color;
    int fx;
    int fy;
    int dx;
    int dy;

    preview_color = (Color){137, 180, 250, 180};
    hover_color = (Color){166, 227, 161, 255};
    dot_color = (Color){180, 180, 180, 255};

    if (g_connector_drag.mode == CONNECTOR_DRAG_MODIFY &&
        g_connector_drag.modify_layer && g_connector_drag.modify_layer->component) {
        component = (ConnectorComponent*)g_connector_drag.modify_layer->component;
        if (!connector_get_component_geometry(component, &x0, &y0, &x1, &y1,
                                              &cx1, &cy1, &cx2, &cy2)) {
            return;
        }

        if (g_connector_drag.modify_from_end) {
            fx = x1;
            fy = y1;
            if (g_connector_drag.hover_layer) {
                connector_get_layer_anchor_point(g_connector_drag.hover_layer,
                                                 g_connector_drag.hover_anchor,
                                                 &x0, &y0);
                connector_apply_endpoint_fanout(g_connector_drag.canvas,
                                                g_connector_drag.hover_layer,
                                                g_connector_drag.hover_anchor,
                                                1, component, 0, &x0, &y0);
            } else {
                x0 = g_connector_drag.mouse_x;
                y0 = g_connector_drag.mouse_y;
            }
            dx = x0;
            dy = y0;
        } else {
            fx = x0;
            fy = y0;
            if (g_connector_drag.hover_layer) {
                connector_get_layer_anchor_point(g_connector_drag.hover_layer,
                                                 g_connector_drag.hover_anchor,
                                                 &x1, &y1);
                connector_apply_endpoint_fanout(g_connector_drag.canvas,
                                                g_connector_drag.hover_layer,
                                                g_connector_drag.hover_anchor,
                                                0, component, 0, &x1, &y1);
            } else {
                x1 = g_connector_drag.mouse_x;
                y1 = g_connector_drag.mouse_y;
            }
            dx = x1;
            dy = y1;
        }

        connector_auto_control_points(x0, y0, x1, y1, CONNECTOR_CURVE_AUTO,
                                      &cx1, &cy1, &cx2, &cy2);
        backend_render_bezier_cubic(x0, y0, cx1, cy1, cx2, cy2, x1, y1,
                                    preview_color, 2);
        connector_render_dot(fx, fy, g_connector_drag.dot_size, dot_color);
        connector_render_dot(dx, dy, g_connector_drag.dot_size, dot_color);

        if (g_connector_drag.hover_layer) {
            connector_get_layer_anchor_point(g_connector_drag.hover_layer,
                                             g_connector_drag.hover_anchor, &ax, &ay);
            connector_render_dot(ax, ay, g_connector_drag.dot_size + 2, hover_color);
        }
        return;
    }

    if (!g_connector_drag.from_layer) {
        return;
    }

    connector_get_layer_anchor_point(g_connector_drag.from_layer,
                                     g_connector_drag.from_anchor, &x0, &y0);
    if (g_connector_drag.canvas) {
        connector_apply_endpoint_fanout(g_connector_drag.canvas,
                                        g_connector_drag.from_layer,
                                        g_connector_drag.from_anchor, 1, NULL, 1,
                                        &x0, &y0);
    }

    if (g_connector_drag.hover_layer) {
        connector_get_layer_anchor_point(g_connector_drag.hover_layer,
                                         g_connector_drag.hover_anchor, &x1, &y1);
        if (g_connector_drag.canvas) {
            connector_apply_endpoint_fanout(g_connector_drag.canvas,
                                            g_connector_drag.hover_layer,
                                            g_connector_drag.hover_anchor, 0, NULL, 1,
                                            &x1, &y1);
        }
    } else {
        x1 = g_connector_drag.mouse_x;
        y1 = g_connector_drag.mouse_y;
    }
    connector_auto_control_points(x0, y0, x1, y1, CONNECTOR_CURVE_AUTO,
                                  &cx1, &cy1, &cx2, &cy2);

    backend_render_bezier_cubic(x0, y0, cx1, cy1, cx2, cy2, x1, y1,
                                preview_color, 2);

    if (g_connector_drag.hover_layer) {
        connector_get_layer_anchor_point(g_connector_drag.hover_layer,
                                         g_connector_drag.hover_anchor, &ax, &ay);
        connector_render_dot(ax, ay, g_connector_drag.dot_size + 2, hover_color);
    }
}
