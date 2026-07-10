#include "text_syntax.h"
#include "../backend.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

extern float scale;

typedef enum {
    HL_TOKEN_DEFAULT = 0,
    HL_TOKEN_KEY,
    HL_TOKEN_STRING,
    HL_TOKEN_NUMBER,
    HL_TOKEN_KEYWORD,
    HL_TOKEN_PUNCTUATION,
} HighlightTokenType;

typedef struct {
    int start;
    int end;
    HighlightTokenType type;
} HighlightToken;

#define MAX_HIGHLIGHT_TOKENS 256

void text_syntax_config_init(TextSyntaxConfig* config, TextSyntaxLanguage language, Color default_color) {
    if (!config) return;
    memset(config, 0, sizeof(TextSyntaxConfig));
    config->language = language;
    config->default_color = default_color;
    config->key_color = (Color){156, 220, 254, 255};
    config->string_color = (Color){206, 145, 120, 255};
    config->number_color = (Color){181, 206, 168, 255};
    config->keyword_color = (Color){86, 156, 214, 255};
    config->punctuation_color = (Color){212, 212, 212, 255};
}

void text_syntax_config_set_color(TextSyntaxConfig* config, const char* name, Color color) {
    if (!config || !name) return;
    if (strcmp(name, "key") == 0) config->key_color = color;
    else if (strcmp(name, "string") == 0) config->string_color = color;
    else if (strcmp(name, "number") == 0) config->number_color = color;
    else if (strcmp(name, "keyword") == 0) config->keyword_color = color;
    else if (strcmp(name, "punctuation") == 0) config->punctuation_color = color;
    else if (strcmp(name, "default") == 0) config->default_color = color;
}

static Color token_color(const TextSyntaxConfig* config, HighlightTokenType type) {
    switch (type) {
        case HL_TOKEN_KEY: return config->key_color;
        case HL_TOKEN_STRING: return config->string_color;
        case HL_TOKEN_NUMBER: return config->number_color;
        case HL_TOKEN_KEYWORD: return config->keyword_color;
        case HL_TOKEN_PUNCTUATION: return config->punctuation_color;
        default: return config->default_color;
    }
}

static int is_json_keyword(const char* text, int pos, int end, const char* word, int word_len) {
    if (pos + word_len > end) return 0;
    if (strncmp(text + pos, word, word_len) != 0) return 0;
    if (pos + word_len < end) {
        char next = text[pos + word_len];
        if (isalnum((unsigned char)next) || next == '_') return 0;
    }
    return 1;
}

static int json_tokenize_line(const char* text, int start, int end, HighlightToken tokens[], int max_tokens) {
    int count = 0;
    int pos = start;

    while (pos < end && count < max_tokens) {
        char c = text[pos];
        if (c == ' ' || c == '\t' || c == '\r') {
            int ws = pos;
            while (pos < end && (text[pos] == ' ' || text[pos] == '\t' || text[pos] == '\r')) {
                pos++;
            }
            tokens[count++] = (HighlightToken){ws, pos, HL_TOKEN_DEFAULT};
            continue;
        }

        if (c == '"') {
            int str_start = pos;
            pos++;
            while (pos < end) {
                if (text[pos] == '\\' && pos + 1 < end) {
                    pos += 2;
                    continue;
                }
                if (text[pos] == '"') {
                    pos++;
                    break;
                }
                pos++;
            }
            HighlightTokenType type = HL_TOKEN_STRING;
            int check = pos;
            while (check < end && (text[check] == ' ' || text[check] == '\t' || text[check] == '\r')) {
                check++;
            }
            if (check < end && text[check] == ':') {
                type = HL_TOKEN_KEY;
            }
            tokens[count++] = (HighlightToken){str_start, pos, type};
            continue;
        }

        if (c == '-' || (c >= '0' && c <= '9')) {
            int num_start = pos;
            if (c == '-') pos++;
            while (pos < end && (isdigit((unsigned char)text[pos]) || text[pos] == '.' ||
                                  text[pos] == 'e' || text[pos] == 'E' ||
                                  text[pos] == '+' || text[pos] == '-')) {
                pos++;
            }
            tokens[count++] = (HighlightToken){num_start, pos, HL_TOKEN_NUMBER};
            continue;
        }

        if (is_json_keyword(text, pos, end, "true", 4)) {
            tokens[count++] = (HighlightToken){pos, pos + 4, HL_TOKEN_KEYWORD};
            pos += 4;
            continue;
        }
        if (is_json_keyword(text, pos, end, "false", 5)) {
            tokens[count++] = (HighlightToken){pos, pos + 5, HL_TOKEN_KEYWORD};
            pos += 5;
            continue;
        }
        if (is_json_keyword(text, pos, end, "null", 4)) {
            tokens[count++] = (HighlightToken){pos, pos + 4, HL_TOKEN_KEYWORD};
            pos += 4;
            continue;
        }

        if (c == '{' || c == '}' || c == '[' || c == ']' || c == ':' || c == ',') {
            tokens[count++] = (HighlightToken){pos, pos + 1, HL_TOKEN_PUNCTUATION};
            pos++;
            continue;
        }

        tokens[count++] = (HighlightToken){pos, pos + 1, HL_TOKEN_DEFAULT};
        pos++;
    }

    return count;
}

