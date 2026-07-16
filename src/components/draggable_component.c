#include "draggable_component.h"
#include "connector_component.h"
#include "../backend.h"
#include "../event.h"
#include "../layout.h"
#include "../layer.h"
#include "../util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern Layer* g_ui_root;

static void draggable_ensure_absolute_layout(Layer* layer)
{
    if (!layer) {
        return;
    }

    if (!layer->layout_manager) {
        layer->layout_manager = (LayoutManager*)calloc(1, sizeof(LayoutManager));
        if (!layer->layout_manager) {
            return;
        }
        layer->layout_manager->type = LAYOUT_ABSOLUTE;
        return;
    }

    layer->layout_manager->type = LAYOUT_ABSOLUTE;
}

static void draggable_sync_layout_base(Layer* layer)
{
    Layer* parent = layer ? layer->parent : NULL;
    int scroll_x = 0;
    int scroll_y = 0;

    if (!layer || !parent) {
        return;
    }

    if (parent->scrollable == 1 || parent->scrollable == 3) {
        scroll_y = parent->scroll_offset;
    }
    if (parent->scrollable == 2 || parent->scrollable == 3) {
        scroll_x = parent->scroll_offset_x;
    }

    layer->layout_base_rect.x = layer->rect.x - parent->rect.x + scroll_x;
    layer->layout_base_rect.y = layer->rect.y - parent->rect.y + scroll_y;
    layer->layout_base_valid = 1;
}

static void draggable_emit_drag_change(Layer* layer, DraggableComponent* component,
                                       const char* phase, int prev_x, int prev_y)
{
    char detail[256];
    EventHandler handler;

    if (!layer || !component || !phase || !component->on_drag_change_name[0]) {
        return;
    }

    handler = find_event_by_name(component->on_drag_change_name);
    if (!handler) {
        return;
    }

    snprintf(detail, sizeof(detail),
             "{\"phase\":\"%s\",\"position\":[%d,%d],\"previous\":[%d,%d]}",
             phase, layer->rect.x, layer->rect.y, prev_x, prev_y);

    if (!layer->event) {
        layer->event = (Event*)calloc(1, sizeof(Event));
    }
    if (layer->event) {
        strncpy(layer->event->click_name, component->on_drag_change_name,
                sizeof(layer->event->click_name) - 1);
        layer->event->click_name[sizeof(layer->event->click_name) - 1] = '\0';
    }

    layer_set_text(layer, detail);
    EVENT_INVOKE(handler, layer);
}

static int draggable_component_register_event(Layer* layer, const char* event_name,
                                              const char* event_func_name,
                                              EventHandler event_handler)
{
    DraggableComponent* component;

    (void)event_handler;
    if (!layer || !layer->component || !event_func_name) {
        return -1;
    }
    if (strcmp(event_name, "onDragChange") != 0 &&
        strcmp(event_name, "dragChange") != 0) {
        return -1;
    }

    component = (DraggableComponent*)layer->component;
    if (event_func_name[0] == '@') {
        event_func_name++;
    }
    strncpy(component->on_drag_change_name, event_func_name,
            sizeof(component->on_drag_change_name) - 1);
    component->on_drag_change_name[sizeof(component->on_drag_change_name) - 1] = '\0';
    return 0;
}

static int draggable_is_drag_handle(DraggableComponent* component, Layer* layer,
                                    int x, int y)
{
    Rect handle_rect;

    if (!component || !layer) {
        return 0;
    }

    if (!is_point_in_rect(x, y, layer->rect)) {
        return 0;
    }

    handle_rect = layer->rect;
    handle_rect.h = component->drag_handle_height;
    return is_point_in_rect(x, y, handle_rect);
}

