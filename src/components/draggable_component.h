#ifndef YUI_DRAGGABLE_COMPONENT_H
#define YUI_DRAGGABLE_COMPONENT_H

#include "../layer.h"
#include "../ytype.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct DraggableComponent {
    Layer* layer;
    int dragging;
    int drag_offset_x;
    int drag_offset_y;
    int drag_start_x;
    int drag_start_y;
    int drag_handle_height;
    char on_drag_change_name[MAX_PATH];
    int show_dots;
    int dot_size;
    Color dot_color;
    Layer* dot_overlay;
} DraggableComponent;

DraggableComponent* draggable_component_create(Layer* layer);
DraggableComponent* draggable_component_create_from_json(Layer* layer, cJSON* json_obj);
void draggable_component_destroy(DraggableComponent* component);
void draggable_component_render(Layer* layer);
int draggable_component_handle_mouse_event(Layer* layer, MouseEvent* event);

#ifdef __cplusplus
}
#endif

#endif
