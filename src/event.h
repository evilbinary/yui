#ifndef YUI_EVENT_H
#define YUI_EVENT_H

#include "ytype.h"
#include "layer.h"
#include "layout.h"

typedef struct KeyEvent KeyEvent;
typedef struct PointerEvent PointerEvent;

typedef void* (*EventHandler)(void* data);

int register_event_handler(const char* name, EventHandler handler);
void print_registered_events();
EventHandler find_event_by_name(const char* name);

void handle_key_event(Layer* layer, KeyEvent* event);
int handle_pointer_event(Layer* layer, PointerEvent* event);
int default_layer_handle_pointer_event(Layer* layer, PointerEvent* event);
const PointerEvent* get_current_pointer_event(void);
const char* pointer_phase_to_string(PointerPhase phase);

#endif
