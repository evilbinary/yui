#ifndef YUI_LIST_COMPONENT_H
#define YUI_LIST_COMPONENT_H

#include "../layer.h"
#include "../ytype.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ListComponent {
    Layer* layer;
    int item_height;
    int spacing;
    int hovered_index;
    int pressed_index;
    int touch_scrolled;
    char on_select_name[128];
} ListComponent;

ListComponent* list_component_create(Layer* layer);
void list_component_destroy(ListComponent* component);
ListComponent* list_component_create_from_json(Layer* layer, cJSON* json_obj);

void list_component_update_content_size(ListComponent* component);
int list_component_get_item_count(ListComponent* component);
int list_component_index_at_y(ListComponent* component, int x, int y);

int list_component_handle_mouse_event(Layer* layer, MouseEvent* event);
void list_component_handle_scroll_event(Layer* layer, int scroll_delta);
int list_component_handle_key_event(Layer* layer, KeyEvent* event);
int list_component_handle_touch_event(Layer* layer, TouchEvent* event);
void list_component_render(Layer* layer);

#ifdef __cplusplus
}
#endif

#endif  // YUI_LIST_COMPONENT_H
