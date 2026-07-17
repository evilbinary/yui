/**
 * @file lv_port_indev.c
 */

/*********************
 *      INCLUDES
 *********************/
#include "lv_port_indev.h"
#include "../lv_port.h"

#ifdef D_SDL
#include <SDL2/SDL.h>
#else
#include <SDL.h>
#endif

#include <stdbool.h>

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void mouse_init(void);
static void mouse_read(lv_indev_drv_t* indev_drv, lv_indev_data_t* data);
static bool mouse_is_pressed(void);
static void mouse_get_xy(lv_coord_t* x, lv_coord_t* y);

static void keypad_init(void);
static void keypad_read(lv_indev_drv_t* indev_drv, lv_indev_data_t* data);
static uint32_t sdl_key_to_lv(SDL_Keycode sym);
static void lv_port_feed_text(const char* text);

/**********************
 *  STATIC VARIABLES
 **********************/
lv_indev_t* indev_mouse;
lv_indev_t* indev_keypad;

static lv_point_t g_mouse_point;
static bool g_mouse_pressed = false;
static int g_should_quit = 0;
static lv_port_sdl_event_fn g_sdl_event_hook = NULL;
static void* g_sdl_event_hook_ud = NULL;

static lv_group_t* g_default_group = NULL;
static uint32_t g_keypad_last_key = 0;
static lv_indev_state_t g_keypad_state = LV_INDEV_STATE_REL;

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_port_indev_init(void)
{
    static lv_indev_drv_t indev_drv;
    static lv_indev_drv_t keypad_drv;

    mouse_init();
    keypad_init();

    g_default_group = lv_group_create();
    lv_group_set_default(g_default_group);

    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = mouse_read;
    indev_mouse = lv_indev_drv_register(&indev_drv);

    lv_indev_drv_init(&keypad_drv);
    keypad_drv.type = LV_INDEV_TYPE_KEYPAD;
    keypad_drv.read_cb = keypad_read;
    indev_keypad = lv_indev_drv_register(&keypad_drv);
    lv_indev_set_group(indev_keypad, g_default_group);

    /* Use SDL system cursor; LVGL software cursor partial refresh erases YUI framebuffer. */
    SDL_ShowCursor(SDL_ENABLE);
    SDL_StartTextInput();
}

void lv_port_indev_deinit(void)
{
    SDL_StopTextInput();
    indev_mouse = NULL;
    indev_keypad = NULL;
    g_default_group = NULL;
    g_mouse_pressed = false;
    g_keypad_last_key = 0;
    g_keypad_state = LV_INDEV_STATE_REL;
    g_should_quit = 0;
}

void lv_port_indev_set_sdl_event_hook(lv_port_sdl_event_fn fn, void* user_data)
{
    g_sdl_event_hook = fn;
    g_sdl_event_hook_ud = user_data;
}

