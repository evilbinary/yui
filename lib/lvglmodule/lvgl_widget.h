#ifndef LVGL_WIDGET_H
#define LVGL_WIDGET_H

#include "lvgl_component.h"
#include "../../lib/lvgl/lvgl.h"
#include "cJSON.h"

LvglComponent* lvgl_component_new(Layer* layer);
LvglComponent* lvgl_component_from_layer(Layer* layer);

void lvgl_bind_all_layer_events(Layer* root);
void lvgl_widget_finish_create(Layer* layer, LvglComponent* component, cJSON* json);
int lvgl_widget_register_event(Layer* layer, const char* event_name,
                               const char* event_func_name, EventHandler event_handler);

/** Copy string into LVGL heap; caller passes pointer to lv_mem_free-compatible APIs. */
char* lvgl_strdup_lv(const char* src);

cJSON* lvgl_json_get(cJSON* json, const char* key);
int lvgl_json_int(cJSON* json, const char* key, int default_value);
const char* lvgl_json_string(cJSON* json, const char* key, const char* default_value);
int lvgl_json_bool(cJSON* json, const char* key, int default_value);

char* lvgl_build_option_string(cJSON* json, const char* key);
const char** lvgl_build_btnmatrix_map(cJSON* json, const char* key, uint16_t* out_count);
void lvgl_free_btnmatrix_map(const char** map, uint16_t count);

lv_color_t lvgl_color_from_yui(Color color);
int lvgl_has_visual_style(Layer* layer, cJSON* json);
void lvgl_style_clear_container_chrome(lv_obj_t* obj);
void lvgl_apply_common_style(lv_obj_t* obj, Layer* layer, cJSON* json);
void lvgl_apply_label_style(lv_obj_t* obj, Layer* layer, cJSON* json);
void lvgl_apply_text_style(lv_obj_t* obj, Layer* layer, cJSON* json);
void lvgl_apply_range(lv_obj_t* obj, cJSON* json, int default_min, int default_max);
int lvgl_apply_value(lv_obj_t* obj, cJSON* json, Layer* layer, int default_value);

void lvgl_widget_destroy(Layer* layer);
void lvgl_widget_layout(Layer* layer);

void lvgl_widgets_register_all(void);
void lvgl_widgets_extra_register_all(void);

#endif
