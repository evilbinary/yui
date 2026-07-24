#include "state.h"
#include "../event.h"

#include <stdio.h>
#include <string.h>

#define INPUT_KEY_SLOTS 64

typedef struct {
    char name[32];
    int down;
    int prev;
    int pressed;
    int released;
} InputKeySlot;

static InputKeySlot g_keys[INPUT_KEY_SLOTS];
static int g_key_count;
static int g_pointer_x;
static int g_pointer_y;
static int g_btn_down[4];
static int g_btn_prev[4];
static int g_btn_pressed[4];
static int g_inited;
static int g_listeners_registered;

static InputKeySlot* input_key_slot(const char* name, int create)
{
    int i;
    if (!name || !name[0]) {
        return NULL;
    }
    for (i = 0; i < g_key_count; i++) {
        if (strcmp(g_keys[i].name, name) == 0) {
            return &g_keys[i];
        }
    }
    if (!create || g_key_count >= INPUT_KEY_SLOTS) {
        return NULL;
    }
    memset(&g_keys[g_key_count], 0, sizeof(g_keys[0]));
    strncpy(g_keys[g_key_count].name, name, sizeof(g_keys[0].name) - 1);
    g_keys[g_key_count].name[sizeof(g_keys[0].name) - 1] = '\0';
    return &g_keys[g_key_count++];
}

static void input_key_set(const char* name, int down)
{
    InputKeySlot* slot = input_key_slot(name, 1);
    if (!slot) {
        return;
    }
    slot->down = down ? 1 : 0;
}

/* key_code (SDLK / 兼容常量) → 逻辑名 */
static int input_key_name_from_code(int key_code, char* out, size_t out_len)
{
    if (!out || out_len < 2) {
        return 0;
    }
    out[0] = '\0';
    if (key_code == SDLK_LEFT) {
        strncpy(out, "Left", out_len - 1);
    } else if (key_code == SDLK_RIGHT) {
        strncpy(out, "Right", out_len - 1);
    } else if (key_code == SDLK_UP) {
        strncpy(out, "Up", out_len - 1);
    } else if (key_code == SDLK_DOWN) {
        strncpy(out, "Down", out_len - 1);
    } else if (key_code == SDLK_SPACE) {
        strncpy(out, "Space", out_len - 1);
    } else if (key_code == SDLK_RETURN || key_code == SDLK_KP_ENTER) {
        strncpy(out, "Return", out_len - 1);
    } else if (key_code == SDLK_ESCAPE) {
        strncpy(out, "Escape", out_len - 1);
    } else if (key_code == SDLK_TAB) {
        strncpy(out, "Tab", out_len - 1);
    } else if (key_code == SDLK_BACKSPACE) {
        strncpy(out, "Backspace", out_len - 1);
    } else if (key_code >= SDLK_F1 && key_code <= SDLK_F12) {
        snprintf(out, out_len, "F%d", (int)(key_code - SDLK_F1 + 1));
    } else if (key_code >= 'a' && key_code <= 'z') {
        out[0] = (char)(key_code - 'a' + 'A');
        out[1] = '\0';
    } else if (key_code >= 'A' && key_code <= 'Z') {
        out[0] = (char)key_code;
        out[1] = '\0';
    } else if (key_code >= '0' && key_code <= '9') {
        out[0] = (char)key_code;
        out[1] = '\0';
    } else {
        return 0;
    }
    out[out_len - 1] = '\0';
    return 1;
}

static void input_on_key(const KeyEvent* event)
{
    char name[32];
    int down;
    if (!event) {
        return;
    }
    if (event->type != KEY_EVENT_DOWN && event->type != KEY_EVENT_UP) {
        return;
    }
    if (!input_key_name_from_code(event->data.key.key_code, name, sizeof(name))) {
        return;
    }
    down = (event->type == KEY_EVENT_DOWN) ? 1 : 0;
    /* repeat DOWN 保持 down=1，不额外处理 */
    input_key_set(name, down);
    if (strcmp(name, "Return") == 0) {
        input_key_set("Enter", down);
    }
}

