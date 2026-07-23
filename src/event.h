#ifndef YUI_EVENT_H
#define YUI_EVENT_H

#include "ytype.h"
#include "layer.h"
#include "layout.h"

typedef void* (*EventHandler)(void* data);

/* Global device listeners (UI Layer tree 之外；不替代 layer->handle_* ) */
typedef void (*PointerEventListener)(const PointerEvent* event);
typedef void (*KeyEventListener)(const KeyEvent* event);

int register_event_handler(const char* name, EventHandler handler);
void print_registered_events();
EventHandler find_event_by_name(const char* name);

/* 注册/注销全局 pointer、key 监听（在 Layer 分发前调用；不可消费事件） */
int register_pointer_event_listener(PointerEventListener listener);
int unregister_pointer_event_listener(PointerEventListener listener);
int register_key_event_listener(KeyEventListener listener);
int unregister_key_event_listener(KeyEventListener listener);

void handle_key_event(Layer* layer, KeyEvent* event);
int handle_pointer_event(Layer* layer, PointerEvent* event);
int default_layer_handle_pointer_event(Layer* layer, PointerEvent* event);
const PointerEvent* get_current_pointer_event(void);
const char* pointer_phase_to_string(PointerPhase phase);

/* 当前指针手势是否发生过内容区滚动（DOWN→UP 之间） */
void pointer_gesture_mark_scrolled(void);

#endif
