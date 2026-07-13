#ifndef YUI_SASH_COMPONENT_H
#define YUI_SASH_COMPONENT_H

#include "../ytype.h"

typedef struct Layer Layer;
typedef struct MouseEvent MouseEvent;

typedef struct {
    Layer* layer;
    int dragging;
    int drag_start_y;    // drag_start_x when horizontal
    int initial_height;  // initial_width when horizontal
    Layer* target;
    char target_id[MAX_TEXT];
    int min_size;
    int hover;
    int horizontal;      // 0=vertical (resize height), 1=horizontal (resize width)
    char on_change_name[MAX_PATH];
    EventHandler on_change;
    Color bg_color;
    Color hover_bg_color;
    Color border_color;
    Color dot_color;
    Color hover_dot_color;
    Color accent_color;
} SashComponent;

SashComponent* sash_component_create(Layer* layer);
SashComponent* sash_component_create_from_json(Layer* layer, cJSON* json_obj);
void sash_component_destroy(SashComponent* component);
void sash_component_render(Layer* layer);
int sash_component_handle_mouse_event(Layer* layer, MouseEvent* event);

#endif
