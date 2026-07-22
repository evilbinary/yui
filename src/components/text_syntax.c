#include "../ytype.h"
#include "text_syntax.h"
#include "../backend.h"
#include "../util.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* Keep old names as aliases during tokenizer migration */
typedef TextHlToken HighlightToken;
typedef TextHlKind HighlightTokenType;
#define HL_TOKEN_DEFAULT      TEXT_HL_DEFAULT
#define HL_TOKEN_KEY          TEXT_HL_KEY
#define HL_TOKEN_STRING       TEXT_HL_STRING
#define HL_TOKEN_NUMBER       TEXT_HL_NUMBER
#define HL_TOKEN_KEYWORD      TEXT_HL_KEYWORD
#define HL_TOKEN_PUNCTUATION  TEXT_HL_PUNCTUATION
#define HL_TOKEN_COMMENT      TEXT_HL_COMMENT
#define HL_TOKEN_HEADING      TEXT_HL_HEADING
#define HL_TOKEN_CODE         TEXT_HL_CODE
#define MAX_HIGHLIGHT_TOKENS  TEXT_SYNTAX_MAX_TOKENS

static int syntax_utf8_char_len(const char* text, int pos, int end) {
    int len;

    if (pos >= end) {
        return 0;
    }
    len = utf8_char_len_at(text + pos);
    if (pos + len > end) {
        len = end - pos;
    }
    return len;
}

static int syntax_append_default_token(HighlightToken tokens[], int* count, int max_tokens,
                                       const char* text, int pos, int end) {
    int clen = syntax_utf8_char_len(text, pos, end);
    if (clen <= 0 || *count >= max_tokens) return pos + (clen > 0 ? clen : 1);
    tokens[(*count)++] = (HighlightToken){pos, pos + clen, HL_TOKEN_DEFAULT};
    return pos + clen;
}

void text_syntax_config_init(TextSyntaxConfig* config, const char* language, Color default_color) {
    if (!config) return;
    text_syntax_ensure_init();
    memset(config, 0, sizeof(TextSyntaxConfig));
    config->lang = text_syntax_find(language);
    config->default_color = default_color;
    config->comment_color = (Color){108, 112, 134, 255};
    config->punctuation_color = (Color){137, 220, 235, 255};
    config->number_color = (Color){250, 179, 135, 255};
    config->keyword_color = (Color){203, 166, 247, 255};
    config->string_color = (Color){166, 227, 161, 255};
    config->key_color = (Color){137, 180, 250, 255};
    if (config->lang && config->lang->init_colors) {
        config->lang->init_colors(config, default_color);
    }
}

void text_syntax_config_set_color(TextSyntaxConfig* config, const char* name, Color color) {
    if (!config || !name) return;
    if (strcmp(name, "key") == 0) config->key_color = color;
    else if (strcmp(name, "string") == 0) config->string_color = color;
    else if (strcmp(name, "number") == 0) config->number_color = color;
    else if (strcmp(name, "keyword") == 0) config->keyword_color = color;
    else if (strcmp(name, "punctuation") == 0) config->punctuation_color = color;
    else if (strcmp(name, "comment") == 0) config->comment_color = color;
    else if (strcmp(name, "code") == 0) config->key_color = color;
    else if (strcmp(name, "heading") == 0) config->keyword_color = color;
    else if (strcmp(name, "link") == 0) config->key_color = color;
    else if (strcmp(name, "default") == 0) config->default_color = color;
}

static Color token_color(const TextSyntaxConfig* config, HighlightTokenType type) {
    switch (type) {
        case HL_TOKEN_KEY: return config->key_color;
        case HL_TOKEN_STRING: return config->string_color;
        case HL_TOKEN_NUMBER: return config->number_color;
        case HL_TOKEN_KEYWORD: return config->keyword_color;
        case HL_TOKEN_PUNCTUATION: return config->punctuation_color;
        case HL_TOKEN_COMMENT: return config->comment_color;
        case HL_TOKEN_HEADING: return config->keyword_color;
        case HL_TOKEN_CODE: return config->key_color;
        case TEXT_HL_TYPE: return config->keyword_color;
        case TEXT_HL_PREPROC: return config->key_color;
        default: return config->default_color;
    }
}

