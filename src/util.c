#include "util.h"
#include <ctype.h>
#include <float.h> // 添加这个头文件以定义 DBL_EPSILON
#include "ytype.h"

#ifdef _WIN32
#ifndef strtok_r
#define strtok_r strtok_s
#endif
#endif

int is_cjson_float(const cJSON *item) {
    if (item == NULL || !cJSON_IsNumber(item)) {
        return 0;
    }
    
    // 获取双精度值
    double value = item->valuedouble;
    
    // 检查是否为有限数（非无穷大和非NaN）
    if (!isfinite(value)) {
        return 0;
    }
    
    // 检查值是否超出整数范围
    if (value > INT_MAX || value < INT_MIN) {
        return 1; // 超出整数范围，肯定是浮点数
    }
    
    // 检查值是否包含小数部分
    double int_part;
    double frac_part = modf(value, &int_part);
    
    // 如果小数部分的绝对值大于一个很小的容差值，则是浮点数
    if (fabs(frac_part) > DBL_EPSILON) {
        return 1;
    }
    
    return 0; // 是整数
}

// 替换字符串中的占位符
char* replace_placeholder(const char* template, const char* placeholder, const char* value) {
    // 查找占位符位置
    char* pos = strstr(template, placeholder);
    if (pos == NULL) return NULL;
    
    // 计算新字符串长度
    size_t template_len = strlen(template);
    size_t placeholder_len = strlen(placeholder);
    size_t value_len = strlen(value);
    size_t new_len = template_len - placeholder_len + value_len + 1;
    
    // 分配内存
    char* result = (char*)malloc(new_len);
    if (result == NULL) return NULL;
    
    // 复制第一部分
    size_t prefix_len = pos - template;
    strncpy(result, template, prefix_len);
    result[prefix_len] = '\0';
    
    // 追加替换值
    strcat(result, value);
    
    // 追加剩余部分
    strcat(result, pos + placeholder_len);
    
    return result;
}

// 替换所有出现的占位符
char* replace_all_placeholders(const char* template, const char* placeholder, const char* value) {
    char* result = strdup(template);
    if (result == NULL) return NULL;
    
    char* pos = strstr(result, placeholder);
    while (pos != NULL) {
        // 计算新字符串长度
        size_t result_len = strlen(result);
        size_t placeholder_len = strlen(placeholder);
        size_t value_len = strlen(value);
        size_t new_len = result_len - placeholder_len + value_len + 1;
        
        // 分配临时内存
        char* temp = (char*)malloc(new_len);
        if (temp == NULL) {
            free(result);
            return NULL;
        }
        
        // 复制第一部分
        size_t prefix_len = pos - result;
        strncpy(temp, result, prefix_len);
        temp[prefix_len] = '\0';
        
        // 追加替换值
        strcat(temp, value);
        
        // 追加剩余部分
        strcat(temp, pos + placeholder_len);
        
        // 释放旧结果，使用新结果
        free(result);
        result = temp;
        
        // 查找下一个占位符
        pos = strstr(result, placeholder);
    }
    
    return result;
}



// 辅助函数：将十六进制字符转换为数值
static int hex_to_nibble(char c) {
    c = tolower(c);
    if (c >= '0' && c <= '9') {
        return c - '0';
    } else if (c >= 'a' && c <= 'f') {
        return 10 + c - 'a';
    }
    return 0; // 默认返回0
}

