#ifndef YUI_UTIL_H
#define YUI_UTIL_H

#include <limits.h>
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



void parse_color(char* valuestring, Color* color);


#endif

