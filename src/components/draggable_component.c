#include "draggable_component.h"
#include "connector_component.h"
#include "../backend.h"
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

static int draggable_dot_handle_mouse_event(Layer* layer, MouseEvent* event)
{
    (void)layer;
    (void)event;
    return 0;
}

static int draggable_append_child(Layer* parent, Layer* child)
{
    Layer** children;

    if (!parent || !child) {
        return -1;
    }

    child->parent = parent;
    children = (Layer**)realloc(parent->children,
                              (size_t)(parent->child_count + 1) * sizeof(Layer*));
    if (!children) {
        return -1;
    }

    parent->children = children;
    parent->children[parent->child_count++] = child;
    return 0;
}

static Layer* draggable_create_dot_layer(DraggableComponent* component, ConnectorAnchor anchor)
{
    Layer* parent = component->layer;
    int radius = component->dot_size;
    int size = radius * 2;
    Layer* dot;
    static const char suffixes[] = "CTBLR";

    dot = layer_create(parent, 0, 0, size, size);
    if (!dot) {
        return NULL;
    }

    dot->type = VIEW;
    snprintf(dot->id, sizeof(dot->id), "%s_dot%c", parent->id, suffixes[anchor]);
    dot->bg_color = component->dot_color;
    dot->radius = radius;
    dot->focusable = 0;
    dot->handle_mouse_event = draggable_dot_handle_mouse_event;
    dot->fixed_width = size;
    dot->fixed_height = size;
    return dot;
}

static void draggable_anchor_offset(DraggableComponent* component, ConnectorAnchor anchor,
                                    int* rel_x, int* rel_y)
{
    Layer* parent = component->layer;
    int radius = component->dot_size;
    int center_x = parent->rect.w / 2;
    int center_y = parent->rect.h / 2;

    switch (anchor) {
        case CONNECTOR_ANCHOR_TOP:
            *rel_x = center_x - radius;
            *rel_y = -radius;
            break;
        case CONNECTOR_ANCHOR_BOTTOM:
            *rel_x = center_x - radius;
            *rel_y = parent->rect.h - radius;
            break;
        case CONNECTOR_ANCHOR_LEFT:
            *rel_x = -radius;
            *rel_y = center_y - radius;
            break;
        case CONNECTOR_ANCHOR_RIGHT:
            *rel_x = parent->rect.w - radius;
            *rel_y = center_y - radius;
            break;
        case CONNECTOR_ANCHOR_CENTER:
        default:
            *rel_x = center_x - radius;
            *rel_y = center_y - radius;
            break;
    }
}

static void draggable_update_dot_layer(DraggableComponent* component, Layer* dot,
                                       int rel_x, int rel_y)
{
    Layer* parent = component->layer;
    int size = component->dot_size * 2;

    if (!dot || !parent) {
        return;
    }

    dot->layout_base_rect.x = rel_x;
    dot->layout_base_rect.y = rel_y;
    dot->layout_base_rect.w = size;
    dot->layout_base_rect.h = size;
    dot->layout_base_valid = 1;
    dot->rect.x = parent->rect.x + rel_x;
    dot->rect.y = parent->rect.y + rel_y;
    dot->rect.w = size;
    dot->rect.h = size;
    dot->bg_color = component->dot_color;
    dot->radius = component->dot_size;
}

static void draggable_sync_dots(DraggableComponent* component)
{
    Layer* parent;
    unsigned int anchor_mask = 0;
    int anchor;

    if (!component || !component->layer) {
        return;
    }

    parent = component->layer;

    if (component->show_dots && parent->id[0] && g_ui_root) {
        connector_collect_anchors_for_layer(g_ui_root, parent->id, &anchor_mask);
    }

    if (!anchor_mask || component->dot_size <= 0) {
        for (anchor = CONNECTOR_ANCHOR_CENTER; anchor <= CONNECTOR_ANCHOR_RIGHT; anchor++) {
            if (component->anchor_dots[anchor]) {
                layer_hide(component->anchor_dots[anchor]);
            }
        }
        return;
    }

    for (anchor = CONNECTOR_ANCHOR_CENTER; anchor <= CONNECTOR_ANCHOR_RIGHT; anchor++) {
        int rel_x;
        int rel_y;
        Layer* dot = component->anchor_dots[anchor];

        if (!(anchor_mask & (1u << anchor))) {
            if (dot) {
                layer_hide(dot);
            }
            continue;
        }

        if (!dot) {
            dot = draggable_create_dot_layer(component, (ConnectorAnchor)anchor);
            if (!dot || draggable_append_child(parent, dot) != 0) {
                return;
            }
            component->anchor_dots[anchor] = dot;
        } else {
            layer_set_visible(dot, VISIBLE);
        }

        draggable_anchor_offset(component, (ConnectorAnchor)anchor, &rel_x, &rel_y);
        draggable_update_dot_layer(component, dot, rel_x, rel_y);
    }
}

static void draggable_component_layout(Layer* layer)
{
    DraggableComponent* component;

    if (!layer || !layer->component) {
        return;
    }

    component = (DraggableComponent*)layer->component;
    draggable_sync_dots(component);
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

int draggable_component_get_dot_style(Layer* layer, int* show_dots, int* dot_size,
                                      Color* dot_color)
{
    DraggableComponent* component;

    if (!layer || layer->type != DRAGGABLE || !layer->component) {
        return 0;
    }

    component = (DraggableComponent*)layer->component;
    if (show_dots) {
        *show_dots = component->show_dots;
    }
    if (dot_size) {
        *dot_size = component->dot_size;
    }
    if (dot_color) {
        *dot_color = component->dot_color;
    }
    return 1;
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
        return 1;
    }

    if (event->state == SDL_RELEASED && event->button == SDL_BUTTON_LEFT) {
        if (!component->dragging) {
            return 0;
        }
        component->dragging = 0;
        draggable_sync_layout_base(layer);
        layout_layer(layer);
        return 1;
    }

    return 0;
}
