#ifndef YUI_TERMINAL_COMPONENT_H
#define YUI_TERMINAL_COMPONENT_H

#include "../ytype.h"

typedef struct Layer Layer;
typedef struct KeyEvent KeyEvent;

typedef struct {
    Layer* layer;
    int cursor_pos;
    int scroll_x;
    char** history;
    int history_count;
    int history_capacity;
    int history_index;
    char** output_lines;
    int output_count;
    int output_capacity;
    int output_scroll;
    int line_height;
    int input_height;
    int input_padding;
    Color prompt_color;
    Color input_color;
    Color cursor_color;
    Color output_color;
    char prompt_text[64];
    char on_command_name[128];
    EventHandler on_command;
} TerminalComponent;

TerminalComponent* terminal_component_create(Layer* layer);
TerminalComponent* terminal_component_create_from_json(Layer* layer, cJSON* json_obj);
void terminal_component_destroy(TerminalComponent* comp);
void terminal_component_render(Layer* layer);
int terminal_component_handle_pointer_event(Layer* layer, PointerEvent* event);
int terminal_component_handle_key_event(Layer* layer, KeyEvent* event);
int terminal_component_register_event(Layer* layer, const char* event_name,
                                      const char* event_func_name, EventHandler event_handler);
void terminal_component_append_output(TerminalComponent* comp, const char* text);
void terminal_component_set_prompt(TerminalComponent* comp, const char* prompt);

#endif
