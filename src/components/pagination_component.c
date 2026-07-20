#include "pagination_component.h"
#include "../backend.h"
#include "../event.h"
#include "../layer_update.h"
#include "../util.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#define PAGINATION_SPACING 4

typedef struct {
    PaginationZone type;
    Rect rect;
} PaginationZoneRect;

typedef struct {
    PaginationZoneRect zones[3];
    int zone_count;
    char info_text[96];
    Rect info_rect;
    int info_visible;
} PaginationLayout;

static PaginationMode pagination_parse_mode(const char* value) {
    if (!value) return PAGINATION_MODE_SIMPLE;
    if (strcmp(value, "mini") == 0) return PAGINATION_MODE_MINI;
    if (strcmp(value, "full") == 0) return PAGINATION_MODE_FULL;
    return PAGINATION_MODE_SIMPLE;
}

static const char* pagination_mode_name(PaginationMode mode) {
    switch (mode) {
        case PAGINATION_MODE_MINI: return "mini";
        case PAGINATION_MODE_FULL: return "full";
        default: return "simple";
    }
}

int pagination_component_get_page_count(const PaginationComponent* component) {
    if (!component || component->page_size <= 0 || component->total <= 0) return 0;
    return (int)ceil((double)component->total / (double)component->page_size);
}

static void pagination_clamp_page(PaginationComponent* component) {
    int page_count = pagination_component_get_page_count(component);
    if (page_count <= 0) {
        component->page = 1;
        return;
    }
    if (component->page < 1) component->page = 1;
    if (component->page > page_count) component->page = page_count;
}

static int pagination_should_hide(PaginationComponent* component) {
    if (!component || !component->hide_on_single_page) return 0;
    if (component->total <= 0 || component->total > component->page_size) return 0;
    /* simple 模式单页时仍显示区间信息（如 1-50 / 50），仅 mini 整组件隐藏 */
    if (component->mode == PAGINATION_MODE_SIMPLE) return 0;
    return 1;
}

static void pagination_sync_visibility(PaginationComponent* component) {
    if (!component || !component->layer) return;
    layer_set_visible(component->layer, pagination_should_hide(component) ? IN_VISIBLE : VISIBLE);
}

static void pagination_build_info_text(PaginationComponent* component, char* buf, size_t buf_size) {
    if (!component || !buf || buf_size == 0) return;

    int page_count = pagination_component_get_page_count(component);
    if (component->total <= 0) {
        snprintf(buf, buf_size, "0 / 0");
        return;
    }

    int offset = (component->page - 1) * component->page_size;
    int start = offset + 1;
    int end = offset + component->page_size;
    if (end > component->total) end = component->total;

    if (component->mode == PAGINATION_MODE_MINI) {
        if (component->show_page_count) {
            snprintf(buf, buf_size, "%d/%d", component->page, page_count);
        } else {
            snprintf(buf, buf_size, "%d", component->page);
        }
        return;
    }

    if (component->show_total) {
        if (component->total <= component->page_size) {
            snprintf(buf, buf_size, "%d", component->total);
        } else {
            snprintf(buf, buf_size, "%d-%d / %d", start, end, component->total);
        }
    } else if (page_count > 0) {
        snprintf(buf, buf_size, "%d / %d", component->page, page_count);
    } else {
        snprintf(buf, buf_size, "0 / 0");
    }
}

