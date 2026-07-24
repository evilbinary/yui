#ifndef YUI_UTIL_H
#define YUI_UTIL_H

#include <limits.h>
#include <stdint.h>
#include <string.h>
#include "cJSON.h"
#include <math.h>
#include <stdlib.h>
#include "ytype.h"

char* replace_placeholder(const char* template, const char* placeholder, const char* value);
char* replace_all_placeholders(const char* template, const char* placeholder, const char* value);
int is_cjson_float(const cJSON *item);

// 检查点是否在矩形内
int is_point_in_rect(int x, int y, Rect rect);
int point_in_rect(Point pt, Rect rect);


void parse_color(char* valuestring, Color* color);

Color layer_gradient_sample(const LayerGradient* g, float t);

/* shadow: "ox oy blur spread #color" 或对象 {x,y,blur,spread,color} */
int parse_layer_shadow(cJSON* value, LayerShadow* out);
/* bgGradient: "linear vertical|#to-bottom c1 c2" / linear-gradient(...) / 对象 */
int parse_layer_gradient(cJSON* value, LayerGradient* out);

// UTF-8 字符处理
int utf8_char_len(unsigned char c);
int utf8_char_len_at(const char* s);
int get_prev_utf8_char_len(const char* text, int cursor_pos);
int get_current_utf8_char_len(const char* text, int cursor_pos);
int utf8_safe_prefix_bytes(const char* text, int byte_len);
int utf8_prev_prefix_bytes(const char* text, int byte_len);
int utf8_decode_codepoint(const char** text, uint32_t* codepoint);

#endif

