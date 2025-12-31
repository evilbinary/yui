#ifndef YUI_EVENT_H
#define YUI_EVENT_H

#include "layer.h"
#include "ytype.h"
#include "layout.h"

// Fix: Make sure EventHandler is properly declared
typedef void (*EventHandler)(void* data);

int register_event_handler(const char* name, EventHandler handler);
void print_registered_events();
EventHandler find_event_by_name(const char* name);

// 处理垂直滚动事件
void handle_scroll_event(Layer* layer,int mouse_x,int mouse_y,int scroll_deltax,int scroll_deltay);

// 处理水平滚动事件
void handle_horizontal_scroll_event(Layer* layer, int scroll_delta);

// 处理垂直滚动事件
void handler_virtical_scroll_event(Layer* layer, int scroll_delta);

// Fix: Make sure KeyEvent is properly declared
void handle_key_event(Layer* layer, struct KeyEvent* event);

void handle_mouse_event(Layer* layer, struct MouseEvent* event);
void handle_scrollbar_drag_event(Layer* root, int mouse_x, int mouse_y, SDL_EventType event_type);
#endif