static void pagination_layout(PaginationComponent* component, PaginationLayout* layout) {
    memset(layout, 0, sizeof(*layout));
    if (!component || !component->layer) return;

    Layer* layer = component->layer;
    int item = component->item_size > 0 ? component->item_size : 24;
    if (component->mode == PAGINATION_MODE_MINI && item > 22) item = 22;
    int spacing = PAGINATION_SPACING;
    int x = layer->rect.x;
    int y = layer->rect.y;
    int h = layer->rect.h > 0 ? layer->rect.h : item;
    int cy = y + (h - item) / 2;

    pagination_build_info_text(component, layout->info_text, sizeof(layout->info_text));

    if (component->mode == PAGINATION_MODE_MINI) {
        int total_w = item * 2 + spacing * 2;
        int label_w = 0;
        Texture* info_tex = render_text(layer, layout->info_text, component->text_color);
        if (info_tex) {
            int tw, th;
            backend_query_texture(info_tex, NULL, NULL, &tw, &th);
            label_w = tw / (int)yui_density + spacing * 2;
            backend_render_text_destroy(info_tex);
        }
        total_w += label_w;
        x = layer->rect.x + (layer->rect.w - total_w) / 2;
        if (x < layer->rect.x) x = layer->rect.x;

        layout->zones[layout->zone_count++] = (PaginationZoneRect){
            PAGINATION_ZONE_PREV, {x, cy, item, item}
        };
        x += item + spacing;

        layout->info_visible = 1;
        layout->info_rect = (Rect){x, y, label_w > 0 ? label_w : item, h};
        x += layout->info_rect.w + spacing;

        layout->zones[layout->zone_count++] = (PaginationZoneRect){
            PAGINATION_ZONE_NEXT, {x, cy, item, item}
        };
        return;
    }

    int right = layer->rect.x + layer->rect.w;
    Rect next_rect = {right - item, cy, item, item};
    Rect prev_rect = {next_rect.x - spacing - item, cy, item, item};

    layout->zones[layout->zone_count++] = (PaginationZoneRect){PAGINATION_ZONE_NEXT, next_rect};
    layout->zones[layout->zone_count++] = (PaginationZoneRect){PAGINATION_ZONE_PREV, prev_rect};

    layout->info_visible = 1;
    layout->info_rect = (Rect){
        layer->rect.x,
        y,
        prev_rect.x - layer->rect.x - spacing,
        h
    };
    if (layout->info_rect.w < 0) layout->info_rect.w = 0;
}

static PaginationZone pagination_hit_test(PaginationComponent* component, int x, int y) {
    PaginationLayout layout;
    pagination_layout(component, &layout);
    for (int i = 0; i < layout.zone_count; i++) {
        if (is_point_in_rect(x, y, layout.zones[i].rect)) {
            return layout.zones[i].type;
        }
    }
    return PAGINATION_ZONE_NONE;
}

static int pagination_zone_enabled(PaginationComponent* component, PaginationZone zone) {
    int page_count = pagination_component_get_page_count(component);
    if (zone == PAGINATION_ZONE_PREV) {
        return component->page > 1 && page_count > 0;
    }
    if (zone == PAGINATION_ZONE_NEXT) {
        return page_count > 0 && component->page < page_count;
    }
    return 0;
}

static void pagination_dispatch_change(PaginationComponent* component) {
    if (!component || component->on_change_name[0] == '\0') return;

    Layer* layer = component->layer;
    if (!layer) return;

    int page_count = pagination_component_get_page_count(component);
    int offset = (component->page - 1) * component->page_size;
    int start = component->total > 0 ? offset + 1 : 0;
    int end = component->total > 0 ? offset + component->page_size : 0;
    if (end > component->total) end = component->total;

    cJSON* payload = cJSON_CreateObject();
    if (!payload) return;

    cJSON_AddNumberToObject(payload, "page", component->page);
    cJSON_AddNumberToObject(payload, "pageSize", component->page_size);
    cJSON_AddNumberToObject(payload, "total", component->total);
    cJSON_AddNumberToObject(payload, "pageCount", page_count);
    cJSON_AddNumberToObject(payload, "offset", offset);
    cJSON_AddNumberToObject(payload, "limit", component->page_size);
    cJSON_AddNumberToObject(payload, "start", start);
    cJSON_AddNumberToObject(payload, "end", end);

    char* json = cJSON_PrintUnformatted(payload);
    cJSON_Delete(payload);
    if (!json) return;

    layer_set_text(layer, json);
    free(json);

    pagination_component_trigger_on_change(component);
}

