#include "lvgl_widget.h"
#include "component_registry.h"
#include "../../lib/lvgl/lv_port.h"
#include "../../src/event.h"
#include "../../src/util.h"

#include <stdlib.h>
#include <string.h>

int lvgl_widget_register_event(Layer* layer, const char* event_name,
                               const char* event_func_name, EventHandler event_handler);

LvglComponent* lvgl_component_new(Layer* layer)
{
    LvglComponent* component = (LvglComponent*)calloc(1, sizeof(LvglComponent));
    if (!component) {
        return NULL;
    }
    component->layer = layer;
    if (layer) {
        layer->register_event = lvgl_widget_register_event;
    }
    return component;
}

LvglComponent* lvgl_component_from_layer(Layer* layer)
{
    const YuiComponentOps* ops;

    if (!layer || !layer->component) {
        return NULL;
    }

    ops = yui_type_get_ops(layer->type);
    if (!ops || !(ops->flags & YUI_COMP_LVGL_WIDGET)) {
        return NULL;
    }

    return (LvglComponent*)layer->component;
}

void lvgl_widget_destroy(Layer* layer)
{
    if (!layer || !layer->component) {
        return;
    }

    lvgl_component_destroy((LvglComponent*)layer->component);
    layer->component = NULL;
}

void lvgl_widget_layout(Layer* layer)
{
    LvglComponent* component = lvgl_component_from_layer(layer);
    if (component) {
        lvgl_component_sync_rect(component);
    }
}

char* lvgl_strdup_lv(const char* src)
{
    size_t len;
    char* dup;

    if (!src) {
        return NULL;
    }

    len = strlen(src) + 1;
    dup = (char*)lv_mem_alloc(len);
    if (!dup) {
        return NULL;
    }
    memcpy(dup, src, len);
    return dup;
}

static void lvgl_widget_resolve_handlers(Layer* layer, LvglComponent* component)
{
    if (!layer || !component) {
        return;
    }

    if (layer->event && !layer->event->click && layer->event->click_name[0] != '\0') {
        layer->event->click = (void (*)(Layer*))find_event_by_name(layer->event->click_name);
    }

    if (!component->on_change && component->on_change_name[0] != '\0') {
        component->on_change = find_event_by_name(component->on_change_name);
    }
}

static void lvgl_widget_trigger_on_change(LvglComponent* component)
{
    Layer* layer;

    if (!component || !component->layer) {
        return;
    }

    layer = component->layer;
    lvgl_widget_resolve_handlers(layer, component);
    if (component->on_change) {
        component->on_change(layer);
    }
}

static void lvgl_widget_on_clicked(lv_event_t* e)
{
    LvglComponent* component = (LvglComponent*)lv_event_get_user_data(e);
    Layer* layer;

    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (!component || !component->layer || !component->obj) {
        return;
    }

    layer = component->layer;
    lvgl_widget_resolve_handlers(layer, component);
    if (layer->event && layer->event->click) {
        layer->event->click(layer);
    }
}

static void lvgl_widget_on_value_changed(lv_event_t* e)
{
    LvglComponent* component = (LvglComponent*)lv_event_get_user_data(e);

    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) {
        return;
    }
    if (!component || !component->layer || !component->obj) {
        return;
    }

    lvgl_widget_trigger_on_change(component);
}

void lvgl_widget_parse_events(Layer* layer, LvglComponent* component, cJSON* json)
{
    cJSON* events;
    cJSON* on_change;

    if (!layer || !component || !json) {
        return;
    }

    events = cJSON_GetObjectItem(json, "events");
    if (!events) {
        return;
    }

    on_change = cJSON_GetObjectItem(events, "onChange");
    if (!on_change || !cJSON_IsString(on_change) || !on_change->valuestring) {
        return;
    }

    {
        const char* event_name = on_change->valuestring;
        if (event_name[0] == '@') {
            event_name++;
        }
        strncpy(component->on_change_name, event_name, sizeof(component->on_change_name) - 1);
        component->on_change_name[sizeof(component->on_change_name) - 1] = '\0';
        component->on_change = find_event_by_name(event_name);
    }
}

void lvgl_widget_finish_create(Layer* layer, LvglComponent* component, cJSON* json)
{
    if (!component) {
        return;
    }

    lvgl_widget_parse_events(layer, component, json);
    lvgl_component_sync_rect(component);
}

