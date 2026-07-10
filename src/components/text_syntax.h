#ifndef YUI_TEXT_SYNTAX_H
#define YUI_TEXT_SYNTAX_H

#ifdef D_SDL
#include <SDL2/SDL.h>
#else
#include <SDL.h>
#endif

typedef struct TTF_Font DFont;
typedef SDL_Color Color;

typedef enum {
    TEXT_SYNTAX_NONE = 0,
    TEXT_SYNTAX_JSON,
    TEXT_SYNTAX_SQL,
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