static int is_word_char(char c) {
    return isalnum((unsigned char)c) || c == '_';
}

static int is_identifier_start(char c) {
    return isalpha((unsigned char)c) || c == '_';
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

static int json_tokenize_line(const char* text, int start, int end, HighlightToken tokens[], int max_tokens, void* ctx) {
    (void)ctx;
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

        pos = syntax_append_default_token(tokens, &count, max_tokens, text, pos, end);
    }

    return count;
}

static const char* SQL_KEYWORDS[] = {
    "SELECT", "FROM", "WHERE", "INSERT", "UPDATE", "DELETE", "CREATE", "DROP", "ALTER",
    "TABLE", "INDEX", "VIEW", "JOIN", "INNER", "LEFT", "RIGHT", "OUTER", "FULL", "CROSS",
    "ON", "AS", "AND", "OR", "NOT", "IN", "IS", "NULL", "LIKE", "BETWEEN", "GROUP", "BY",
    "ORDER", "HAVING", "LIMIT", "OFFSET", "DISTINCT", "UNION", "ALL", "SET", "VALUES",
    "INTO", "PRIMARY", "KEY", "FOREIGN", "REFERENCES", "CONSTRAINT", "DEFAULT", "AUTO_INCREMENT",
    "COUNT", "SUM", "AVG", "MIN", "MAX", "CASE", "WHEN", "THEN", "ELSE", "END",
    "TRUE", "FALSE", "EXISTS", "ASC", "DESC", "DATABASE", "SCHEMA", "SHOW", "USE",
    "GRANT", "REVOKE", "BEGIN", "COMMIT", "ROLLBACK", "TRANSACTION", "IF", "REPLACE",
    "TRUNCATE", "EXEC", "EXECUTE", "VARCHAR", "INT", "INTEGER", "TEXT", "BLOB", "DATE",
    "DATETIME", "TIMESTAMP", "BOOLEAN", "DECIMAL", "FLOAT", "DOUBLE", "UNSIGNED", "ZEROFILL",
    "ENGINE", "CHARSET", "COLLATE", "COMMENT", "WITH", "RECURSIVE", "OVER", "PARTITION",
    "WINDOW", "ROW", "ROWS", "RANGE", "UNBOUNDED", "PRECEDING", "FOLLOWING", "CURRENT",
    "NATURAL", "USING", "DUAL", "EXPLAIN", "DESCRIBE", "DESC", "PROCEDURE", "FUNCTION",
    "TRIGGER", "EVENT", "LOCK", "UNLOCK", "TABLES", "DATABASES", "COLUMN", "COLUMNS",
    "ADD", "MODIFY", "CHANGE", "RENAME", "TO", "FIRST", "AFTER", "IGNORE", "DUPLICATE",
    "DELAYED", "HIGH_PRIORITY", "LOW_PRIORITY", "SQL", "CALC_FOUND_ROWS", "STRAIGHT_JOIN",
    "FOR", "SHARE", "NOWAIT", "SKIP", "LOCKED", "LATERAL", "CASCADE", "RESTRICT", "NO",
    "ACTION", "MATCH", "FULL", "PARTIAL", "SIMPLE", "LOCALTIME", "LOCALTIMESTAMP",
    "UTC_DATE", "UTC_TIME", "UTC_TIMESTAMP", "INTERVAL", "YEAR", "MONTH", "DAY", "HOUR",
    "MINUTE", "SECOND", "MICROSECOND", "XOR", "REGEXP", "RLIKE", "DIV", "MOD", "BINARY",
    NULL
};

