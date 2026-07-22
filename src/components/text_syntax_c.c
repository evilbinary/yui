#include "text_syntax.h"
#include <ctype.h>
#include <string.h>

static int is_word_char(char c) {
    return isalnum((unsigned char)c) || c == '_';
}

static int is_ident_start(char c) {
    return isalpha((unsigned char)c) || c == '_';
}

static const char* const C_KEYWORDS[] = {
    "auto", "break", "case", "const", "continue", "default", "do", "else",
    "enum", "extern", "for", "goto", "if", "inline", "register", "restrict",
    "return", "sizeof", "static", "struct", "switch", "typedef", "union",
    "volatile", "while", "_Alignas", "_Alignof", "_Atomic", "_Bool",
    "_Complex", "_Generic", "_Imaginary", "_Noreturn", "_Static_assert",
    "_Thread_local", NULL
};

static const char* const C_TYPES[] = {
    "void", "char", "short", "int", "long", "float", "double", "signed",
    "unsigned", "size_t", "ssize_t", "ptrdiff_t", "intptr_t", "uintptr_t",
    "int8_t", "int16_t", "int32_t", "int64_t",
    "uint8_t", "uint16_t", "uint32_t", "uint64_t",
    "bool", "true", "false", "NULL", NULL
};

static int match_word_list(const char* text, int start, int end, const char* const* words) {
    int len = end - start;
    if (len <= 0) return 0;
    for (int i = 0; words[i]; i++) {
        int wlen = (int)strlen(words[i]);
        if (wlen == len && strncmp(text + start, words[i], (size_t)len) == 0) {
            return 1;
        }
    }
    return 0;
}

static int c_tokenize_line(const char* text, int start, int end, TextHlToken* out, int max_out, void* ctx) {
    (void)ctx;
    int count = 0;
    int pos = start;

    while (pos < end && count < max_out) {
        char c = text[pos];

        if (c == ' ' || c == '\t' || c == '\r') {
            int ws = pos;
            while (pos < end && (text[pos] == ' ' || text[pos] == '\t' || text[pos] == '\r')) {
                pos++;
            }
            out[count++] = (TextHlToken){ws, pos, TEXT_HL_DEFAULT};
            continue;
        }

        /* // line comment */
        if (c == '/' && pos + 1 < end && text[pos + 1] == '/') {
            out[count++] = (TextHlToken){pos, end, TEXT_HL_COMMENT};
            break;
        }

        /* block comment (within this line) */
        if (c == '/' && pos + 1 < end && text[pos + 1] == '*') {
            int s = pos;
            pos += 2;
            while (pos + 1 < end && !(text[pos] == '*' && text[pos + 1] == '/')) {
                pos++;
            }
            if (pos + 1 < end) pos += 2;
            else pos = end;
            out[count++] = (TextHlToken){s, pos, TEXT_HL_COMMENT};
            continue;
        }

        /* preprocessor */
        if (c == '#') {
            int s = pos;
            pos++;
            while (pos < end && (text[pos] == ' ' || text[pos] == '\t')) pos++;
            while (pos < end && is_word_char(text[pos])) pos++;
            out[count++] = (TextHlToken){s, pos, TEXT_HL_PREPROC};
            continue;
        }

        /* string / char literal */
        if (c == '"' || c == '\'') {
            char quote = c;
            int s = pos++;
            while (pos < end) {
                if (text[pos] == '\\' && pos + 1 < end) {
                    pos += 2;
                    continue;
                }
                if (text[pos] == quote) {
                    pos++;
                    break;
                }
                pos++;
            }
            out[count++] = (TextHlToken){s, pos, TEXT_HL_STRING};
            continue;
        }

        /* number */
        if (isdigit((unsigned char)c) ||
            (c == '.' && pos + 1 < end && isdigit((unsigned char)text[pos + 1]))) {
            int s = pos++;
            while (pos < end && (isdigit((unsigned char)text[pos]) || text[pos] == '.' ||
                                 text[pos] == 'x' || text[pos] == 'X' ||
                                 text[pos] == 'b' || text[pos] == 'B' ||
                                 (text[pos] >= 'a' && text[pos] <= 'f') ||
                                 (text[pos] >= 'A' && text[pos] <= 'F') ||
                                 text[pos] == 'u' || text[pos] == 'U' ||
                                 text[pos] == 'l' || text[pos] == 'L')) {
                pos++;
            }
            out[count++] = (TextHlToken){s, pos, TEXT_HL_NUMBER};
            continue;
        }

        /* identifier / keyword / type */
        if (is_ident_start(c)) {
            int s = pos++;
            while (pos < end && is_word_char(text[pos])) pos++;
            TextHlKind kind = TEXT_HL_DEFAULT;
            if (match_word_list(text, s, pos, C_KEYWORDS)) kind = TEXT_HL_KEYWORD;
            else if (match_word_list(text, s, pos, C_TYPES)) kind = TEXT_HL_TYPE;
            out[count++] = (TextHlToken){s, pos, kind};
            continue;
        }

        /* punctuation / operators */
        if (strchr("{}[]();,.+-*/%<>=!&|^~?:", c)) {
            int s = pos++;
            /* multi-char operators */
            if (pos < end) {
                char n = text[pos];
                if ((c == '=' && n == '=') || (c == '!' && n == '=') ||
                    (c == '<' && (n == '=' || n == '<')) ||
                    (c == '>' && (n == '=' || n == '>')) ||
                    (c == '&' && n == '&') || (c == '|' && n == '|') ||
                    (c == '+' && n == '+') || (c == '-' && n == '-') ||
                    (c == '+' && n == '=') || (c == '-' && n == '=') ||
                    (c == '*' && n == '=') || (c == '/' && n == '=') ||
                    (c == '%' && n == '=') || (c == '&' && n == '=') ||
                    (c == '|' && n == '=') || (c == '^' && n == '=') ||
                    (c == '-' && n == '>')) {
                    pos++;
                }
            }
            out[count++] = (TextHlToken){s, pos, TEXT_HL_PUNCTUATION};
            continue;
        }

        out[count++] = (TextHlToken){pos, pos + 1, TEXT_HL_DEFAULT};
        pos++;
    }

    return count;
}

static const char* const c_aliases[] = { "h", NULL };

static const TextSyntaxLang g_lang_c = {
    "c", c_aliases, c_tokenize_line, NULL, NULL
};

void text_syntax_lang_c_register(void) {
    text_syntax_register(&g_lang_c);
}
