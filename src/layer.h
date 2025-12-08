#ifndef YUI_LAYER_H
#define YUI_LAYER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"
#include "ytype.h"
#include "event.h"


cJSON* parse_json(char* json_path);
Layer* parse_layer(cJSON* json_obj,Layer* parent);
Layer* find_layer_by_id(Layer* root, const char* id);
void destroy_layer(Layer* layer);


#endif