void lv_port_indev_poll(void)
{
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (g_sdl_event_hook) {
            g_sdl_event_hook(&event, g_sdl_event_hook_ud);
        }
        if (event.type == SDL_QUIT) {
            g_should_quit = 1;
            continue;
        }
        if (event.type == SDL_KEYDOWN) {
            uint32_t lv_key = sdl_key_to_lv(event.key.keysym.sym);
            if (lv_key != 0) {
                g_keypad_last_key = lv_key;
                g_keypad_state = LV_INDEV_STATE_PR;
            }
        } else if (event.type == SDL_KEYUP) {
            g_keypad_state = LV_INDEV_STATE_REL;
        } else if (event.type == SDL_TEXTINPUT) {
            lv_port_feed_text(event.text.text);
        } else if (event.type == SDL_MOUSEMOTION) {
            g_mouse_point.x = event.motion.x;
            g_mouse_point.y = event.motion.y;
        } else if (event.type == SDL_MOUSEBUTTONDOWN) {
            g_mouse_point.x = event.button.x;
            g_mouse_point.y = event.button.y;
            g_mouse_pressed = true;
        } else if (event.type == SDL_MOUSEBUTTONUP) {
            g_mouse_point.x = event.button.x;
            g_mouse_point.y = event.button.y;
            g_mouse_pressed = false;
        } else if (event.type == SDL_FINGERDOWN) {
            int w = lv_port_get_width();
            int h = lv_port_get_height();
            if (w > 0 && h > 0) {
                g_mouse_point.x = (lv_coord_t)(event.tfinger.x * w);
                g_mouse_point.y = (lv_coord_t)(event.tfinger.y * h);
            }
            g_mouse_pressed = true;
        } else if (event.type == SDL_FINGERMOTION) {
            int w = lv_port_get_width();
            int h = lv_port_get_height();
            if (w > 0 && h > 0) {
                g_mouse_point.x = (lv_coord_t)(event.tfinger.x * w);
                g_mouse_point.y = (lv_coord_t)(event.tfinger.y * h);
            }
        } else if (event.type == SDL_FINGERUP) {
            int w = lv_port_get_width();
            int h = lv_port_get_height();
            if (w > 0 && h > 0) {
                g_mouse_point.x = (lv_coord_t)(event.tfinger.x * w);
                g_mouse_point.y = (lv_coord_t)(event.tfinger.y * h);
            }
            g_mouse_pressed = false;
        }
    }
}

int lv_port_indev_should_quit(void)
{
    return g_should_quit;
}

void lv_port_indev_request_quit(void)
{
    g_should_quit = 1;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void mouse_init(void)
{
    g_mouse_point.x = 0;
    g_mouse_point.y = 0;
    g_mouse_pressed = false;
}

static void mouse_read(lv_indev_drv_t* indev_drv, lv_indev_data_t* data)
{
    (void)indev_drv;

    mouse_get_xy(&data->point.x, &data->point.y);
    if (mouse_is_pressed()) {
        data->state = LV_INDEV_STATE_PR;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

static bool mouse_is_pressed(void)
{
    return g_mouse_pressed;
}

static void mouse_get_xy(lv_coord_t* x, lv_coord_t* y)
{
    if (x) {
        *x = g_mouse_point.x;
    }
    if (y) {
        *y = g_mouse_point.y;
    }
}

static void keypad_init(void)
{
    g_keypad_last_key = 0;
    g_keypad_state = LV_INDEV_STATE_REL;
}

static void keypad_read(lv_indev_drv_t* indev_drv, lv_indev_data_t* data)
{
    (void)indev_drv;

    data->key = g_keypad_last_key;
    data->state = g_keypad_state;
}

static uint32_t sdl_key_to_lv(SDL_Keycode sym)
{
    switch (sym) {
        case SDLK_RIGHT:
            return LV_KEY_RIGHT;
        case SDLK_LEFT:
            return LV_KEY_LEFT;
        case SDLK_UP:
            return LV_KEY_UP;
        case SDLK_DOWN:
            return LV_KEY_DOWN;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            return LV_KEY_ENTER;
        case SDLK_ESCAPE:
            return LV_KEY_ESC;
        case SDLK_BACKSPACE:
            return LV_KEY_BACKSPACE;
        case SDLK_DELETE:
            return LV_KEY_DEL;
        case SDLK_TAB:
            return LV_KEY_NEXT;
        case SDLK_HOME:
            return LV_KEY_HOME;
        case SDLK_END:
            return LV_KEY_END;
        default:
            return 0;
    }
}

static void lv_port_feed_text(const char* text)
{
    lv_group_t* group;

    if (!text || !text[0]) {
        return;
    }

    group = lv_group_get_default();
    if (!group) {
        return;
    }

    uint32_t i = 0;
    while (text[i] != '\0') {
        uint32_t c = _lv_txt_encoded_next(text, &i);
        if (c == '\r') {
            continue;
        }
        lv_group_send_data(group, c);
    }
}
