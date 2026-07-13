#include "sash_component.h"
#include "../render.h"
#include "../backend.h"
#include "../layout.h"
#include "../layer_update.h"
#include "../layer.h"
#include "../util.h"
#include "../event.h"
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>

extern Layer* g_ui_root;

static void sash_component_set_style(Layer* layer, cJSON* style);

int sash_component_register_event(Layer* layer, const char* event_name,
                                const char* event_func_name, EventHandler event_handler);

SashComponent* sash_component_create(Layer* layer) {
    if (!layer) return NULL;

    SashComponent* comp = malloc(sizeof(SashComponent));
    if (!comp) return NULL;

    memset(comp, 0, sizeof(SashComponent));
    comp->layer = layer;
    comp->min_size = 60;
    comp->target_id[0] = '\0';
    comp->on_change_name[0] = '\0';
    comp->bg_color = (Color){49, 50, 68, 255};
    comp->hover_bg_color = (Color){100, 100, 115, 255};
    comp->border_color = (Color){69, 71, 90, 255};
    comp->dot_color = (Color){100, 100, 115, 255};
    comp->hover_dot_color = (Color){180, 180, 190, 255};
    comp->accent_color = (Color){137, 180, 250, 180};

    layer->component = comp;
    layer->render = sash_component_render;
    layer->handle_mouse_event = sash_component_handle_mouse_event;
    layer->register_event = sash_component_register_event;
    layer->set_style = sash_component_set_style;

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

    cJSON* orientation_json = cJSON_GetObjectItem(json_obj, "orientation");
    if (orientation_json && cJSON_IsString(orientation_json)) {
        if (strcmp(orientation_json->valuestring, "horizontal") == 0) {
            comp->horizontal = 1;
        }
    }

    cJSON* events = cJSON_GetObjectItem(json_obj, "events");
    if (events) {
        cJSON* on_change = cJSON_GetObjectItem(events, "onChange");
        if (on_change && cJSON_IsString(on_change) && on_change->valuestring) {
            const char* name = on_change->valuestring;
            if (name[0] == '@') name++;
            strncpy(comp->on_change_name, name, sizeof(comp->on_change_name) - 1);
            comp->on_change_name[sizeof(comp->on_change_name) - 1] = '\0';
        }
    }

    cJSON* style = cJSON_GetObjectItem(json_obj, "style");
    if (style && layer->set_style) {
        layer->set_style(layer, style);
    }

    return comp;
}

static void sash_component_set_style(Layer* layer, cJSON* style) {
    if (!layer || !style || !layer->component) return;

    SashComponent* comp = (SashComponent*)layer->component;

    cJSON* item = style->child;
    while (item) {
        if (!item->string || !cJSON_IsString(item)) {
            item = item->next;
            continue;
        }
        Color color;
        if (strcmp(item->string, "bgColor") == 0) {
            parse_color(item->valuestring, &comp->bg_color);
            layer->bg_color = comp->bg_color;
        } else if (strcmp(item->string, "hoverBgColor") == 0) {
            parse_color(item->valuestring, &comp->hover_bg_color);
        } else if (strcmp(item->string, "borderColor") == 0) {
            parse_color(item->valuestring, &comp->border_color);
        } else if (strcmp(item->string, "dotColor") == 0) {
            parse_color(item->valuestring, &comp->dot_color);
        } else if (strcmp(item->string, "hoverDotColor") == 0) {
            parse_color(item->valuestring, &comp->hover_dot_color);
        } else if (strcmp(item->string, "accentColor") == 0) {
            parse_color(item->valuestring, &comp->accent_color);
        }
        item = item->next;
    }

    mark_layer_dirty(layer, DIRTY_COLOR);
}

void sash_component_destroy(SashComponent* comp) {
    if (comp) free(comp);
}

static void ensure_target(SashComponent* comp) {
    if (!comp->target && comp->target_id[0] && g_ui_root) {
        comp->target = find_layer_by_id(g_ui_root, comp->target_id);
    }
}

static Layer* sash_find_sibling(SashComponent* comp) {
    if (!comp->layer || !comp->layer->parent) return NULL;

    Layer* parent = comp->layer->parent;
    for (int i = 0; i < parent->child_count - 1; i++) {
        if (parent->children[i] == comp->layer) {
            return parent->children[i + 1];
        }
    }
    return NULL;
}

static int sash_apply_split(SashComponent* comp, int new_size) {
    ensure_target(comp);
    if (!comp->target || !comp->target->parent) return 0;

    Layer* sibling = sash_find_sibling(comp);
    if (!sibling) return 0;

    if (new_size < comp->min_size) new_size = comp->min_size;

    if (comp->horizontal) {
        int total = comp->target->rect.w + sibling->rect.w;
        if (new_size > total - 20) new_size = total - 20;
        comp->target->rect.w = new_size;
        comp->target->fixed_width = new_size;
        sibling->rect.w = total - new_size;
        sibling->fixed_width = total - new_size;
    } else {
        int total = comp->target->rect.h + sibling->rect.h;
        if (new_size > total - 20) new_size = total - 20;
        comp->target->rect.h = new_size;
        comp->target->fixed_height = new_size;
        sibling->rect.h = total - new_size;
        sibling->fixed_height = total - new_size;
    }

    mark_layer_dirty(comp->target->parent, DIRTY_LAYOUT);
    layout_layer(comp->target->parent);
    return 1;
}

