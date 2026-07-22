#ifndef YUI_TEXT_SYNTAX_H
#define YUI_TEXT_SYNTAX_H

#if !defined(YUI_TYPE_H) && !defined(YUI_BACKEND_MOBILE)
#ifdef D_SDL
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#else
#include <SDL.h>
#include <SDL_ttf.h>
#endif

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
#endif /* !YUI_TYPE_H */

#ifdef __cplusplus
extern "C" {
#endif

#define TEXT_SYNTAX_MAX_TOKENS 256
#define TEXT_SYNTAX_MAX_LANGS  32

typedef enum {
    TEXT_HL_DEFAULT = 0,
    TEXT_HL_KEY,
    TEXT_HL_STRING,
    TEXT_HL_NUMBER,
    TEXT_HL_KEYWORD,
    TEXT_HL_PUNCTUATION,
    TEXT_HL_COMMENT,
    TEXT_HL_HEADING,
    TEXT_HL_CODE,
    TEXT_HL_TYPE,
    TEXT_HL_PREPROC,
} TextHlKind;

typedef struct {
    int start;
    int end;
    TextHlKind type;
} TextHlToken;

typedef struct TextSyntaxConfig TextSyntaxConfig;
typedef struct TextSyntaxLang TextSyntaxLang;

typedef int (*TextSyntaxTokenizeFn)(const char* text, int start, int end,
                                    TextHlToken* out, int max_out, void* ctx);
typedef void (*TextSyntaxInitColorsFn)(TextSyntaxConfig* config, Color default_color);

struct TextSyntaxLang {
    const char* name;
    const char* const* aliases; /* NULL-terminated optional aliases, e.g. "md" */
    TextSyntaxTokenizeFn tokenize_line;
    TextSyntaxInitColorsFn init_colors; /* optional theme defaults */
    void* ctx;
};

struct TextSyntaxConfig {
    const TextSyntaxLang* lang; /* NULL = plain text */
    Color default_color;
    Color key_color;
    Color string_color;
    Color number_color;
    Color keyword_color;
    Color punctuation_color;
    Color comment_color;
};

void text_syntax_ensure_init(void);
int text_syntax_register(const TextSyntaxLang* lang);
const TextSyntaxLang* text_syntax_find(const char* name);

/* language: "json" / "sql" / "markdown" / "c" / NULL / "" */
void text_syntax_config_init(TextSyntaxConfig* config, const char* language, Color default_color);
void text_syntax_config_set_color(TextSyntaxConfig* config, const char* name, Color color);

int text_syntax_measure_width(DFont* font, const char* text, int start, int end, Color color);
int text_syntax_measure_range(DFont* font, const char* text, int start, int end,
                              const TextSyntaxConfig* config);
void text_syntax_render_range(DFont* font, const char* text, int start, int end,
                              const TextSyntaxConfig* config, int x, int y);

#ifdef __cplusplus
}
#endif

#endif