void pagination_component_trigger_on_change(PaginationComponent* component) {
    if (!component || component->on_change_name[0] == '\0') return;

    if (component->on_change == NULL) {
        const char* event_name = component->on_change_name;
        component->on_change = find_event_by_name(event_name);
    }

    if (component->on_change) {
        component->on_change(component->layer);
    } else {
        printf("pagination_component_trigger_on_change: event not found '%s'\n",
               component->on_change_name);
        print_registered_events();
    }
}

int pagination_component_register_event(Layer* layer, const char* event_name,
                                        const char* event_func_name, EventHandler event_handler) {
    if (!layer || !layer->component || !event_handler) return -1;
    if (strcmp(event_name, "change") != 0 && strcmp(event_name, "onChange") != 0) return -1;

    PaginationComponent* component = (PaginationComponent*)layer->component;
    component->on_change = event_handler;
    if (event_func_name && event_func_name[0] != '\0') {
        const char* name = event_func_name;
        if (name[0] == '@') name++;
        strncpy(component->on_change_name, name, sizeof(component->on_change_name) - 1);
        component->on_change_name[sizeof(component->on_change_name) - 1] = '\0';
    }
    return 0;
}

static void pagination_change_page(PaginationComponent* component, int delta) {
    if (!component) return;
    component->page += delta;
    pagination_clamp_page(component);
    pagination_sync_visibility(component);
    mark_layer_dirty(component->layer, DIRTY_TEXT | DIRTY_LAYOUT);
    pagination_dispatch_change(component);
}

static void pagination_draw_button(PaginationComponent* component, const Rect* rect,
                                   const char* label, PaginationZone zone) {
    if (!component || !rect) return;

    int enabled = pagination_zone_enabled(component, zone);
    Color bg = component->button_bg_color;
    Color fg = enabled ? component->text_color : component->disabled_color;

    if (zone == component->pressed_zone && enabled) {
        bg.r = (unsigned char)(bg.r * 0.85f);
        bg.g = (unsigned char)(bg.g * 0.85f);
        bg.b = (unsigned char)(bg.b * 0.85f);
    } else if (zone == component->hovered_zone && enabled) {
        bg = component->button_hover_color;
    }

    if (component->border_radius > 0) {
        backend_render_rounded_rect(rect, bg, component->border_radius);
    } else {
        backend_render_fill_rect(rect, bg);
    }

    Layer* layer = component->layer;
    Texture* tex = render_text(layer, label, fg);
    if (!tex) return;

    int tw, th;
    backend_query_texture(tex, NULL, NULL, &tw, &th);
    int draw_w = tw / (int)yui_density;
    int draw_h = th / (int)yui_density;
    Rect dst = {
        rect->x + (rect->w - draw_w) / 2,
        rect->y + (rect->h - draw_h) / 2,
        draw_w,
        draw_h
    };
    backend_render_text_copy(tex, NULL, &dst);
    backend_render_text_destroy(tex);
}

static void pagination_draw_info(PaginationComponent* component, const Rect* rect, const char* text) {
    if (!component || !rect || !text || rect->w <= 0) return;

    Layer* layer = component->layer;
    Texture* tex = render_text(layer, text, component->text_color);
    if (!tex) return;

    int tw, th;
    backend_query_texture(tex, NULL, NULL, &tw, &th);
    int draw_w = tw / (int)yui_density;
    int draw_h = th / (int)yui_density;

    int draw_x = rect->x;
    if (component->mode == PAGINATION_MODE_MINI) {
        draw_x = rect->x + (rect->w - draw_w) / 2;
    }

    Rect dst = {
        draw_x,
        rect->y + (rect->h - draw_h) / 2,
        draw_w,
        draw_h
    };
    backend_render_text_copy(tex, NULL, &dst);
    backend_render_text_destroy(tex);
}

void pagination_component_set_page(PaginationComponent* component, int page, int dispatch_change) {
    if (!component) return;
    component->page = page > 0 ? page : 1;
    pagination_clamp_page(component);
    pagination_sync_visibility(component);
    mark_layer_dirty(component->layer, DIRTY_TEXT | DIRTY_LAYOUT);
    if (dispatch_change) pagination_dispatch_change(component);
}