static int strncasecmp_ascii(const char* a, const char* b, int n) {
    for (int i = 0; i < n; i++) {
        unsigned char ca = (unsigned char)a[i];
        unsigned char cb = (unsigned char)b[i];
        if (tolower(ca) != tolower(cb)) return (int)tolower(ca) - (int)tolower(cb);
        if (ca == '\0') return 0;
    }
    return 0;
}

static int is_sql_keyword(const char* text, int start, int end) {
    int len = end - start;
    if (len <= 0) return 0;
    for (int i = 0; SQL_KEYWORDS[i]; i++) {
        const char* kw = SQL_KEYWORDS[i];
        int kw_len = (int)strlen(kw);
        if (kw_len == len && strncasecmp_ascii(text + start, kw, len) == 0) {
            return 1;
        }
    }
    return 0;
}

static int sql_tokenize_line(const char* text, int start, int end, HighlightToken tokens[], int max_tokens, void* ctx) {
    (void)ctx;
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

        if (c == '-' && pos + 1 < end && text[pos + 1] == '-') {
            tokens[count++] = (HighlightToken){pos, end, HL_TOKEN_COMMENT};
            break;
        }

        if (c == '#') {
            tokens[count++] = (HighlightToken){pos, end, HL_TOKEN_COMMENT};
            break;
        }

        if (c == '/' && pos + 1 < end && text[pos + 1] == '*') {
            int comment_start = pos;
            pos += 2;
            while (pos + 1 < end && !(text[pos] == '*' && text[pos + 1] == '/')) {
                pos++;
            }
            if (pos + 1 < end) pos += 2;
            tokens[count++] = (HighlightToken){comment_start, pos, HL_TOKEN_COMMENT};
            continue;
        }

        if (c == '\'' || c == '"') {
            char quote = c;
            int str_start = pos;
            pos++;
            while (pos < end) {
                if (text[pos] == quote) {
                    if (pos + 1 < end && text[pos + 1] == quote) {
                        pos += 2;
                        continue;
                    }
                    pos++;
                    break;
                }
                if (text[pos] == '\\' && pos + 1 < end) {
                    pos += 2;
                    continue;
                }
                pos++;
            }
            tokens[count++] = (HighlightToken){str_start, pos, HL_TOKEN_STRING};
            continue;
        }

        if (c == '`') {
            int str_start = pos;
            pos++;
            while (pos < end && text[pos] != '`') {
                pos++;
            }
            if (pos < end) pos++;
            tokens[count++] = (HighlightToken){str_start, pos, HL_TOKEN_STRING};
            continue;
        }

        if (isdigit((unsigned char)c) || (c == '.' && pos + 1 < end && isdigit((unsigned char)text[pos + 1]))) {
            int num_start = pos;
            if (c == '.') pos++;
            while (pos < end && isdigit((unsigned char)text[pos])) pos++;
            if (pos < end && text[pos] == '.' && pos + 1 < end && isdigit((unsigned char)text[pos + 1])) {
                pos++;
                while (pos < end && isdigit((unsigned char)text[pos])) pos++;
            }
            if (pos < end && (text[pos] == 'e' || text[pos] == 'E')) {
                pos++;
                if (pos < end && (text[pos] == '+' || text[pos] == '-')) pos++;
                while (pos < end && isdigit((unsigned char)text[pos])) pos++;
            }
            tokens[count++] = (HighlightToken){num_start, pos, HL_TOKEN_NUMBER};
            continue;
        }

        if (is_identifier_start(c)) {
            int id_start = pos;
            while (pos < end && is_word_char(text[pos])) pos++;
            HighlightTokenType type = is_sql_keyword(text, id_start, pos) ? HL_TOKEN_KEYWORD : HL_TOKEN_DEFAULT;
            tokens[count++] = (HighlightToken){id_start, pos, type};
            continue;
        }

        if (c == '(' || c == ')' || c == ',' || c == ';' || c == '.') {
            tokens[count++] = (HighlightToken){pos, pos + 1, HL_TOKEN_PUNCTUATION};
            pos++;
            continue;
        }

        if (c == '=' || c == '+' || c == '-' || c == '*' || c == '/' || c == '%' ||
            c == '<' || c == '>' || c == '!' || c == '|' || c == '&' || c == '^' || c == '~') {
            int op_start = pos;
            pos++;
            if (pos < end) {
                char n = text[pos];
                if ((c == '<' && (n == '=' || n == '>' || n == '<')) ||
                    (c == '>' && (n == '=' || n == '>')) ||
                    (c == '!' && n == '=') ||
                    (c == '=' && n == '=') ||
                    (c == '|' && n == '|') ||
                    (c == '&' && n == '&') ||
                    (c == '-' && n == '>')) {
                    pos++;
                }
            }
            tokens[count++] = (HighlightToken){op_start, pos, HL_TOKEN_PUNCTUATION};
            continue;
        }

        pos = syntax_append_default_token(tokens, &count, max_tokens, text, pos, end);
    }

    return count;
}