static int draggable_dot_overlay_handle_mouse(Layer* layer, MouseEvent* event)
{
    Layer* parent;
    DraggableComponent* component;
    Layer* hit_layer;
    ConnectorAnchor hit_anchor;

    if (!layer || !event) {
        return 0;
    }

    parent = layer->parent;
    if (!parent || parent->type != DRAGGABLE || !parent->component) {
        return 0;
    }

    component = (DraggableComponent*)parent->component;
    if (!component->show_dots || component->dot_size <= 0) {
        return 0;
    }

    if (event->state == SDL_PRESSED && event->button == SDL_BUTTON_LEFT) {
        if (connector_hit_test_draggable_ports(parent, event->x, event->y,
                                               component->dot_size, &hit_layer,
                                               &hit_anchor)) {
            return connector_interaction_start(hit_layer, hit_anchor,
                                               component->dot_size,
                                               event->x, event->y);
        }
    }

    if (event->state == SDL_PRESSED && event->button == SDL_BUTTON_RIGHT) {
        Layer* canvas = connector_find_canvas(parent);
        if (canvas) {
            return connector_try_remove_at(canvas, event->x, event->y,
                                           component->dot_size);
        }
    }

    return 0;
}

static void draggable_dot_overlay_render(Layer* overlay)
{
    Layer* parent;
    DraggableComponent* component;
    ConnectorAnchorEntry entries[CONNECTOR_MAX_ANCHOR_ENTRIES];
    int entry_count = 0;
    int i;
    Rect prev_clip;

    if (!overlay) {
        return;
    }

    parent = overlay->parent;
    if (!parent || parent->type != DRAGGABLE || !parent->component) {
        return;
    }

    component = (DraggableComponent*)parent->component;
    if (!component->show_dots || component->dot_size <= 0 || !g_ui_root) {
        return;
    }

    connector_collect_port_anchors_for_draggable_tree(g_ui_root, parent, entries,
                                                 &entry_count,
                                                 CONNECTOR_MAX_ANCHOR_ENTRIES);
    if (entry_count <= 0) {
        return;
    }

    backend_render_get_clip_rect(&prev_clip);
    backend_render_set_clip_rect(NULL);
    for (i = 0; i < entry_count; i++) {
        connector_render_dots_for_layer(entries[i].layer, entries[i].anchor_mask,
                                        component->dot_size, component->dot_color);
    }
    backend_render_set_clip_rect(&prev_clip);
}

static int draggable_has_visible_dots(DraggableComponent* component)
{
    ConnectorAnchorEntry entries[CONNECTOR_MAX_ANCHOR_ENTRIES];
    int entry_count = 0;

    if (!component || !component->layer || !component->show_dots ||
        component->dot_size <= 0 || !g_ui_root) {
        return 0;
    }

    connector_collect_port_anchors_for_draggable_tree(g_ui_root, component->layer, entries,
                                                 &entry_count,
                                                 CONNECTOR_MAX_ANCHOR_ENTRIES);
    return entry_count > 0;
}

static void draggable_sync_dot_overlay(DraggableComponent* component)
{
    Layer* parent;

    if (!component || !component->layer) {
        return;
    }

    parent = component->layer;

    if (!draggable_has_visible_dots(component)) {
        if (component->dot_overlay) {
            layer_hide(component->dot_overlay);
        }
        return;
    }

    if (!component->dot_overlay) {
        Layer* overlay = layer_create(parent, 0, 0, parent->rect.w, parent->rect.h);
        Layer** children;

        if (!overlay) {
            return;
        }

        overlay->type = VIEW;
        snprintf(overlay->id, sizeof(overlay->id), "%s_dots", parent->id);
        overlay->bg_color.a = 0;
        overlay->focusable = 0;
        overlay->render = draggable_dot_overlay_render;
        overlay->handle_mouse_event = draggable_dot_overlay_handle_mouse;

        overlay->parent = parent;
        children = (Layer**)realloc(parent->children,
                                  (size_t)(parent->child_count + 1) * sizeof(Layer*));
        if (!children) {
            return;
        }
        parent->children = children;
        parent->children[parent->child_count++] = overlay;
        component->dot_overlay = overlay;
    } else {
        layer_set_visible(component->dot_overlay, VISIBLE);
    }

    component->dot_overlay->layout_base_rect.x = 0;
    component->dot_overlay->layout_base_rect.y = 0;
    component->dot_overlay->layout_base_rect.w = parent->rect.w;
    component->dot_overlay->layout_base_rect.h = parent->rect.h;
    component->dot_overlay->layout_base_valid = 1;
    component->dot_overlay->rect.x = parent->rect.x;
    component->dot_overlay->rect.y = parent->rect.y;
    component->dot_overlay->rect.w = parent->rect.w;
    component->dot_overlay->rect.h = parent->rect.h;
}

