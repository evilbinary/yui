#ifndef YUI_RENDER_H
#define YUI_RENDER_H


#include "layer.h"
#include "backend.h"



void render_layer(Layer* layer);
void render_inspect_overlay(Layer* layer);
void load_textures(Layer* root);
void load_font(Layer* root);
void load_all_fonts(Layer* layer);

// 添加滚动条渲染函数声明
void render_scrollbar(Layer* layer);
void render_vertical_scrollbar(Layer* layer);
void render_horizontal_scrollbar(Layer* layer);
int render_clip_start(Layer* layer,Rect* prev_clip);
void render_clip_end(Layer* layer,Rect* prev_clip);

/* Nested clip for components: always intersect with current parent clip.
 * push returns 0 if fully clipped (clip unchanged — skip drawing).
 * pop restores exactly (never intersect). */
int render_clip_push(const Rect* local, Rect* prev_out);
void render_clip_pop(const Rect* prev);

Texture* render_text(Layer* layer,const char* text, Color color);

/* 绘制图层阴影 + 背景（纯色或渐变）。override_bg 非空时覆盖 layer->bg_color */
void render_layer_background(Layer* layer, const Color* override_bg);

#endif