int lvgl_widget_register_event(Layer* layer, const char* event_name,
                               const char* event_func_name, EventHandler event_handler)
{
    LvglComponent* component;
    const char* name;

    if (!layer || !layer->component || !event_name) {
        return -1;
    }
    if (strcmp(event_name, "change") != 0 && strcmp(event_name, "onChange") != 0) {
        return -1;
    }

    component = (LvglComponent*)layer->component;
    component->on_change = event_handler;
    name = event_func_name;
    if (name && name[0] == '@') {
        name++;
    }
    if (name) {
        strncpy(component->on_change_name, name, sizeof(component->on_change_name) - 1);
        component->on_change_name[sizeof(component->on_change_name) - 1] = '\0';
    } else {
        component->on_change_name[0] = '\0';
    }
    return 0;
}

void lvgl_widget_bind_layer_events(Layer* layer)
{
    LvglComponent* component = lvgl_component_from_layer(layer);
    int bind_click = 0;
    int bind_change = 0;

    if (!component || !component->obj || !layer) {
        return;
    }

    lvgl_widget_resolve_handlers(layer, component);

    if (layer->event &&
        (layer->event->click || layer->event->click_name[0] != '\0')) {
        bind_click = 1;
    }
    if (component->on_change || component->on_change_name[0] != '\0') {
        bind_change = 1;
    }

    if (!bind_click && !bind_change) {
        return;
    }

    if (bind_click) {
        lv_obj_add_flag(component->obj, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(component->obj, lvgl_widget_on_clicked, LV_EVENT_CLICKED, component);
    }
    if (bind_change) {
        lv_obj_add_event_cb(component->obj, lvgl_widget_on_value_changed,
                            LV_EVENT_VALUE_CHANGED, component);
    }
}

void lvgl_bind_all_layer_events(Layer* root)
{
    int i;

    if (!root) {
        return;
    }

    lvgl_widget_bind_layer_events(root);

    for (i = 0; i < root->child_count; i++) {
        lvgl_bind_all_layer_events(root->children[i]);
    }
    if (root->sub) {
        lvgl_bind_all_layer_events(root->sub);
    }
}

cJSON* lvgl_json_get(cJSON* json, const char* key)
{
    cJSON* value;

    if (!json || !key) {
        return NULL;
    }

    value = cJSON_GetObjectItem(json, key);
    if (value) {
        return value;
    }

    value = cJSON_GetObjectItem(json, "style");
    if (value) {
        return cJSON_GetObjectItem(value, key);
    }

    return NULL;
}

int lvgl_json_int(cJSON* json, const char* key, int default_value)
{
    cJSON* value = lvgl_json_get(json, key);
    if (!value) {
        return default_value;
    }
    if (cJSON_IsNumber(value)) {
        return value->valueint;
    }
    if (cJSON_IsBool(value)) {
        return cJSON_IsTrue(value) ? 1 : 0;
    }
    return default_value;
}

const char* lvgl_json_string(cJSON* json, const char* key, const char* default_value)
{
    cJSON* value = lvgl_json_get(json, key);
    if (value && cJSON_IsString(value) && value->valuestring) {
        return value->valuestring;
    }
    return default_value;
}

int lvgl_json_bool(cJSON* json, const char* key, int default_value)
{
    cJSON* value = lvgl_json_get(json, key);
    if (!value) {
        return default_value;
    }
    if (cJSON_IsBool(value)) {
        return cJSON_IsTrue(value) ? 1 : 0;
    }
    if (cJSON_IsNumber(value)) {
        return value->valueint != 0;
    }
    return default_value;
}

char* lvgl_build_option_string(cJSON* json, const char* key)
{
    cJSON* options = lvgl_json_get(json, key);
    char* buffer = NULL;
    size_t length = 0;
    int count;
    int i;

    if (!options || !cJSON_IsArray(options)) {
        return NULL;
    }

    count = cJSON_GetArraySize(options);
    for (i = 0; i < count; i++) {
        cJSON* item = cJSON_GetArrayItem(options, i);
        const char* text = (item && cJSON_IsString(item)) ? item->valuestring : "";
        size_t item_len = strlen(text);

        buffer = (char*)realloc(buffer, length + item_len + 2);
        if (!buffer) {
            return NULL;
        }
        if (length > 0) {
            buffer[length++] = '\n';
        }
        memcpy(buffer + length, text, item_len);
        length += item_len;
        buffer[length] = '\0';
    }

    return buffer;
}

const char** lvgl_build_btnmatrix_map(cJSON* json, const char* key, uint16_t* out_count)
{
    cJSON* buttons = lvgl_json_get(json, key);
    const char** map;
    int count;
    int i;

    if (!buttons || !cJSON_IsArray(buttons)) {
        return NULL;
    }

    count = cJSON_GetArraySize(buttons);
    map = (const char**)calloc((size_t)count + 1, sizeof(char*));
    if (!map) {
        return NULL;
    }

    for (i = 0; i < count; i++) {
        cJSON* item = cJSON_GetArrayItem(buttons, i);
        const char* text = (item && cJSON_IsString(item)) ? item->valuestring : "";
        map[i] = strdup(text);
    }
    map[count] = "";

    if (out_count) {
        *out_count = (uint16_t)count;
    }
    return map;
}

void lvgl_free_btnmatrix_map(const char** map, uint16_t count)
{
    uint16_t i;

    if (!map) {
        return;
    }

    for (i = 0; i < count; i++) {
        free((void*)map[i]);
    }
    free((void*)map);
}

lv_color_t lvgl_color_from_yui(Color color)
{
    return lv_color_make(color.r, color.g, color.b);
}

void lvgl_apply_common_style(lv_obj_t* obj, Layer* layer, cJSON* json)
{
    cJSON* bg_color_item;
    cJSON* color_item;
    Color bg_color = layer->bg_color;
    Color text_color = layer->color;

    if (!obj || !layer) {
        return;
    }

    bg_color_item = lvgl_json_get(json, "bgColor");
    if (bg_color_item && cJSON_IsString(bg_color_item)) {
        parse_color(bg_color_item->valuestring, &bg_color);
    }
    if (bg_color.a > 0) {
        lv_obj_set_style_bg_color(obj, lvgl_color_from_yui(bg_color), 0);
        lv_obj_set_style_bg_opa(obj, bg_color.a, 0);
    } else {
        lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
    }

    color_item = lvgl_json_get(json, "color");
    if (color_item && cJSON_IsString(color_item)) {
        parse_color(color_item->valuestring, &text_color);
    }
    if (text_color.a > 0) {
        lv_obj_set_style_text_color(obj, lvgl_color_from_yui(text_color), 0);
    }

    if (layer->radius > 0) {
        lv_obj_set_style_radius(obj, (lv_coord_t)layer->radius, 0);
    }

    if (HAS_STATE(layer, LAYER_STATE_DISABLED)) {
        lv_obj_add_state(obj, LV_STATE_DISABLED);
    }
}

void lvgl_apply_text_style(lv_obj_t* obj, Layer* layer, cJSON* json)
{
    lvgl_apply_common_style(obj, layer, json);
    if (layer && layer->font && layer->font->size > 0) {
        lv_obj_set_style_text_font(obj, LV_FONT_DEFAULT, 0);
    }
}

void lvgl_apply_range(lv_obj_t* obj, cJSON* json, int default_min, int default_max)
{
    int min_value = lvgl_json_int(json, "min", default_min);
    int max_value = lvgl_json_int(json, "max", default_max);

    if (lv_obj_check_type(obj, &lv_bar_class)) {
        lv_bar_set_range(obj, (int32_t)min_value, (int32_t)max_value);
    } else if (lv_obj_check_type(obj, &lv_slider_class)) {
        lv_slider_set_range(obj, (int32_t)min_value, (int32_t)max_value);
    } else if (lv_obj_check_type(obj, &lv_arc_class)) {
        lv_arc_set_range(obj, (int32_t)min_value, (int32_t)max_value);
    }
}

int lvgl_apply_value(lv_obj_t* obj, cJSON* json, Layer* layer, int default_value)
{
    int value = lvgl_json_int(json, "value", default_value);

    (void)layer;

    if (lv_obj_check_type(obj, &lv_bar_class)) {
        lv_bar_set_value(obj, (int32_t)value, LV_ANIM_OFF);
    } else if (lv_obj_check_type(obj, &lv_slider_class)) {
        lv_slider_set_value(obj, (int32_t)value, LV_ANIM_OFF);
    } else if (lv_obj_check_type(obj, &lv_arc_class)) {
        lv_arc_set_value(obj, (int32_t)value);
    }

    return value;
}
