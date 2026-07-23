#include "game.h"

#if YUI_WITH_GAME

#include "../input/state.h"

void game_input_reset(void)
{
    input_state_reset();
}

void game_input_begin_frame(void)
{
    input_state_begin_frame();
}

int game_input_down(const char* name)
{
    return input_state_key_down(name);
}

int game_input_pressed(const char* name)
{
    return input_state_key_pressed(name);
}

int game_input_released(const char* name)
{
    return input_state_key_released(name);
}

float game_input_axis(const char* name)
{
    return input_state_axis(name);
}

void game_input_pointer(int* x, int* y)
{
    input_state_pointer_pos(x, y);
}

int game_input_mouse_down(int button)
{
    return input_state_pointer_button_down(button);
}

int game_input_mouse_pressed(int button)
{
    return input_state_pointer_button_pressed(button);
}

#endif
