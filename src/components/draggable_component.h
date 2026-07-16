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
    int drag_handle_height;
    int show_dots;
    int dot_size;
    Color dot_color;
    Layer* anchor_dots[5];
} DraggableComponent;

DraggableComponent* draggable_component_create(Layer* layer);
DraggableComponent* draggable_component_create_from_json(Layer* layer, cJSON* json_obj);
void draggable_component_destroy(DraggableComponent* component);
int draggable_component_get_dot_style(Layer* layer, int* show_dots, int* dot_size,
                                      Color* dot_color);
void draggable_component_render(Layer* layer);
int draggable_component_handle_mouse_event(Layer* layer, MouseEvent* event);

#ifdef __cplusplus
}
#endif

#endif
