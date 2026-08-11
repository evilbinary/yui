#ifndef YUI_GAME_INTERNAL_H
#define YUI_GAME_INTERNAL_H

#include "game.h"

#if YUI_WITH_GAME

void game_time_reset(void);
float game_time_tick(void);

void game_input_reset(void);
int game_input_mouse_pressed(int button);

void game_entity_pool_init(void);
void game_entity_pool_clear(void);
void game_entity_pool_free(void);
GameEntity* game_entity_alloc(void);
void game_entity_free(GameEntity* e);

void game_camera_init(void);
void game_camera_update(void);
void game_camera_world_to_screen(float wx, float wy, float* sx, float* sy);

void game_sprite_draw_entity(const GameEntity* e);

void game_collide_resolve_solids(GameEntity* e, float dt);

void game_anim_apply_json(GameEntity* e, cJSON* anim);

/* 游戏进行时强制 FULL（每帧清屏+全量重绘，避免脏画布残影），退出恢复原模式 */
void game_apply_render_mode(int active);

void game_perf_begin_update(void);
void game_perf_end_update(void);
void game_perf_begin_render(void);
void game_perf_end_render(int entity_draws);
int game_particles_draw_count(void);

#endif
#endif
