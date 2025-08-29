#ifndef YUI_UTIL_H
#define YUI_UTIL_H

#include <limits.h>
#include <string.h>
#include "cJSON.h"
#include <math.h>
#include <stdlib.h>

char* replace_placeholder(const char* template, const char* placeholder, const char* value);
char* replace_all_placeholders(const char* template, const char* placeholder, const char* value);
int is_cjson_float(const cJSON *item);

#endif