static void input_on_pointer(const PointerEvent* event)
{
    int btn;
    if (!event) {
        return;
    }
    g_pointer_x = event->x;
    g_pointer_y = event->y;
    if (event->device != POINTER_DEVICE_MOUSE) {
        return;
    }
    btn = event->button;
    if (btn < 1 || btn > 3) {
        return;
    }
    if (event->phase == POINTER_DOWN || event->phase == POINTER_DOUBLE_TAP) {
        g_btn_down[btn] = 1;
    } else if (event->phase == POINTER_UP || event->phase == POINTER_CANCEL) {
        g_btn_down[btn] = 0;
    }
}

void input_state_init(void)
{
    if (g_inited) {
        return;
    }
    memset(g_keys, 0, sizeof(g_keys));
    g_key_count = 0;
    g_pointer_x = 0;
    g_pointer_y = 0;
    memset(g_btn_down, 0, sizeof(g_btn_down));
    memset(g_btn_prev, 0, sizeof(g_btn_prev));
    memset(g_btn_pressed, 0, sizeof(g_btn_pressed));
    if (!g_listeners_registered) {
        register_key_event_listener(input_on_key);
        register_pointer_event_listener(input_on_pointer);
        g_listeners_registered = 1;
    }
    g_inited = 1;
}

void input_state_reset(void)
{
    input_state_init();
    memset(g_keys, 0, sizeof(g_keys));
    g_key_count = 0;
    g_pointer_x = 0;
    g_pointer_y = 0;
    memset(g_btn_down, 0, sizeof(g_btn_down));
    memset(g_btn_prev, 0, sizeof(g_btn_prev));
    memset(g_btn_pressed, 0, sizeof(g_btn_pressed));
}

void input_state_release_all_keys(void)
{
    int i;
    input_state_init();
    for (i = 0; i < g_key_count; i++) {
        g_keys[i].down = 0;
    }
    memset(g_btn_down, 0, sizeof(g_btn_down));
}

void input_state_begin_frame(void)
{
    int i;
    input_state_init();
    for (i = 0; i < g_key_count; i++) {
        g_keys[i].pressed = g_keys[i].down && !g_keys[i].prev;
        g_keys[i].released = !g_keys[i].down && g_keys[i].prev;
        g_keys[i].prev = g_keys[i].down;
    }
    for (i = 1; i <= 3; i++) {
        g_btn_pressed[i] = g_btn_down[i] && !g_btn_prev[i];
        g_btn_prev[i] = g_btn_down[i];
    }
}

int input_state_key_down(const char* name)
{
    InputKeySlot* slot;
    input_state_init();
    slot = input_key_slot(name, 0);
    return slot && slot->down;
}

int input_state_key_pressed(const char* name)
{
    InputKeySlot* slot;
    input_state_init();
    slot = input_key_slot(name, 0);
    return slot && slot->pressed;
}

int input_state_key_released(const char* name)
{
    InputKeySlot* slot;
    input_state_init();
    slot = input_key_slot(name, 0);
    return slot && slot->released;
}

float input_state_axis(const char* name)
{
    if (!name) {
        return 0.0f;
    }
    if (strcmp(name, "Horizontal") == 0) {
        float v = 0.0f;
        if (input_state_key_down("Left") || input_state_key_down("A")) {
            v -= 1.0f;
        }
        if (input_state_key_down("Right") || input_state_key_down("D")) {
            v += 1.0f;
        }
        return v;
    }
    if (strcmp(name, "Vertical") == 0) {
        float v = 0.0f;
        if (input_state_key_down("Up") || input_state_key_down("W")) {
            v -= 1.0f;
        }
        if (input_state_key_down("Down") || input_state_key_down("S")) {
            v += 1.0f;
        }
        return v;
    }
    return 0.0f;
}

void input_state_pointer_pos(int* x, int* y)
{
    input_state_init();
    if (x) {
        *x = g_pointer_x;
    }
    if (y) {
        *y = g_pointer_y;
    }
}

int input_state_pointer_button_down(int button)
{
    input_state_init();
    if (button < 1 || button > 3) {
        return 0;
    }
    return g_btn_down[button];
}

int input_state_pointer_button_pressed(int button)
{
    input_state_init();
    if (button < 1 || button > 3) {
        return 0;
    }
    return g_btn_pressed[button];
}