// 将十六进制颜色字符串转换为Color结构体
Color color_from_hex(const char* hex) {
    Color color = {0, 0, 0, 255}; // 默认为黑色，完全不透明
    
    if (!hex) return color;
    
    // 跳过开头的'#'字符（如果有）
    if (hex[0] == '#') {
        hex++;
    }
    
    size_t len = strlen(hex);
    
    // 支持3位、6位和8位十六进制颜色
    if (len == 3) {
        // 3位十六进制颜色（如"FFF"）
        color.r = (hex_to_nibble(hex[0]) << 4) | hex_to_nibble(hex[0]);
        color.g = (hex_to_nibble(hex[1]) << 4) | hex_to_nibble(hex[1]);
        color.b = (hex_to_nibble(hex[2]) << 4) | hex_to_nibble(hex[2]);
    } else if (len == 6) {
        // 6位十六进制颜色（如"FFFFFF"）
        color.r = (hex_to_nibble(hex[0]) << 4) | hex_to_nibble(hex[1]);
        color.g = (hex_to_nibble(hex[2]) << 4) | hex_to_nibble(hex[3]);
        color.b = (hex_to_nibble(hex[4]) << 4) | hex_to_nibble(hex[5]);
    } else if (len == 8) {
        // 8位十六进制颜色（如"FFFFFFFF"）
        color.r = (hex_to_nibble(hex[0]) << 4) | hex_to_nibble(hex[1]);
        color.g = (hex_to_nibble(hex[2]) << 4) | hex_to_nibble(hex[3]);
        color.b = (hex_to_nibble(hex[4]) << 4) | hex_to_nibble(hex[5]);
        color.a = (hex_to_nibble(hex[6]) << 4) | hex_to_nibble(hex[7]);
    }
    
    return color;
}

// 检查点是否在矩形内
int is_point_in_rect(int x, int y, Rect rect) {
    return x >= rect.x && x < rect.x + rect.w &&
           y >= rect.y && y < rect.y + rect.h;
}

int point_in_rect(Point pt, Rect rect) {
    return is_point_in_rect(pt.x, pt.y, rect);
}

void parse_color(char* valuestring, Color* color) {
    // 支持多种颜色格式
    if (strcmp(valuestring, "transparent") == 0) {
        color->r = 0;
        color->g = 0;
        color->b = 0;
        color->a = 0;
    } else if (strncmp(valuestring, "rgba(", 5) == 0) {
      // 解析 rgba(r, g, b, a) 格式，支持空格；a 可为 0-1 或 0-255
      int r, g, b;
      float af = 255.0f;
      if (sscanf(valuestring, "rgba(%d ,%d ,%d ,%f)", &r, &g, &b, &af) == 4 ||
          sscanf(valuestring, "rgba(%d,%d,%d,%f)", &r, &g, &b, &af) == 4) {
        color->r = (unsigned char)r;
        color->g = (unsigned char)g;
        color->b = (unsigned char)b;
        if (af <= 1.0f) {
          color->a = (unsigned char)(af * 255.0f + 0.5f);
        } else {
          color->a = (unsigned char)af;
        }
      }
    } else if (strncmp(valuestring, "rgb(", 4) == 0) {
      // 解析 rgb(r, g, b) 格式，支持空格
      int r, g, b;
      sscanf(valuestring, "rgb(%d,%d,%d)", &r, &g, &b);
      color->r = (unsigned char)r;
      color->g = (unsigned char)g;
      color->b = (unsigned char)b;
      color->a = 255;  // 默认不透明
    } else if (strlen(valuestring) == 9) {
      // 解析十六进制 #RRGGBBAA 格式
      sscanf(valuestring, "#%02hhx%02hhx%02hhx%02hhx", &color->r, &color->g,
             &color->b, &color->a);
    } else {
      // 解析十六进制 #RRGGBB 格式
      sscanf(valuestring, "#%02hhx%02hhx%02hhx", &color->r, &color->g, &color->b);
      color->a = 255;  // 默认不透明
    }
}

static int parse_shadow_length_token(const char* tok, int* out) {
    char buf[64];
    size_t n;
    char* end = NULL;
    long v;
    if (!tok || !out) return 0;
    n = strlen(tok);
    if (n == 0 || n >= sizeof(buf)) return 0;
    memcpy(buf, tok, n + 1);
    if (n > 2 && (buf[n - 1] == 'x' || buf[n - 1] == 'X') &&
        (buf[n - 2] == 'p' || buf[n - 2] == 'P')) {
        buf[n - 2] = '\0';
    }
    v = strtol(buf, &end, 10);
    if (end == buf) return 0;
    *out = (int)v;
    return 1;
}

