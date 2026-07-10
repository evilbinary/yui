#include "sash_component.h"
#include "../render.h"
#include "../backend.h"
#include "../layout.h"
#include "../layer_update.h"
#include <stdlib.h>
#include <string.h>

extern struct Layer* g_layer_root;

SashComponent* sash_component_create(Layer* layer) {
    if (!layer) return NULL;

    SashComponent* comp = malloc(sizeof(SashComponent));
    if (!comp) return NULL;

    memset(comp, 0, sizeof(SashComponent));
    comp->layer = layer;
    comp->min_size = 60;
    comp->target_id[0] = '\0';

    layer->component = comp;
    layer->render = sash_component_render;
    layer->handle_mouse_event = sash_component_handle_mouse_event;

    return comp;
}

SashComponent* sash_component_create_from_json(Layer* layer, cJSON* json_obj) {
    SashComponent* comp = sash_component_create(layer);
    if (!comp) return NULL;

    cJSON* target_json = cJSON_GetObjectItem(json_obj, "target");
    if (target_json && cJSON_IsString(target_json)) {
        strncpy(comp->target_id, target_json->valuestring, MAX_TEXT - 1);
        comp->target_id[MAX_TEXT - 1] = '\0';
    }

    cJSON* min_json = cJSON_GetObjectItem(json_obj, "minSize");
    if (min_json && cJSON_IsNumber(min_json)) {
        comp->min_size = min_json->valueint;
    }

    return comp;
}

void sash_component_destroy(SashComponent* comp) {
    if (comp) free(comp);
}

static void ensure_target(SashComponent* comp) {
    if (!comp->target && comp->target_id[0] && g_layer_root) {
        comp->target = find_layer_by_id(g_layer_root, comp->target_id);
    }
}

void sash_component_render(Layer* layer) {
    if (!layer) return;

    SashComponent* comp = (SashComponent*)layer->component;
    Color bg = comp && comp->hover ? (Color){100, 100, 115, 255} : (Color){49, 50, 68, 255};

    backend_render_fill_rect(&layer->rect, bg);

    if (!comp) return;

    // Top border
    Rect top_line = {layer->rect.x, layer->rect.y, layer->rect.w, 1};
    Color border_color = {69, 71, 90, 255};
    backend_render_fill_rect(&top_line, border_color);

    // Gripper dots
    int dot_r = 2;
    int dot_spacing = 10;
    int center_x = layer->rect.x + layer->rect.w / 2;
    int center_y = layer->rect.y + layer->rect.h / 2;
    Color dot_color = comp->hover ? (Color){180, 180, 190, 255} : (Color){100, 100, 115, 255};

    for (int i = -1; i <= 1; i++) {
        Rect dot = {center_x + i * dot_spacing - dot_r, center_y - dot_r, dot_r * 2, dot_r * 2};
        backend_render_fill_rect(&dot, dot_color);
    }

    // Hover accent
    if (comp->hover) {
        Rect accent = {layer->rect.x, layer->rect.y + 1, layer->rect.w, 1};
        Color accent_color = {137, 180, 250, 180};
        backend_render_fill_rect(&accent, accent_color);
    }
}

int sash_component_handle_mouse_event(Layer* layer, MouseEvent* event) {
    if (!layer || !event) return 0;
    SashComponent* comp = (SashComponent*)layer->component;
    if (!comp) return 0;

    int inside = (event->x >= layer->rect.x && event->x < layer->rect.x + layer->rect.w &&
                  event->y >= layer->rect.y && event->y < layer->rect.y + layer->rect.h);

    if (event->state == SDL_MOUSEMOTION) {
        if (inside && !comp->hover) {
            comp->hover = 1;
            mark_layer_dirty(layer, DIRTY_COLOR);
        } else if (!inside && comp->hover && !comp->dragging) {
            comp->hover = 0;
            mark_layer_dirty(layer, DIRTY_COLOR);
        }

        if (comp->dragging) {
            ensure_target(comp);
            if (!comp->target || !comp->target->parent) return 0;

            int delta_y = event->y - comp->drag_start_y;
            int new_h = comp->initial_height + delta_y;
            if (new_h < comp->min_size) new_h = comp->min_size;

            Layer* parent = comp->target->parent;

            // Find sibling below the sash
            Layer* below = NULL;
            for (int i = 0; i < parent->child_count - 1; i++) {
                if (parent->children[i] == layer) {
                    below = parent->children[i + 1];
                    break;
                }
            }

            if (below) {
                int total = comp->target->rect.h + below->rect.h;
                if (new_h > total - 20) new_h = total - 20;

                comp->target->rect.h = new_h;
                comp->target->fixed_height = new_h;
                below->rect.h = total - new_h;
                below->fixed_height = total - new_h;

                mark_layer_dirty(parent, DIRTY_LAYOUT);
                layout_layer(parent);
            }
        }
        return 0;
    }

    if (event->state == SDL_PRESSED && event->button == SDL_BUTTON_LEFT && inside) {
        ensure_target(comp);
        comp->dragging = 1;
        comp->drag_start_y = event->y;
        if (comp->target) {
            comp->initial_height = comp->target->rect.h;
        }
        return 0;
    }

    if (event->state == SDL_RELEASED && event->button == SDL_BUTTON_LEFT) {
        comp->dragging = 0;
        return 0;
    }
    return 0;
}