int text_syntax_measure_width(DFont* font, const char* text, int start, int end, Color color) {
    if (!font || !text || start >= end) return 0;
    int len = end - start;
    char* buf = (char*)malloc((size_t)len + 1);
    if (!buf) return 0;
    memcpy(buf, text + start, (size_t)len);
    buf[len] = '\0';
    Texture* tex = backend_render_texture(font, buf, color);
    free(buf);
    if (!tex) return 0;
    int width = 0, height = 0;
    backend_query_texture(tex, NULL, NULL, &width, &height);
    backend_render_text_destroy(tex);
    return width / (int)scale;
}

static void render_plain_segment(DFont* font, const char* text, int start, int end, Color color, int x, int y) {
    if (!font || start >= end) return;
    int len = end - start;
    char* buf = (char*)malloc((size_t)len + 1);
    if (!buf) return;
    memcpy(buf, text + start, (size_t)len);
    buf[len] = '\0';
    Texture* tex = backend_render_texture(font, buf, color);
    free(buf);
    if (!tex) return;
    int width = 0, height = 0;
    backend_query_texture(tex, NULL, NULL, &width, &height);
    Rect rect = {x, y, width / (int)scale, height / (int)scale};
    backend_render_text_copy(tex, NULL, &rect);
    backend_render_text_destroy(tex);
}

void text_syntax_render_range(DFont* font, const char* text, int start, int end,
                              const TextSyntaxConfig* config, int x, int y) {
    if (!font || !text || !config || start >= end) return;

    if (config->language == TEXT_SYNTAX_NONE) {
        render_plain_segment(font, text, start, end, config->default_color, x, y);
        return;
    }

    HighlightToken tokens[MAX_HIGHLIGHT_TOKENS];
    int token_count = 0;
    if (config->language == TEXT_SYNTAX_JSON) {
        token_count = json_tokenize_line(text, start, end, tokens, MAX_HIGHLIGHT_TOKENS);
    }

    if (token_count == 0) {
        render_plain_segment(font, text, start, end, config->default_color, x, y);
        return;
    }

    int cursor_x = x;
    for (int i = 0; i < token_count; i++) {
        if (tokens[i].start >= end || tokens[i].end <= start) continue;
        int seg_start = tokens[i].start < start ? start : tokens[i].start;
        int seg_end = tokens[i].end > end ? end : tokens[i].end;
        if (seg_start >= seg_end) continue;

        Color color = token_color(config, tokens[i].type);
        render_plain_segment(font, text, seg_start, seg_end, color, cursor_x, y);
        cursor_x += text_syntax_measure_width(font, text, seg_start, seg_end, color);
    }
}