static int token_looks_like_color(const char* tok) {
    if (!tok || !tok[0]) return 0;
    if (tok[0] == '#') return 1;
    if (strncmp(tok, "rgb", 3) == 0) return 1;
    if (strcmp(tok, "transparent") == 0) return 1;
    return 0;
}

int parse_layer_shadow(cJSON* value, LayerShadow* out) {
    if (!out) return 0;
    memset(out, 0, sizeof(*out));
    out->color.a = 64;

    if (!value) return 0;

    if (cJSON_IsObject(value)) {
        cJSON* x = cJSON_GetObjectItem(value, "offsetX");
        if (!x) x = cJSON_GetObjectItem(value, "x");
        cJSON* y = cJSON_GetObjectItem(value, "offsetY");
        if (!y) y = cJSON_GetObjectItem(value, "y");
        cJSON* blur = cJSON_GetObjectItem(value, "blur");
        if (!blur) blur = cJSON_GetObjectItem(value, "blurRadius");
        cJSON* spread = cJSON_GetObjectItem(value, "spread");
        cJSON* color = cJSON_GetObjectItem(value, "color");
        if (x && cJSON_IsNumber(x)) out->offset_x = x->valueint;
        if (y && cJSON_IsNumber(y)) out->offset_y = y->valueint;
        if (blur && cJSON_IsNumber(blur)) out->blur = blur->valueint;
        if (spread && cJSON_IsNumber(spread)) out->spread = spread->valueint;
        if (color && cJSON_IsString(color)) parse_color(color->valuestring, &out->color);
        out->enabled = 1;
        return 1;
    }

    if (cJSON_IsString(value) && value->valuestring) {
        char* dup = strdup(value->valuestring);
        char* tokens[16];
        int n = 0;
        char* p = dup;
        char* save = NULL;
        char* tok;
        int nums[4];
        int num_count = 0;
        if (!dup) return 0;

        /* rgba(...) 可能含空格：先把 rgba( 整段合并较麻烦，要求颜色无空格或用 #hex */
        for (tok = strtok_r(p, " \t", &save); tok && n < 16; tok = strtok_r(NULL, " \t", &save)) {
            tokens[n++] = tok;
        }
        if (n < 5) {
            free(dup);
            return 0;
        }
        /* 末尾为颜色，前面最多 4 个长度值 */
        if (!token_looks_like_color(tokens[n - 1])) {
            free(dup);
            return 0;
        }
        parse_color(tokens[n - 1], &out->color);
        for (int i = 0; i < n - 1 && num_count < 4; i++) {
            int v = 0;
            if (!parse_shadow_length_token(tokens[i], &v)) {
                free(dup);
                return 0;
            }
            nums[num_count++] = v;
        }
        if (num_count != 4) {
            free(dup);
            return 0;
        }
        out->offset_x = nums[0];
        out->offset_y = nums[1];
        out->blur = nums[2];
        out->spread = nums[3];
        out->enabled = 1;
        free(dup);
        return 1;
    }

    return 0;
}

static Color gradient_lerp(Color a, Color b, float t) {
    Color c;
    if (t < 0.f) t = 0.f;
    if (t > 1.f) t = 1.f;
    c.r = (unsigned char)(a.r + (b.r - a.r) * t + 0.5f);
    c.g = (unsigned char)(a.g + (b.g - a.g) * t + 0.5f);
    c.b = (unsigned char)(a.b + (b.b - a.b) * t + 0.5f);
    c.a = (unsigned char)(a.a + (b.a - a.a) * t + 0.5f);
    return c;
}

Color layer_gradient_sample(const LayerGradient* g, float t) {
    if (!g || g->count <= 0) {
        Color z = {0, 0, 0, 0};
        return z;
    }
    if (g->count == 1 || t <= 0.f) return g->colors[0];
    if (t >= 1.f) return g->colors[g->count - 1];
    float scaled = t * (float)(g->count - 1);
    int i = (int)scaled;
    float f = scaled - (float)i;
    if (i >= g->count - 1) return g->colors[g->count - 1];
    return gradient_lerp(g->colors[i], g->colors[i + 1], f);
}