static int md_line_first_nonspace(const char* text, int start, int end) {
    int p = start;
    while (p < end && (text[p] == ' ' || text[p] == '\t')) p++;
    return p;
}

static int md_is_hr_line(const char* text, int first, int end) {
    if (first + 2 >= end) return 0;
    char c = text[first];
    if (c != '-' && c != '*' && c != '_') return 0;
    if (text[first + 1] != c || text[first + 2] != c) return 0;
    for (int i = first; i < end; i++) {
        if (text[i] != c && text[i] != ' ' && text[i] != '\t') return 0;
    }
    return 1;
}

static int md_try_delimited_span(const char* text, int end, int* pos, char open, char close,
                                 HighlightTokenType content_type, HighlightTokenType marker_type,
                                 HighlightToken tokens[], int* count, int max_tokens) {
    int mark = *pos;
    (*pos)++;
    int content = *pos;
    while (*pos < end && text[*pos] != close) (*pos)++;
    if (*pos >= end) {
        *pos = mark + 1;
        return 0;
    }
    if (*count + 3 > max_tokens) return 0;
    tokens[(*count)++] = (HighlightToken){mark, mark + 1, marker_type};
    tokens[(*count)++] = (HighlightToken){content, *pos, content_type};
    tokens[(*count)++] = (HighlightToken){*pos, *pos + 1, marker_type};
    (*pos)++;
    return 1;
}

