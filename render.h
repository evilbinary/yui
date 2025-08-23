#ifndef YUI_RENDER_H
#define YUI_RENDER_H


#include "layer.h"

extern SDL_Renderer* renderer;
extern float scale;

void render_layer(Layer* layer);
void load_textures(Layer* root);

#endif