int parse_layer_gradient(cJSON* value, LayerGradient* out) {
    if (!out) return 0;
    memset(out, 0, sizeof(*out));
    out->vertical = 1;

    if (!value) return 0;

    if (cJSON_IsObject(value)) {
        cJSON* dir = cJSON_GetObjectItem(value, "direction");
        if (!dir) dir = cJSON_GetObjectItem(value, "orient");
        cJSON* colors = cJSON_GetObjectItem(value, "colors");
        if (dir && cJSON_IsString(dir)) {
            if (strcmp(dir->valuestring, "horizontal") == 0 ||
                strcmp(dir->valuestring, "to-right") == 0 ||
                strcmp(dir->valuestring, "right") == 0) {
                out->vertical = 0;
            }
        }
        if (colors && cJSON_IsArray(colors)) {
            int n = cJSON_GetArraySize(colors);
            for (int i = 0; i < n && out->count < LAYER_GRADIENT_MAX_STOPS; i++) {
                cJSON* c = cJSON_GetArrayItem(colors, i);
                if (c && cJSON_IsString(c)) {
                    parse_color(c->valuestring, &out->colors[out->count++]);
                }
            }
        }
        if (out->count >= 2) {
            out->enabled = 1;
            return 1;
        }
        return 0;
    }

    if (cJSON_IsString(value) && value->valuestring) {
        char* dup = strdup(value->valuestring);
        char* tokens[24];
        int n = 0;
        char* save = NULL;
        char* tok;
        int i = 0;
        if (!dup) return 0;

        /* 简化 linear-gradient(to bottom, #a, #b) */
        if (strncmp(dup, "linear-gradient(", 16) == 0) {
            char* body = dup + 16;
            char* end = strrchr(body, ')');
            if (end) *end = '\0';
            if (strstr(body, "to right") || strstr(body, "90deg") || strstr(body, "to left")) {
                out->vertical = 0;
            } else {
                out->vertical = 1;
            }
            for (tok = strtok_r(body, ",", &save); tok && out->count < LAYER_GRADIENT_MAX_STOPS;
                 tok = strtok_r(NULL, ",", &save)) {
                while (*tok == ' ' || *tok == '\t') tok++;
                if (strncmp(tok, "to ", 3) == 0 || strstr(tok, "deg")) continue;
                parse_color(tok, &out->colors[out->count++]);
            }
            free(dup);
            if (out->count >= 2) {
                out->enabled = 1;
                return 1;
            }
            return 0;
        }

        for (tok = strtok_r(dup, " \t", &save); tok && n < 24; tok = strtok_r(NULL, " \t", &save)) {
            tokens[n++] = tok;
        }
        if (n >= 1 && (strcmp(tokens[0], "linear") == 0 || strcmp(tokens[0], "linear-gradient") == 0)) {
            i = 1;
        }
        if (i < n) {
            if (strcmp(tokens[i], "vertical") == 0 || strcmp(tokens[i], "to-bottom") == 0 ||
                strcmp(tokens[i], "bottom") == 0) {
                out->vertical = 1;
                i++;
            } else if (strcmp(tokens[i], "horizontal") == 0 || strcmp(tokens[i], "to-right") == 0 ||
                       strcmp(tokens[i], "right") == 0) {
                out->vertical = 0;
                i++;
            }
        }
        for (; i < n && out->count < LAYER_GRADIENT_MAX_STOPS; i++) {
            if (!token_looks_like_color(tokens[i])) continue;
            parse_color(tokens[i], &out->colors[out->count++]);
        }
        free(dup);
        if (out->count >= 2) {
            out->enabled = 1;
            return 1;
        }
    }

    return 0;
}