static void draggable_component_layout(Layer* layer)
{
    DraggableComponent* component;

    if (!layer || !layer->component) {
        return;
    }

    component = (DraggableComponent*)layer->component;
    draggable_sync_dot_overlay(component);
}

static void draggable_layer_destroy(Layer* layer)
{
    if (!layer || !layer->component) {
        return;
    }
    draggable_component_destroy((DraggableComponent*)layer->component);
    layer->component = NULL;
}

DraggableComponent* draggable_component_create(Layer* layer)
{
    DraggableComponent* component;

    if (!layer) {
        return NULL;
    }

    component = (DraggableComponent*)calloc(1, sizeof(DraggableComponent));
    if (!component) {
        return NULL;
    }

    component->layer = layer;
    component->drag_handle_height = 36;
    component->show_dots = 1;
    component->dot_size = 4;
    component->dot_color = (Color){137, 180, 250, 255};
    draggable_ensure_absolute_layout(layer);
    layer->component = component;
    layer->render = draggable_component_render;
    layer->layout = draggable_component_layout;
    layer->handle_mouse_event = draggable_component_handle_mouse_event;
    layer->register_event = draggable_component_register_event;
    layer->on_destroy = draggable_layer_destroy;

    return component;
}

DraggableComponent* draggable_component_create_from_json(Layer* layer, cJSON* json_obj)
{
    DraggableComponent* component = draggable_component_create(layer);
    cJSON* style;
    cJSON* handle_item;

    if (!component) {
        return NULL;
    }

    handle_item = json_obj ? cJSON_GetObjectItem(json_obj, "dragHandleHeight") : NULL;
    if (handle_item && cJSON_IsNumber(handle_item)) {
        component->drag_handle_height = handle_item->valueint;
    }

    style = json_obj ? cJSON_GetObjectItem(json_obj, "style") : NULL;
    if (style) {
        cJSON* dots_item = cJSON_GetObjectItem(style, "dots");
        if (dots_item) {
            if (cJSON_IsBool(dots_item)) {
                component->show_dots = cJSON_IsTrue(dots_item);
            } else if (cJSON_IsNumber(dots_item)) {
                component->show_dots = dots_item->valueint != 0;
            }
        }

        if (cJSON_HasObjectItem(style, "dotSize")) {
            component->dot_size = cJSON_GetObjectItem(style, "dotSize")->valueint;
        }

        if (cJSON_HasObjectItem(style, "dotColor")) {
            parse_color(cJSON_GetObjectItem(style, "dotColor")->valuestring,
                        &component->dot_color);
        }
    }

    {
        cJSON* events = json_obj ? cJSON_GetObjectItem(json_obj, "events") : NULL;
        cJSON* on_drag = events ? cJSON_GetObjectItem(events, "onDragChange") : NULL;
        if (on_drag && cJSON_IsString(on_drag) && on_drag->valuestring[0]) {
            const char* name = on_drag->valuestring;
            if (name[0] == '@') {
                name++;
            }
            strncpy(component->on_drag_change_name, name,
                    sizeof(component->on_drag_change_name) - 1);
            component->on_drag_change_name[sizeof(component->on_drag_change_name) - 1] = '\0';
        }
    }

    if (style && layer->set_style) {
        layer->set_style(layer, style);
    } else if (style) {
        cJSON* bg_color = cJSON_GetObjectItem(style, "bgColor");
        if (bg_color && cJSON_IsString(bg_color)) {
            parse_color(bg_color->valuestring, &layer->bg_color);
        }
        if (cJSON_HasObjectItem(style, "borderRadius")) {
            layer->radius = cJSON_GetObjectItem(style, "borderRadius")->valueint;
        }
    }

    return component;
}