static int markdown_tokenize_line(const char* text, int start, int end, HighlightToken tokens[], int max_tokens, void* ctx) {
    (void)ctx;
    int count = 0;
    int first = md_line_first_nonspace(text, start, end);

    if (first < end && text[first] == '#') {
        int h = first;
        while (h < end && text[h] == '#') h++;
        if (h < end && (text[h] == ' ' || text[h] == '\t')) {
            tokens[count++] = (HighlightToken){first, end, HL_TOKEN_HEADING};
            return count;
        }
    }
    if (first < end && text[first] == '>') {
        tokens[count++] = (HighlightToken){first, end, HL_TOKEN_COMMENT};
        return count;
    }
    if (first + 2 < end && text[first] == '`' && text[first + 1] == '`' && text[first + 2] == '`') {
        tokens[count++] = (HighlightToken){first, end, HL_TOKEN_CODE};
        return count;
    }
    if (first > start) {
        int spaces = 0;
        for (int i = start; i < first; i++) {
            if (text[i] == ' ') spaces++;
        }
        if (spaces >= 4) {
            tokens[count++] = (HighlightToken){start, end, HL_TOKEN_CODE};
            return count;
        }
    }
    if (md_is_hr_line(text, first, end)) {
        tokens[count++] = (HighlightToken){first, end, HL_TOKEN_PUNCTUATION};
        return count;
    }

    int pos = start;
    if (first < end && (text[first] == '-' || text[first] == '*' || text[first] == '+') &&
        first + 1 < end && text[first + 1] == ' ') {
        tokens[count++] = (HighlightToken){first, first + 2, HL_TOKEN_PUNCTUATION};
        pos = first + 2;
    } else if (first < end && isdigit((unsigned char)text[first])) {
        int d = first;
        while (d < end && isdigit((unsigned char)text[d])) d++;
        if (d < end && text[d] == '.' && d + 1 < end && text[d + 1] == ' ') {
            tokens[count++] = (HighlightToken){first, d + 2, HL_TOKEN_PUNCTUATION};
            pos = d + 2;
        }
    }

    while (pos < end && count < max_tokens) {
        char c = text[pos];

        if (c == ' ' || c == '\t' || c == '\r') {
            int ws = pos;
            while (pos < end && (text[pos] == ' ' || text[pos] == '\t' || text[pos] == '\r')) pos++;
            tokens[count++] = (HighlightToken){ws, pos, HL_TOKEN_DEFAULT};
            continue;
        }

        if (pos + 3 < end && text[pos] == '<' && text[pos + 1] == '!' &&
            text[pos + 2] == '-' && text[pos + 3] == '-') {
            int cs = pos;
            pos += 4;
            while (pos + 2 < end && !(text[pos] == '-' && text[pos + 1] == '-' && text[pos + 2] == '>')) pos++;
            if (pos + 2 < end) pos += 3;
            tokens[count++] = (HighlightToken){cs, pos, HL_TOKEN_COMMENT};
            continue;
        }

        if (c == '`') {
            int tick_start = pos;
            int ticks = 0;
            while (pos < end && text[pos] == '`') {
                ticks++;
                pos++;
            }
            int close = pos;
            while (close < end) {
                if (text[close] == '`') {
                    int matched = 0;
                    int p2 = close;
                    while (p2 < end && text[p2] == '`' && matched < ticks) {
                        matched++;
                        p2++;
                    }
                    if (matched == ticks) {
                        tokens[count++] = (HighlightToken){tick_start, p2, HL_TOKEN_CODE};
                        pos = p2;
                        goto md_next;
                    }
                }
                close++;
            }
            tokens[count++] = (HighlightToken){tick_start, end, HL_TOKEN_CODE};
            break;
        }

        if (pos + 1 < end && c == '*' && text[pos + 1] == '*') {
            int mark = pos;
            pos += 2;
            int content = pos;
            while (pos + 1 < end && !(text[pos] == '*' && text[pos + 1] == '*')) pos++;
            if (pos + 1 < end) {
                tokens[count++] = (HighlightToken){mark, mark + 2, HL_TOKEN_PUNCTUATION};
                tokens[count++] = (HighlightToken){content, pos, HL_TOKEN_KEYWORD};
                tokens[count++] = (HighlightToken){pos, pos + 2, HL_TOKEN_PUNCTUATION};
                pos += 2;
                continue;
            }
            pos = mark;
        }

        if (pos + 1 < end && c == '_' && text[pos + 1] == '_') {
            int mark = pos;
            pos += 2;
            int content = pos;
            while (pos + 1 < end && !(text[pos] == '_' && text[pos + 1] == '_')) pos++;
            if (pos + 1 < end) {
                tokens[count++] = (HighlightToken){mark, mark + 2, HL_TOKEN_PUNCTUATION};
                tokens[count++] = (HighlightToken){content, pos, HL_TOKEN_KEYWORD};
                tokens[count++] = (HighlightToken){pos, pos + 2, HL_TOKEN_PUNCTUATION};
                pos += 2;
                continue;
            }
            pos = mark;
        }

        if (pos + 1 < end && c == '~' && text[pos + 1] == '~') {
            int mark = pos;
            pos += 2;
            int content = pos;
            while (pos + 1 < end && !(text[pos] == '~' && text[pos + 1] == '~')) pos++;
            if (pos + 1 < end) {
                tokens[count++] = (HighlightToken){mark, mark + 2, HL_TOKEN_PUNCTUATION};
                tokens[count++] = (HighlightToken){content, pos, HL_TOKEN_COMMENT};
                tokens[count++] = (HighlightToken){pos, pos + 2, HL_TOKEN_PUNCTUATION};
                pos += 2;
                continue;
            }
            pos = mark;
        }

        if (c == '*' && (pos + 1 >= end || text[pos + 1] != '*')) {
            if (md_try_delimited_span(text, end, &pos, '*', '*', HL_TOKEN_STRING, HL_TOKEN_PUNCTUATION,
                                      tokens, &count, max_tokens)) {
                continue;
            }
        }

        if (c == '_' && (pos + 1 >= end || text[pos + 1] != '_')) {
            if (md_try_delimited_span(text, end, &pos, '_', '_', HL_TOKEN_STRING, HL_TOKEN_PUNCTUATION,
                                      tokens, &count, max_tokens)) {
                continue;
            }
        }

        if (c == '!' && pos + 1 < end && text[pos + 1] == '[') {
            int ls = pos;
            pos += 2;
            int label_start = pos;
            while (pos < end && text[pos] != ']') pos++;
            if (pos < end) {
                int label_end = pos++;
                if (pos < end && text[pos] == '(') {
                    pos++;
                    int url_start = pos;
                    while (pos < end && text[pos] != ')') pos++;
                    int url_end = pos;
                    if (pos < end) pos++;
                    if (count + 5 <= max_tokens) {
                        tokens[count++] = (HighlightToken){ls, ls + 2, HL_TOKEN_PUNCTUATION};
                        tokens[count++] = (HighlightToken){label_start, label_end, HL_TOKEN_STRING};
                        tokens[count++] = (HighlightToken){label_end, label_end + 1, HL_TOKEN_PUNCTUATION};
                        tokens[count++] = (HighlightToken){url_start, url_end, HL_TOKEN_KEY};
                        if (url_end < end) {
                            tokens[count++] = (HighlightToken){url_end, url_end + 1, HL_TOKEN_PUNCTUATION};
                        }
                        continue;
                    }
                }
            }
            pos = ls + 1;
        }

        if (c == '[') {
            int ls = pos++;
            int label_start = pos;
            while (pos < end && text[pos] != ']') pos++;
            if (pos < end) {
                int label_end = pos++;
                if (pos < end && text[pos] == '(') {
                    pos++;
                    int url_start = pos;
                    while (pos < end && text[pos] != ')') pos++;
                    int url_end = pos;
                    if (pos < end) pos++;
                    if (count + 5 <= max_tokens) {
                        tokens[count++] = (HighlightToken){ls, ls + 1, HL_TOKEN_PUNCTUATION};
                        tokens[count++] = (HighlightToken){label_start, label_end, HL_TOKEN_STRING};
                        tokens[count++] = (HighlightToken){label_end, label_end + 1, HL_TOKEN_PUNCTUATION};
                        tokens[count++] = (HighlightToken){url_start, url_end, HL_TOKEN_KEY};
                        tokens[count++] = (HighlightToken){url_end, url_end + 1, HL_TOKEN_PUNCTUATION};
                        continue;
                    }
                }
            }
            pos = ls + 1;
        }

        pos = syntax_append_default_token(tokens, &count, max_tokens, text, pos, end);
        md_next:;
    }

    return count;
}

