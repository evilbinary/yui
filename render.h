#ifndef YUI_RENDER_H
#define YUI_RENDER_H


#include "layer.h"

extern SDL_Renderer* renderer;
extern float scale;

void render_layer(Layer* layer);
void load_textures(Layer* root);

// 添加滚动条渲染函数声明
void render_scrollbar(Layer* layer);
void render_clip_start(Layer* layer,SDL_Rect* prev_clip);
void render_clip_end(Layer* layer,SDL_Rect* prev_clip);

#endif