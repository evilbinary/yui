#ifndef YUI_TEXT_SYNTAX_H
#define YUI_TEXT_SYNTAX_H

#ifdef D_SDL
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#else
#include <SDL.h>
#include <SDL_ttf.h>
#endif

#ifndef YUI_TYPE_H
#ifndef SDL2
#define SDL2 1
#endif
#if SDL2
#define Color SDL_Color
#else
typedef struct Color {
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
} Color;
#endif
#define DFont TTF_Font
#endif

typedef enum {
    TEXT_SYNTAX_NONE = 0,
    TEXT_SYNTAX_JSON,
    TEXT_SYNTAX_SQL,
    TEXT_SYNTAX_MARKDOWN,
} TextSyntaxLanguage;

typedef struct {
    TextSyntaxLanguage language;
    Color default_color;
    Color key_color;
    Color string_color;
    Color number_color;
    Color keyword_color;
    Color punctuation_color;
    Color comment_color;
} TextSyntaxConfig;

void text_syntax_config_init(TextSyntaxConfig* config, TextSyntaxLanguage language, Color default_color);
void text_syntax_config_set_color(TextSyntaxConfig* config, const char* name, Color color);

int text_syntax_measure_width(DFont* font, const char* text, int start, int end, Color color);
void text_syntax_render_range(DFont* font, const char* text, int start, int end,
                              const TextSyntaxConfig* config, int x, int y);

#endif