int text_syntax_measure_width(DFont* font, const char* text, int start, int end, Color color) {
    if (!font || !text || start >= end) return 0;
    int len = end - start;
    char* buf = (char*)malloc((size_t)len + 1);
    int measured;
    Texture* tex;
    int width = 0;
    int height = 0;

    if (!buf) return 0;
    memcpy(buf, text + start, (size_t)len);
    buf[len] = '\0';

    measured = backend_measure_text_width(font, buf);
    if (measured > 0) {
        free(buf);
        return measured;
    }

    tex = backend_render_texture(font, buf, color);
    free(buf);
    if (!tex) return 0;
    backend_query_texture(tex, NULL, NULL, &width, &height);
    backend_render_text_destroy(tex);
    return width / (int)yui_density;
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
    Rect rect = {x, y, width / (int)yui_density, height / (int)yui_density};
    backend_render_text_copy(tex, NULL, &rect);
    backend_render_text_destroy(tex);
}

int text_syntax_measure_range(DFont* font, const char* text, int start, int end,
                              const TextSyntaxConfig* config) {
    HighlightToken tokens[MAX_HIGHLIGHT_TOKENS];
    int token_count = 0;
    int width = 0;

    if (!font || !text || !config || start >= end) return 0;
    if (!config->lang || !config->lang->tokenize_line) {
        return text_syntax_measure_width(font, text, start, end, config->default_color);
    }

    token_count = config->lang->tokenize_line(text, start, end, tokens, MAX_HIGHLIGHT_TOKENS,
                                              config->lang->ctx);

    if (token_count == 0) {
        return text_syntax_measure_width(font, text, start, end, config->default_color);
    }

    for (int i = 0; i < token_count; i++) {
        int seg_start = tokens[i].start < start ? start : tokens[i].start;
        int seg_end = tokens[i].end > end ? end : tokens[i].end;
        if (seg_start < seg_end) {
            width += text_syntax_measure_width(font, text, seg_start, seg_end,
                                               token_color(config, tokens[i].type));
        }
    }
    return width;
}

