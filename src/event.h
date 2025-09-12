#ifndef YUI_EVENT_H
#define YUI_EVENT_H

#include "layer.h"
#include "ytype.h"
#include "layout.h"

int register_event_handler(const char* name, EventHandler handler);

EventHandler find_event_by_name(const char* name);

void handle_scroll_event(Layer* layer, int scroll_delta);

#endif