void pagination_component_set_page_size(PaginationComponent* component, int page_size, int dispatch_change) {
    if (!component || page_size <= 0) return;
    component->page_size = page_size;
    pagination_clamp_page(component);
    pagination_sync_visibility(component);
    mark_layer_dirty(component->layer, DIRTY_TEXT | DIRTY_LAYOUT);
    if (dispatch_change) pagination_dispatch_change(component);
}

void pagination_component_set_total(PaginationComponent* component, int total, int dispatch_change) {
    if (!component) return;
    component->total = total >= 0 ? total : 0;
    pagination_clamp_page(component);
    pagination_sync_visibility(component);
    mark_layer_dirty(component->layer, DIRTY_TEXT | DIRTY_LAYOUT);
    if (dispatch_change) pagination_dispatch_change(component);
}

static void pagination_apply_style(PaginationComponent* component, cJSON* style) {
    if (!component || !style) return;

    if (cJSON_HasObjectItem(style, "color")) {
        parse_color(cJSON_GetObjectItem(style, "color")->valuestring, &component->text_color);
    }
    if (cJSON_HasObjectItem(style, "activeColor")) {
        parse_color(cJSON_GetObjectItem(style, "activeColor")->valuestring, &component->active_color);
    }
    if (cJSON_HasObjectItem(style, "disabledColor")) {
        parse_color(cJSON_GetObjectItem(style, "disabledColor")->valuestring, &component->disabled_color);
    }
    if (cJSON_HasObjectItem(style, "buttonBgColor")) {
        parse_color(cJSON_GetObjectItem(style, "buttonBgColor")->valuestring, &component->button_bg_color);
    }
    if (cJSON_HasObjectItem(style, "buttonHoverColor")) {
        parse_color(cJSON_GetObjectItem(style, "buttonHoverColor")->valuestring, &component->button_hover_color);
    }
    if (cJSON_HasObjectItem(style, "fontSize")) {
        component->font_size = cJSON_GetObjectItem(style, "fontSize")->valueint;
    }
    if (cJSON_HasObjectItem(style, "itemSize")) {
        component->item_size = cJSON_GetObjectItem(style, "itemSize")->valueint;
    }
    if (cJSON_HasObjectItem(style, "borderRadius")) {
        component->border_radius = cJSON_GetObjectItem(style, "borderRadius")->valueint;
    }
}

static void pagination_apply_json_props(PaginationComponent* component, cJSON* json_obj) {
    if (!component || !json_obj) return;

    cJSON* mode = cJSON_GetObjectItem(json_obj, "mode");
    if (mode && cJSON_IsString(mode)) {
        component->mode = pagination_parse_mode(mode->valuestring);
    }

    cJSON* page = cJSON_GetObjectItem(json_obj, "page");
    if (page && cJSON_IsNumber(page)) {
        component->page = page->valueint > 0 ? page->valueint : 1;
    }

    cJSON* page_size = cJSON_GetObjectItem(json_obj, "pageSize");
    if (page_size && cJSON_IsNumber(page_size)) {
        component->page_size = page_size->valueint > 0 ? page_size->valueint : 20;
    }

    cJSON* total = cJSON_GetObjectItem(json_obj, "total");
    if (total && cJSON_IsNumber(total)) {
        component->total = total->valueint >= 0 ? total->valueint : 0;
    }

    cJSON* hide = cJSON_GetObjectItem(json_obj, "hideOnSinglePage");
    if (hide) component->hide_on_single_page = cJSON_IsTrue(hide);

    cJSON* show_total = cJSON_GetObjectItem(json_obj, "showTotal");
    if (show_total) component->show_total = cJSON_IsTrue(show_total);
    else if (component->mode == PAGINATION_MODE_SIMPLE) component->show_total = 1;

    cJSON* show_page_count = cJSON_GetObjectItem(json_obj, "showPageCount");
    if (show_page_count) component->show_page_count = cJSON_IsTrue(show_page_count);

    cJSON* events = cJSON_GetObjectItem(json_obj, "events");
    if (events) {
        cJSON* on_change = cJSON_GetObjectItem(events, "onChange");
        if (on_change && cJSON_IsString(on_change) && on_change->valuestring) {
            const char* name = on_change->valuestring;
            if (name[0] == '@') name++;
            strncpy(component->on_change_name, name, sizeof(component->on_change_name) - 1);
            component->on_change_name[sizeof(component->on_change_name) - 1] = '\0';
        }
    }

    cJSON* style = cJSON_GetObjectItem(json_obj, "style");
    if (style) pagination_apply_style(component, style);

    pagination_clamp_page(component);
    pagination_sync_visibility(component);
}