static void sash_update_layout_base(SashComponent* comp) {
    if (comp->target && comp->target->layout_base_valid) {
        if (comp->horizontal) {
            comp->target->layout_base_rect.w = comp->target->rect.w;
            comp->target->layout_base_fixed_w = comp->target->fixed_width;
        } else {
            comp->target->layout_base_rect.h = comp->target->rect.h;
            comp->target->layout_base_fixed_h = comp->target->fixed_height;
        }
    }

    Layer* sibling = sash_find_sibling(comp);
    if (sibling && sibling->layout_base_valid) {
        if (comp->horizontal) {
            sibling->layout_base_rect.w = sibling->rect.w;
            sibling->layout_base_fixed_w = sibling->fixed_width;
        } else {
            sibling->layout_base_rect.h = sibling->rect.h;
            sibling->layout_base_fixed_h = sibling->fixed_height;
        }
    }
}

static void sash_dispatch_change(SashComponent* comp) {
    if (!comp || comp->on_change_name[0] == '\0') return;

    Layer* layer = comp->layer;
    Layer* sibling = sash_find_sibling(comp);
    if (!layer || !comp->target) return;

    cJSON* payload = cJSON_CreateObject();
    if (!payload) return;

    if (comp->horizontal) {
        cJSON_AddNumberToObject(payload, "targetWidth", comp->target->rect.w);
        if (sibling) cJSON_AddNumberToObject(payload, "siblingWidth", sibling->rect.w);
    } else {
        cJSON_AddStringToObject(payload, "target", comp->target_id);
        cJSON_AddNumberToObject(payload, "targetHeight", comp->target->rect.h);
        if (sibling) cJSON_AddNumberToObject(payload, "siblingHeight", sibling->rect.h);
    }

    char* json = cJSON_PrintUnformatted(payload);
    cJSON_Delete(payload);
    if (!json) return;

    layer_set_text(layer, json);
    free(json);

    if (comp->on_change == NULL) {
        comp->on_change = find_event_by_name(comp->on_change_name);
    }
    if (comp->on_change) {
        comp->on_change(layer);
    }
}

int sash_component_register_event(Layer* layer, const char* event_name,
                                const char* event_func_name, EventHandler event_handler) {
    if (!layer || !layer->component || !event_handler) return -1;
    if (strcmp(event_name, "change") != 0 && strcmp(event_name, "onChange") != 0) return -1;

    SashComponent* comp = (SashComponent*)layer->component;
    comp->on_change = event_handler;
    if (event_func_name && event_func_name[0] != '\0') {
        const char* name = event_func_name;
        if (name[0] == '@') name++;
        strncpy(comp->on_change_name, name, sizeof(comp->on_change_name) - 1);
        comp->on_change_name[sizeof(comp->on_change_name) - 1] = '\0';
    }
    return 0;
}

void sash_component_render(Layer* layer) {
    if (!layer) return;

    SashComponent* comp = (SashComponent*)layer->component;
    Color bg = comp && comp->hover ? comp->hover_bg_color : (comp ? comp->bg_color : (Color){49, 50, 68, 255});

    backend_render_fill_rect(&layer->rect, bg);

    if (!comp) return;

    int dot_r = 2;
    int dot_spacing = 10;
    int center_x = layer->rect.x + layer->rect.w / 2;
    int center_y = layer->rect.y + layer->rect.h / 2;
    Color border_color = comp->border_color;
    Color dot_color = comp->hover ? comp->hover_dot_color : comp->dot_color;

    if (comp->horizontal) {
        Rect left_line = {layer->rect.x, layer->rect.y, 1, layer->rect.h};
        backend_render_fill_rect(&left_line, border_color);

        for (int i = -1; i <= 1; i++) {
            Rect dot = {center_x - dot_r, center_y + i * dot_spacing - dot_r, dot_r * 2, dot_r * 2};
            backend_render_fill_rect(&dot, dot_color);
        }

        if (comp->hover) {
            Rect accent = {layer->rect.x + 1, layer->rect.y, 1, layer->rect.h};
            backend_render_fill_rect(&accent, comp->accent_color);
        }
    } else {
        Rect top_line = {layer->rect.x, layer->rect.y, layer->rect.w, 1};
        backend_render_fill_rect(&top_line, border_color);

        for (int i = -1; i <= 1; i++) {
            Rect dot = {center_x + i * dot_spacing - dot_r, center_y - dot_r, dot_r * 2, dot_r * 2};
            backend_render_fill_rect(&dot, dot_color);
        }

        if (comp->hover) {
            Rect accent = {layer->rect.x, layer->rect.y + 1, layer->rect.w, 1};
            backend_render_fill_rect(&accent, comp->accent_color);
        }
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
            int delta = comp->horizontal ? (event->x - comp->drag_start_y) : (event->y - comp->drag_start_y);
            int new_size = comp->initial_height + delta;
            sash_apply_split(comp, new_size);
        }
        return 0;
    }

    if (event->state == SDL_PRESSED && event->button == SDL_BUTTON_LEFT && inside) {
        ensure_target(comp);
        comp->dragging = 1;
        comp->drag_start_y = comp->horizontal ? event->x : event->y;
        if (comp->target) {
            comp->initial_height = comp->horizontal ? comp->target->rect.w : comp->target->rect.h;
        }
        return 0;
    }

    if (event->state == SDL_RELEASED && event->button == SDL_BUTTON_LEFT) {
        if (comp->dragging) {
            sash_update_layout_base(comp);
            sash_dispatch_change(comp);
        }
        comp->dragging = 0;
        return 0;
    }
    return 0;
}
