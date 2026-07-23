#include "game.h"
#include "internal.h"

#if YUI_WITH_GAME

#include "../backend.h"

void game_sprite_draw_entity(const GameEntity* e)
{
    float sx;
    float sy;
    Rect dst;
    if (!e || !e->alive) {
        return;
    }
    game_camera_world_to_screen(e->x, e->y, &sx, &sy);
    dst.x = (int)sx;
    dst.y = (int)sy;
    dst.w = (int)(e->w > 0 ? e->w : 16);
    dst.h = (int)(e->h > 0 ? e->h : 16);
    if (dst.w < 1) dst.w = 1;
    if (dst.h < 1) dst.h = 1;

    if (e->texture) {
        backend_render_text_copy(e->texture, NULL, &dst);
    } else {
        backend_render_fill_rect(&dst, e->color);
    }
}

#endif