int pagination_component_set_property_from_json(Layer* layer, const char* key, cJSON* value, int is_creating) {
    (void)is_creating;
    if (!layer || !key || !value || !layer->component) return 0;

    PaginationComponent* component = (PaginationComponent*)layer->component;

    if (strcmp(key, "page") == 0 && cJSON_IsNumber(value)) {
        pagination_component_set_page(component, value->valueint, 0);
        return 1;
    }
    if (strcmp(key, "pageSize") == 0 && cJSON_IsNumber(value)) {
        pagination_component_set_page_size(component, value->valueint, 0);
        return 1;
    }
    if (strcmp(key, "total") == 0 && cJSON_IsNumber(value)) {
        pagination_component_set_total(component, value->valueint, 0);
        return 1;
    }
    if (strcmp(key, "mode") == 0 && cJSON_IsString(value)) {
        component->mode = pagination_parse_mode(value->valuestring);
        mark_layer_dirty(layer, DIRTY_TEXT | DIRTY_LAYOUT);
        pagination_sync_visibility(component);
        return 1;
    }
    if (strcmp(key, "hideOnSinglePage") == 0) {
        component->hide_on_single_page = cJSON_IsTrue(value);
        pagination_sync_visibility(component);
        return 1;
    }
    if (strcmp(key, "showTotal") == 0) {
        component->show_total = cJSON_IsTrue(value);
        mark_layer_dirty(layer, DIRTY_TEXT);
        return 1;
    }
    if (strcmp(key, "showPageCount") == 0) {
        component->show_page_count = cJSON_IsTrue(value);
        mark_layer_dirty(layer, DIRTY_TEXT);
        return 1;
    }

    return 0;
}

static void pagination_component_apply_theme_style(Layer* layer, cJSON* style) {
    if (!layer || !style || !layer->component) {
        return;
    }
    pagination_apply_style((PaginationComponent*)layer->component, style);
    mark_layer_dirty(layer, DIRTY_COLOR | DIRTY_TEXT | DIRTY_LAYOUT);
}

cJSON* pagination_component_get_property(Layer* layer, const char* property_name) {
    if (!layer || !property_name || !layer->component) return NULL;

    PaginationComponent* component = (PaginationComponent*)layer->component;

    if (strcmp(property_name, "page") == 0) {
        return cJSON_CreateNumber(component->page);
    }
    if (strcmp(property_name, "pageSize") == 0) {
        return cJSON_CreateNumber(component->page_size);
    }
    if (strcmp(property_name, "total") == 0) {
        return cJSON_CreateNumber(component->total);
    }
    if (strcmp(property_name, "pageCount") == 0) {
        return cJSON_CreateNumber(pagination_component_get_page_count(component));
    }
    if (strcmp(property_name, "mode") == 0) {
        return cJSON_CreateString(pagination_mode_name(component->mode));
    }

    return NULL;
}

static void pagination_layer_destroy(Layer* layer) {
    if (!layer || !layer->component) return;
    pagination_component_destroy((PaginationComponent*)layer->component);
    layer->component = NULL;
}