void text_syntax_render_range(DFont* font, const char* text, int start, int end,
                              const TextSyntaxConfig* config, int x, int y) {
    if (!font || !text || !config || start >= end) return;

    if (!config->lang || !config->lang->tokenize_line) {
        render_plain_segment(font, text, start, end, config->default_color, x, y);
        return;
    }

    HighlightToken tokens[MAX_HIGHLIGHT_TOKENS];
    int token_count = config->lang->tokenize_line(text, start, end, tokens, MAX_HIGHLIGHT_TOKENS,
                                                  config->lang->ctx);

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

/* ---- language registry ---- */

static const TextSyntaxLang* g_langs[TEXT_SYNTAX_MAX_LANGS];
static int g_lang_count = 0;
static int g_syntax_inited = 0;

static const char* const markdown_aliases[] = { "md", NULL };

static const TextSyntaxLang g_lang_json = {
    "json", NULL, json_tokenize_line, NULL, NULL
};
static const TextSyntaxLang g_lang_sql = {
    "sql", NULL, sql_tokenize_line, NULL, NULL
};
static const TextSyntaxLang g_lang_markdown = {
    "markdown", markdown_aliases, markdown_tokenize_line, NULL, NULL
};

/* Optional extra language modules (compiled separately). */
void text_syntax_lang_c_register(void);

int text_syntax_register(const TextSyntaxLang* lang) {
    if (!lang || !lang->name || !lang->tokenize_line) return -1;
    if (text_syntax_find(lang->name)) return 0; /* already registered */
    if (g_lang_count >= TEXT_SYNTAX_MAX_LANGS) return -1;
    g_langs[g_lang_count++] = lang;
    return 0;
}

const TextSyntaxLang* text_syntax_find(const char* name) {
    if (!name || name[0] == '\0') return NULL;
    for (int i = 0; i < g_lang_count; i++) {
        const TextSyntaxLang* lang = g_langs[i];
        if (!lang || !lang->name) continue;
        if (strcmp(lang->name, name) == 0) return lang;
        if (lang->aliases) {
            for (int a = 0; lang->aliases[a]; a++) {
                if (strcmp(lang->aliases[a], name) == 0) return lang;
            }
        }
    }
    return NULL;
}

void text_syntax_ensure_init(void) {
    if (g_syntax_inited) return;
    g_syntax_inited = 1;
    text_syntax_register(&g_lang_json);
    text_syntax_register(&g_lang_sql);
    text_syntax_register(&g_lang_markdown);
    text_syntax_lang_c_register();
}
