#ifndef YUI_TERMINAL_COMPONENT_H
#define YUI_TERMINAL_COMPONENT_H

#include "../ytype.h"
#include <libtsm.h>

typedef struct Layer Layer;
typedef struct KeyEvent KeyEvent;

typedef struct {
    Layer* layer;
    struct tsm_screen* screen;
    struct tsm_vte* vte;

    int cursor_pos;
    int scroll_x;
    char** history;
    int history_count;
    int history_capacity;
    int history_index;

    int line_height;
    int cell_width;
    int input_height;
    int input_padding;
    Color prompt_color;
    Color input_color;
    Color cursor_color;
    Color output_color;
    Color output_bg_color;
    char prompt_text[64];
    char on_command_name[128];
    EventHandler on_command;
    char on_key_name[128];
    EventHandler on_key;

    int last_key_type;
    int last_key_code;
    int last_key_mod;
    char last_key_text[32];

    unsigned int cols;
    unsigned int rows;
    unsigned int scrollback_max;
    int needs_prompt;
    int selecting;
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