PaginationComponent* pagination_component_create(Layer* layer) {
    PaginationComponent* component = (PaginationComponent*)calloc(1, sizeof(PaginationComponent));
    if (!component) return NULL;

    component->layer = layer;
    component->mode = PAGINATION_MODE_SIMPLE;
    component->page = 1;
    component->page_size = 20;
    component->total = 0;
    component->show_total = 1;
    component->text_color = (Color){166, 173, 200, 255};
    component->active_color = (Color){205, 214, 244, 255};
    component->disabled_color = (Color){69, 71, 90, 255};
    component->button_bg_color = (Color){49, 50, 68, 255};
    component->button_hover_color = (Color){69, 71, 90, 255};
    component->font_size = 11;
    component->item_size = 24;
    component->border_radius = 4;

    layer->component = component;
    layer->render = pagination_component_render;
    layer->handle_pointer_event = pagination_component_handle_pointer_event;
    layer->get_property = pagination_component_get_property;
    layer->set_property = pagination_component_set_property_from_json;
    layer->set_style = pagination_component_apply_theme_style;
    layer->register_event = pagination_component_register_event;
    layer->on_destroy = pagination_layer_destroy;

    return component;
}

PaginationComponent* pagination_component_create_from_json(Layer* layer, cJSON* json_obj) {
    PaginationComponent* component = pagination_component_create(layer);
    if (!component) return NULL;

    if (json_obj) {
        pagination_apply_json_props(component, json_obj);
    }

    if (component->font_size > 0) {
        if (!layer->font) {
            layer->font = (Font*)calloc(1, sizeof(Font));
        }
        if (layer->font) {
            layer->font->size = component->font_size;
        }
    }

    return component;
}

void pagination_component_destroy(PaginationComponent* component) {
    if (component) free(component);
}

void pagination_component_render(Layer* layer) {
    if (!layer || !layer->component || layer->visible != VISIBLE) return;

    PaginationComponent* component = (PaginationComponent*)layer->component;
    PaginationLayout layout;
    pagination_layout(component, &layout);

    if (layout.info_visible && layout.info_text[0] != '\0' &&
        component->mode != PAGINATION_MODE_MINI) {
        pagination_draw_info(component, &layout.info_rect, layout.info_text);
    }

    for (int i = 0; i < layout.zone_count; i++) {
        const char* label = layout.zones[i].type == PAGINATION_ZONE_PREV ? "<" : ">";
        pagination_draw_button(component, &layout.zones[i].rect, label, layout.zones[i].type);
    }

    if (component->mode == PAGINATION_MODE_MINI && layout.info_visible && layout.info_text[0]) {
        pagination_draw_info(component, &layout.info_rect, layout.info_text);
    }
}

int pagination_component_handle_pointer_event(Layer* layer, PointerEvent* event) {
    if (!layer || !layer->component || layer->visible != VISIBLE) return 0;
    if (HAS_STATE(layer, LAYER_STATE_DISABLED)) return 0;

    PaginationComponent* component = (PaginationComponent*)layer->component;
    PaginationZone zone = pagination_hit_test(component, event->x, event->y);

    if (event->phase == POINTER_MOVE) {
        if (zone != component->hovered_zone) {
            component->hovered_zone = zone;
            mark_layer_dirty(layer, DIRTY_TEXT);
        }
        return zone != PAGINATION_ZONE_NONE;
    }

    if (event->phase == POINTER_DOWN) {
        if (zone != PAGINATION_ZONE_NONE && pagination_zone_enabled(component, zone)) {
            component->pressed_zone = zone;
            mark_layer_dirty(layer, DIRTY_TEXT);
            return 1;
        }
        return 0;
    }

    if (event->phase == POINTER_UP) {
        PaginationZone pressed = component->pressed_zone;
        component->pressed_zone = PAGINATION_ZONE_NONE;
        mark_layer_dirty(layer, DIRTY_TEXT);

        if (pressed != PAGINATION_ZONE_NONE && zone == pressed &&
            pagination_zone_enabled(component, pressed)) {
            if (pressed == PAGINATION_ZONE_PREV) {
                pagination_change_page(component, -1);
            } else if (pressed == PAGINATION_ZONE_NEXT) {
                pagination_change_page(component, 1);
            }
            return 1;
        }
    }

    return 0;
}
