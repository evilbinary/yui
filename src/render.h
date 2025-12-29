#ifndef YUI_RENDER_H
#define YUI_RENDER_H


#include "layer.h"
#include "backend.h"



void render_layer(Layer* layer);
void load_textures(Layer* root);
void load_font(Layer* root);
void load_all_fonts(Layer* layer);

// 添加滚动条渲染函数声明
void render_scrollbar(Layer* layer);
void render_vertical_scrollbar(Layer* layer);
void render_horizontal_scrollbar(Layer* layer);
void render_clip_start(Layer* layer,Rect* prev_clip);
void render_clip_end(Layer* layer,Rect* prev_clip);

Texture* render_text(Layer* layer,const char* text, Color color);

#endif