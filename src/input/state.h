#ifndef YUI_INPUT_STATE_H
#define YUI_INPUT_STATE_H

#include "../ytype.h"

#ifdef __cplusplus
extern "C" {
#endif

void input_state_init(void);
void input_state_reset(void);
void input_state_begin_frame(void);

/* 查询（由 KeyEvent / PointerEvent 监听更新） */
int input_state_key_down(const char* name);
int input_state_key_pressed(const char* name);
int input_state_key_released(const char* name);
float input_state_axis(const char* name);

void input_state_pointer_pos(int* x, int* y);
int input_state_pointer_button_down(int button);    /* 1=left 2=middle 3=right */
int input_state_pointer_button_pressed(int button);

#ifdef __cplusplus
}
#endif

#endif /* YUI_INPUT_STATE_H */
