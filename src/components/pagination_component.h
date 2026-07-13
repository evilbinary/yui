#ifndef YUI_PAGINATION_COMPONENT_H
#define YUI_PAGINATION_COMPONENT_H

#include "../ytype.h"
#include "../render.h"
#include <SDL.h>

typedef enum {
    PAGINATION_MODE_MINI = 0,
    PAGINATION_MODE_SIMPLE,
    PAGINATION_MODE_FULL
} PaginationMode;

typedef enum {
    PAGINATION_ZONE_NONE = 0,
    PAGINATION_ZONE_PREV,
    PAGINATION_ZONE_NEXT
} PaginationZone;

typedef struct PaginationComponent {
    Layer* layer;
    PaginationMode mode;
    int page;
    int page_size;
    int total;
    int hide_on_single_page;
    int show_total;
    int show_page_count;

    PaginationZone hovered_zone;
    PaginationZone pressed_zone;

    char on_change_name[MAX_PATH];

    Color text_color;
    Color active_color;
    Color disabled_color;
    Color button_bg_color;
    Color button_hover_color;
    int font_size;
    int item_size;
    int border_radius;
} PaginationComponent;

PaginationComponent* pagination_component_create(Layer* layer);
PaginationComponent* pagination_component_create_from_json(Layer* layer, cJSON* json_obj);
void pagination_component_destroy(PaginationComponent* component);

int pagination_component_set_property_from_json(Layer* layer, const char* key, cJSON* value, int is_creating);
cJSON* pagination_component_get_property(Layer* layer, const char* property_name);

void pagination_component_render(Layer* layer);
int pagination_component_handle_mouse_event(Layer* layer, MouseEvent* event);

int pagination_component_get_page_count(const PaginationComponent* component);
void pagination_component_set_page(PaginationComponent* component, int page, int dispatch_change);
void pagination_component_set_page_size(PaginationComponent* component, int page_size, int dispatch_change);
void pagination_component_set_total(PaginationComponent* component, int total, int dispatch_change);

#endif