// 获取 UTF-8 字符的字节长度（仅根据首字节判断）
int utf8_char_len(unsigned char c) {
    if ((c & 0x80) == 0) return 1;        // 0xxxxxxx - ASCII
    if ((c & 0xE0) == 0xC0) return 2;     // 110xxxxx - 2字节
    if ((c & 0xF0) == 0xE0) return 3;     // 1110xxxx - 3字节（中文）
    if ((c & 0xF8) == 0xF0) return 4;     // 11110xxx - 4字节
    return 1; // 默认返回1字节
}

int utf8_char_len_at(const char* s) {
    const unsigned char* u = (const unsigned char*)s;
    int len;

    if (!u || !u[0]) {
        return 0;
    }

    len = utf8_char_len(u[0]);
    if (len == 2 && !u[1]) {
        return 1;
    }
    if (len == 3 && (!u[1] || !u[2])) {
        return 1;
    }
    if (len == 4 && (!u[1] || !u[2] || !u[3])) {
        return 1;
    }
    return len;
}

// 获取光标前一个 UTF-8 字符的字节长度
int get_prev_utf8_char_len(const char* text, int cursor_pos) {
    if (cursor_pos <= 0) return 0;
    
    // 从光标位置向前查找 UTF-8 字符的起始位置
    int i = cursor_pos - 1;
    while (i >= 0 && ((unsigned char)text[i] & 0xC0) == 0x80) {
        i--; // 跳过 UTF-8 continuation bytes (10xxxxxx)
    }
    
    return cursor_pos - i;
}

// 获取光标处 UTF-8 字符的字节长度
int get_current_utf8_char_len(const char* text, int cursor_pos) {
    if (!text) {
        return 0;
    }
    return utf8_char_len_at(text + cursor_pos);
}

int utf8_safe_prefix_bytes(const char* text, int byte_len) {
    const char* p = text;
    const char* end = text + byte_len;
    const char* last = text;

    if (!text || byte_len <= 0) {
        return 0;
    }

    while (p < end && *p) {
        int clen = utf8_char_len_at(p);
        if (p + clen > end) {
            break;
        }
        p += clen;
        last = p;
    }

    return (int)(last - text);
}

int utf8_prev_prefix_bytes(const char* text, int byte_len) {
    const char* p = text;
    const char* end = text + byte_len;
    const char* last_start = text;

    if (!text || byte_len <= 0) {
        return 0;
    }

    while (p < end && *p) {
        int clen = utf8_char_len_at(p);
        if (p + clen > end) {
            break;
        }
        last_start = p;
        p += clen;
    }

    return (int)(last_start - text);
}

int utf8_decode_codepoint(const char** text, uint32_t* codepoint) {
    const unsigned char* p;

    if (!text || !*text || !codepoint) {
        return 0;
    }

    p = (const unsigned char*)(*text);
    if (!p[0]) {
        return 0;
    }

    if (p[0] < 0x80) {
        *codepoint = p[0];
        *text += 1;
        return 1;
    }
    if ((p[0] & 0xE0) == 0xC0 && p[1]) {
        *codepoint = ((uint32_t)(p[0] & 0x1F) << 6) | (p[1] & 0x3F);
        *text += 2;
        return 1;
    }
    if ((p[0] & 0xF0) == 0xE0 && p[1] && p[2]) {
        *codepoint = ((uint32_t)(p[0] & 0x0F) << 12) |
                     ((uint32_t)(p[1] & 0x3F) << 6) |
                     (p[2] & 0x3F);
        *text += 3;
        return 1;
    }
    if ((p[0] & 0xF8) == 0xF0 && p[1] && p[2] && p[3]) {
        *codepoint = ((uint32_t)(p[0] & 0x07) << 18) |
                     ((uint32_t)(p[1] & 0x3F) << 12) |
                     ((uint32_t)(p[2] & 0x3F) << 6) |
                     (p[3] & 0x3F);
        *text += 4;
        return 1;
    }

    *codepoint = p[0];
    *text += 1;
    return 1;
}