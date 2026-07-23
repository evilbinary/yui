#include "game.h"
#include "internal.h"

#if YUI_WITH_GAME

#include "../backend.h"
#include <stdio.h>
#include <string.h>

static int g_debug_boxes;
static DFont* g_dbg_font;
static Uint32 g_last_dump_ms;
static int g_dumped_once;

void game_debug_set_boxes(int enabled)
{
    g_debug_boxes = enabled ? 1 : 0;
    if (g_debug_boxes) {
        g_dumped_once = 0;
        g_last_dump_ms = 0;
        printf("[game-debug] collider boxes ON (F3 toggle)\n");
    } else {
        printf("[game-debug] collider boxes OFF\n");
    }
}

int game_debug_boxes_enabled(void)
{
    return g_debug_boxes;
}

static DFont* game_debug_font(void)
{
    if (g_dbg_font) {
        return g_dbg_font;
    }
    g_dbg_font = backend_load_font("app/assets/Roboto-Regular.ttf", 12);
    if (!g_dbg_font) {
        g_dbg_font = backend_load_font("assets/Roboto-Regular.ttf", 12);
    }
    return g_dbg_font;
}

static void game_debug_draw_label(float wx, float wy, const char* text, Color color)
{
    Texture* tex;
    Rect dst;
    float sx, sy;
    int tw = 0, th = 0;
    DFont* font = game_debug_font();
    if (!font || !text || !text[0]) {
        return;
    }
    tex = backend_render_texture(font, text, color);
    if (!tex) {
        return;
    }
    backend_query_texture(tex, NULL, NULL, &tw, &th);
    game_camera_world_to_screen(wx, wy, &sx, &sy);
    dst.x = (int)sx;
    dst.y = (int)sy - th - 2;
    dst.w = tw;
    dst.h = th;
    backend_render_text_copy(tex, NULL, &dst);
    backend_render_text_destroy(tex);
}

static void game_debug_dump_entities(void)
{
    int n = 0;
    int i;
    GameEntity* all = game_entities(&n);
    printf("[game-debug] ---- entities (%d) ----\n", n);
    for (i = 0; i < n; i++) {
        GameEntity* e = &all[i];
        float cx, cy, cw, ch;
        if (!e->alive) {
            continue;
        }
        game_entity_world_aabb(e, &cx, &cy, &cw, &ch);
        printf("  id=%-16s tag=%-10s solid=%d trigger=%d  "
               "sprite=%.0fx%.0f  hit=%.0fx%.0f@(+%.0f,+%.0f)  "
               "pos=(%.1f,%.1f) grounded=%d\n",
               e->id[0] ? e->id : "(none)",
               e->tag[0] ? e->tag : "-",
               e->solid, e->trigger,
               e->w, e->h,
               cw, ch, e->cox, e->coy,
               e->x, e->y, e->grounded);
        if (e->solid && e->cw > 0.0f && e->w != e->cw) {
            printf("  !! MISMATCH id=%s  e->w=%.1f  e->cw=%.1f (display vs hitbox)\n",
                   e->id[0] ? e->id : "?", e->w, e->cw);
        }
    }
    printf("[game-debug] ------------------------\n");
}

void game_debug_update(void)
{
    Uint32 now;
    if (game_input_pressed("F3")) {
        game_debug_set_boxes(!g_debug_boxes);
    }
    if (!g_debug_boxes) {
        return;
    }
    now = backend_get_ticks();
    if (!g_dumped_once || (now - g_last_dump_ms) >= 1000) {
        game_debug_dump_entities();
        g_last_dump_ms = now;
        g_dumped_once = 1;
    }
}

void game_debug_render(void)
{
    int n = 0;
    int i;
    GameEntity* all;
    if (!g_debug_boxes) {
        return;
    }
    all = game_entities(&n);
    for (i = 0; i < n; i++) {
        GameEntity* e = &all[i];
        float sx, sy;
        float hx, hy, hw, hh;
        Rect sprite_r;
        Rect hit_r;
        Color sprite_c = {0, 200, 255, 255};
        Color hit_c;
        char label[128];
        if (!e->alive) {
            continue;
        }

        /* cyan = true e->w/h (what size field holds; may differ from draw) */
        game_camera_world_to_screen(e->x, e->y, &sx, &sy);
        sprite_r.x = (int)sx;
        sprite_r.y = (int)sy;
        sprite_r.w = (int)(e->w > 0 ? e->w : 1);
        sprite_r.h = (int)(e->h > 0 ? e->h : 1);
        backend_render_rect(&sprite_r, sprite_c);

        /* collider / hit box */
        game_entity_world_aabb(e, &hx, &hy, &hw, &hh);
        game_camera_world_to_screen(hx, hy, &sx, &sy);
        hit_r.x = (int)sx;
        hit_r.y = (int)sy;
        hit_r.w = (int)(hw > 0 ? hw : 1);
        hit_r.h = (int)(hh > 0 ? hh : 1);
        if (e->solid) {
            hit_c = (Color){50, 255, 80, 255};
        } else if (e->trigger) {
            hit_c = (Color){255, 220, 50, 255};
        } else {
            hit_c = (Color){255, 80, 80, 255};
        }
        backend_render_rect(&hit_r, hit_c);

        snprintf(label, sizeof(label), "%s  sw=%.0fx%.0f hw=%.0fx%.0f",
                 e->id[0] ? e->id : "?", e->w, e->h, hw, hh);
        game_debug_draw_label(e->x, e->y, label, hit_c);
    }
}

#endif
