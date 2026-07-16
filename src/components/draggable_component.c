#include "draggable_component.h"
#include "../backend.h"
#include "../layout.h"
#include "../util.h"
#include <stdlib.h>
#include <string.h>

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
    draggable_ensure_absolute_layout(layer);
    layer->component = component;
    layer->render = draggable_component_render;
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
    backend_render_fill_rect(&handle_rect, handle_color);
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
