#include "game.h"
#include "internal.h"

#if YUI_WITH_GAME

#include "../backend.h"

void game_sprite_draw_entity(const GameEntity* e)
{
    float sx;
    float sy;
    Rect dst;
    Rect src;
    if (!e || !e->alive) {
        return;
    }
    game_camera_world_to_screen(e->x, e->y, &sx, &sy);
    dst.x = (int)sx;
    dst.y = (int)sy;
    // /* Solids: always draw at hitbox size so visuals can't drift from collision. */
    // if (e->solid && e->cw > 0.0f && e->ch > 0.0f) {
    //     dst.w = (int)e->cw;
    //     dst.h = (int)e->ch;
    // } else {
        dst.w = (int)(e->w > 0 ? e->w : 16);
        dst.h = (int)(e->h > 0 ? e->h : 16);
    // }
    if (dst.w < 1) dst.w = 1;
    if (dst.h < 1) dst.h = 1;

    /* Skip completely off-screen (optional cull) — still draw if partially visible */
    if (e->texture) {
        if (e->use_frame && e->frame_w > 0 && e->frame_h > 0) {
            src.x = e->frame_x;
            src.y = e->frame_y;
            src.w = e->frame_w;
            src.h = e->frame_h;
            backend_render_text_copy(e->texture, &src, &dst);
        } else {
            backend_render_text_copy(e->texture, NULL, &dst);
        }
    } else {
        Color c = e->color;
        if (c.a == 0) {
            c.a = 255;
        }
        backend_render_fill_rect(&dst, c);
    }
}

#endif
