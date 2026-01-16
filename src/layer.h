#ifndef YUI_LAYER_H
#define YUI_LAYER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"
#include "ytype.h"
#include "event.h"


cJSON* parse_json(char* json_path);
Layer* layer_create_from_json(cJSON* json_obj,Layer* parent);
Layer* parse_layer_from_string(const char* json_str, Layer* parent);
Layer* find_layer_by_id(Layer* root, const char* id);
Layer* parse_layer_from_json(Layer* layer,cJSON* json_obj, Layer* parent);
Layer* layer_create_from_json(cJSON* json_obj, Layer* parent) ;
Layer* layer_create(Layer* root_layer, int x, int y, int width, int height);


void destroy_layer(Layer* layer);

void layer_set_label(Layer* layer, const char* value);
void layer_set_text(Layer* layer, const char* value);
const char* layer_get_label(const Layer* layer);
const char* layer_get_text(const Layer* layer);


#endif
