#ifndef YUI_GAME_H
#define YUI_GAME_H

#include "../ytype.h"

#ifndef YUI_WITH_GAME
#define YUI_WITH_GAME 1
#endif

#if YUI_WITH_GAME

#ifdef __cplusplus
extern "C" {
#endif

#define GAME_MAX_ENTITIES 128
#define GAME_ID_LEN 64
#define GAME_PATH_LEN 256

typedef struct GameEntity {
    int alive;
    char id[GAME_ID_LEN];
    char tag[GAME_ID_LEN];
    char script[GAME_ID_LEN]; /* JS global function name: update(entity, dt) */
    float x;
    float y;
    float z;
    float vx;
    float vy;
    float w;
    float h;
    float cw; /* collider; 0 => use w/h */
    float ch;
    float cox;
    float coy;
    int trigger;
    int solid;
    int grounded;
    Color color;
    char sprite_path[GAME_PATH_LEN];
    Texture* texture;
} GameEntity;

typedef struct GameCamera {
    float x;
    float y;
    char follow[GAME_ID_LEN];
} GameCamera;

typedef void (*GameScriptUpdateFn)(GameEntity* entity, float dt);

void game_init(void);
void game_shutdown(void);
int game_is_active(void);

void game_update(float dt_override); /* <0 => use internal clock */
void game_render(void);

void game_clear_scene(void);
int game_load_scene_json(const char* path);
GameEntity* game_spawn_from_json(cJSON* obj);
GameEntity* game_spawn(const char* id);
void game_destroy(GameEntity* e);
void game_destroy_by_id(const char* id);
GameEntity* game_find(const char* id);
GameEntity* game_find_by_tag(const char* tag);
GameEntity* game_entities(int* out_count);

float game_time_dt(void);
float game_time_scale(void);
void game_time_set_scale(float scale);

void game_input_begin_frame(void);
int game_input_down(const char* name);
int game_input_pressed(const char* name);
int game_input_released(const char* name);
float game_input_axis(const char* name);
void game_input_pointer(int* x, int* y);
int game_input_mouse_down(int button); /* 1=left 2=middle 3=right */
int game_input_mouse_pressed(int button);

GameCamera* game_camera(void);
void game_camera_set(float x, float y);
void game_camera_follow(const char* id);

void game_entity_world_aabb(const GameEntity* e, float* out_x, float* out_y,
                           float* out_w, float* out_h);
int game_aabb_overlap(float ax, float ay, float aw, float ah,
                      float bx, float by, float bw, float bh);
int game_entities_overlap(const GameEntity* a, const GameEntity* b);
/* Move e by vx*dt, vy*dt and resolve against solid colliders. */
void game_move_and_collide(GameEntity* e, float dt);

void game_set_script_update_fn(GameScriptUpdateFn fn);
void game_set_enabled(int on);

#ifdef __cplusplus
}
#endif

#endif /* YUI_WITH_GAME */
#endif /* YUI_GAME_H */