void draggable_component_destroy(DraggableComponent* component)
{
    if (component) {
        free(component);
    }
}

void draggable_component_render(Layer* layer)
{
    DraggableComponent* component;
    Rect handle_rect;
    Color handle_color;

    if (!layer || !layer->component) {
        return;
    }

    component = (DraggableComponent*)layer->component;

    if (layer->bg_color.a > 0) {
        if (layer->radius > 0) {
            backend_render_rounded_rect(&layer->rect, layer->bg_color, layer->radius);
        } else {
            backend_render_fill_rect(&layer->rect, layer->bg_color);
        }
    }

    if (component->drag_handle_height <= 0) {
        return;
    }

    handle_rect = layer->rect;
    handle_rect.h = component->drag_handle_height;
    if (handle_rect.h > layer->rect.h) {
        handle_rect.h = layer->rect.h;
    }
    handle_color = layer->bg_color;
    handle_color.r = (uint8_t)(handle_color.r * 85 / 100);
    handle_color.g = (uint8_t)(handle_color.g * 85 / 100);
    handle_color.b = (uint8_t)(handle_color.b * 85 / 100);

    if (layer->radius > 0) {
        int radius = layer->radius;
        if (radius > handle_rect.h / 2) {
            radius = handle_rect.h / 2;
        }
        backend_render_rounded_rect(&handle_rect, handle_color, radius);
        if (handle_rect.h > radius) {
            Rect flat_bottom = {
                handle_rect.x,
                handle_rect.y + handle_rect.h - radius,
                handle_rect.w,
                radius
            };
            backend_render_fill_rect(&flat_bottom, handle_color);
        }
    } else {
        backend_render_fill_rect(&handle_rect, handle_color);
    }
}

int draggable_component_handle_mouse_event(Layer* layer, MouseEvent* event)
{
    DraggableComponent* component;

    if (!layer || !event || !layer->component) {
        return 0;
    }

    component = (DraggableComponent*)layer->component;

    if (event->state == SDL_MOUSEMOTION) {
        if (!component->dragging) {
            return 0;
        }
        layer->rect.x = event->x - component->drag_offset_x;
        layer->rect.y = event->y - component->drag_offset_y;
        draggable_sync_layout_base(layer);
        layout_layer(layer);
        return 1;
    }

    if (event->state == SDL_PRESSED && event->button == SDL_BUTTON_LEFT) {
        if (!draggable_is_drag_handle(component, layer, event->x, event->y)) {
            return 0;
        }
        component->dragging = 1;
        component->drag_offset_x = event->x - layer->rect.x;
        component->drag_offset_y = event->y - layer->rect.y;
        component->drag_start_x = layer->rect.x;
        component->drag_start_y = layer->rect.y;
        draggable_emit_drag_change(layer, component, "start",
                                   component->drag_start_x, component->drag_start_y);
        return 1;
    }

    if (event->state == SDL_RELEASED && event->button == SDL_BUTTON_LEFT) {
        if (!component->dragging) {
            return 0;
        }
        component->dragging = 0;
        draggable_sync_layout_base(layer);
        layout_layer(layer);
        draggable_emit_drag_change(layer, component, "end",
                                   component->drag_start_x, component->drag_start_y);
        return 1;
    }

    return 0;
}
