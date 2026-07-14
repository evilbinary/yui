#include "lvgl_widget.h"
#include "component_registry.h"
#include "../../lib/lvgl/lv_port.h"

#include <stdlib.h>
#include <string.h>

#define LVGL_LAYOUT(name) static void name(Layer* layer) { lvgl_widget_layout(layer); }

#if LV_USE_LABEL
static void* lvgl_label_create(Layer* layer, cJSON* json)
{
    LvglComponent* component = lvgl_component_new(layer);
    const char* text;

    if (!component) {
        return NULL;
    }

    component->obj = lv_label_create(lv_port_get_root());
    text = layer->text && layer->text[0] ? layer->text
                                           : lvgl_json_string(json, "text", "");
    if (text[0]) {
        lv_label_set_text(component->obj, text);
    }
    {
        cJSON* color_item = lvgl_json_get(json, "color");
        if (color_item && cJSON_IsString(color_item)) {
            parse_color(color_item->valuestring, &layer->color);
        }
    }
    lvgl_apply_text_style(component->obj, layer, json);
    lvgl_widget_finish_create(layer, component, json);
    return component;
}

static void lvgl_label_layout(Layer* layer)
{
    LvglComponent* component = lvgl_component_from_layer(layer);

    if (!component || !component->obj) {
        return;
    }

    lvgl_widget_layout(layer);

    if (layer->color.a > 0) {
        lv_obj_set_style_text_color(component->obj, lvgl_color_from_yui(layer->color), 0);
    }
}
#endif

#if LV_USE_BTN
static void* lvgl_btn_create(Layer* layer, cJSON* json)
{
    LvglComponent* component = lvgl_component_new(layer);
    lv_obj_t* label;
    const char* text;

    if (!component) {
        return NULL;
    }

    component->obj = lv_btn_create(lv_port_get_root());
    label = lv_label_create(component->obj);
    text = layer->text && layer->text[0] ? layer->text
                                           : lvgl_json_string(json, "text", "Button");
    lv_label_set_text(label, text);
    lv_obj_center(label);
    lvgl_apply_common_style(component->obj, layer, json);
    lv_obj_set_style_shadow_width(component->obj, 0, 0);
    if (!lvgl_json_get(json, "borderColor")) {
        lv_obj_set_style_border_width(component->obj, 0, 0);
    }
    lvgl_apply_label_style(label, layer, json);
    lvgl_widget_finish_create(layer, component, json);
    return component;
}
LVGL_LAYOUT(lvgl_btn_layout)
#endif

#if LV_USE_BAR
static void* lvgl_bar_create(Layer* layer, cJSON* json)
{
    LvglComponent* component = lvgl_component_new(layer);

    if (!component) {
        return NULL;
    }

    component->obj = lv_bar_create(lv_port_get_root());
    lvgl_apply_common_style(component->obj, layer, json);
    lvgl_apply_range(component->obj, json, 0, 100);
    lvgl_apply_value(component->obj, json, layer, 0);
    lvgl_widget_finish_create(layer, component, json);
    return component;
}
LVGL_LAYOUT(lvgl_bar_layout)
#endif

#if LV_USE_SLIDER
static void* lvgl_slider_create(Layer* layer, cJSON* json)
{
    LvglComponent* component = lvgl_component_new(layer);

    if (!component) {
        return NULL;
    }

    component->obj = lv_slider_create(lv_port_get_root());
    lvgl_apply_common_style(component->obj, layer, json);
    lvgl_apply_range(component->obj, json, 0, 100);
    lvgl_apply_value(component->obj, json, layer, 0);
    lvgl_widget_finish_create(layer, component, json);
    return component;
}
LVGL_LAYOUT(lvgl_slider_layout)
#endif

#if LV_USE_SWITCH
static void* lvgl_switch_create(Layer* layer, cJSON* json)
{
    LvglComponent* component = lvgl_component_new(layer);

    if (!component) {
        return NULL;
    }

    component->obj = lv_switch_create(lv_port_get_root());
    lvgl_apply_common_style(component->obj, layer, json);
    if (lvgl_json_bool(json, "checked", 0)) {
        lv_obj_add_state(component->obj, LV_STATE_CHECKED);
    }
    lvgl_widget_finish_create(layer, component, json);
    return component;
}
LVGL_LAYOUT(lvgl_switch_layout)
#endif

#if LV_USE_CHECKBOX
static void* lvgl_checkbox_create(Layer* layer, cJSON* json)
{
    LvglComponent* component = lvgl_component_new(layer);
    const char* text;

    if (!component) {
        return NULL;
    }

    component->obj = lv_checkbox_create(lv_port_get_root());
    text = layer->text && layer->text[0] ? layer->text
                                         : lvgl_json_string(json, "text", "Checkbox");
    lv_checkbox_set_text(component->obj, text);
    lvgl_apply_text_style(component->obj, layer, json);
    if (lvgl_json_bool(json, "checked", 0)) {
        lv_obj_add_state(component->obj, LV_STATE_CHECKED);
    }
    lvgl_widget_finish_create(layer, component, json);
    return component;
}
LVGL_LAYOUT(lvgl_checkbox_layout)
#endif

#if LV_USE_ARC
static void* lvgl_arc_create(Layer* layer, cJSON* json)
{
    LvglComponent* component = lvgl_component_new(layer);

    if (!component) {
        return NULL;
    }

    component->obj = lv_arc_create(lv_port_get_root());
    lvgl_apply_common_style(component->obj, layer, json);
    lvgl_apply_range(component->obj, json, 0, 100);
    lvgl_apply_value(component->obj, json, layer, 40);
    lv_arc_set_bg_angles(component->obj, 0, 360);
    lvgl_widget_finish_create(layer, component, json);
    return component;
}
LVGL_LAYOUT(lvgl_arc_layout)
#endif

#if LV_USE_DROPDOWN
static void lvgl_dropdown_destroy(Layer* layer)
{
    LvglComponent* component = lvgl_component_from_layer(layer);
    char* options = NULL;

    if (component && component->widget_data) {
        options = (char*)component->widget_data;
        component->widget_data = NULL;
    }
    lvgl_widget_destroy(layer);
    if (options) {
        free(options);
    }
}

static void* lvgl_dropdown_create(Layer* layer, cJSON* json)
{
    LvglComponent* component = lvgl_component_new(layer);
    char* options;

    if (!component) {
        return NULL;
    }

    component->obj = lv_dropdown_create(lv_port_get_root());
    options = lvgl_build_option_string(json, "options");
    if (!options) {
        options = strdup("One\nTwo\nThree");
    }
    component->widget_data = options;
    lv_dropdown_set_options(component->obj, options);
    if (layer->text && layer->text[0]) {
        lv_dropdown_set_text(component->obj, layer->text);
    }
    lvgl_apply_text_style(component->obj, layer, json);
    lvgl_widget_finish_create(layer, component, json);
    return component;
}
LVGL_LAYOUT(lvgl_dropdown_layout)
#endif

#if LV_USE_ROLLER
static void lvgl_roller_destroy(Layer* layer)
{
    LvglComponent* component = lvgl_component_from_layer(layer);
    char* options = NULL;

    if (component && component->widget_data) {
        options = (char*)component->widget_data;
        component->widget_data = NULL;
    }
    lvgl_widget_destroy(layer);
    if (options) {
        free(options);
    }
}

static void* lvgl_roller_create(Layer* layer, cJSON* json)
{
    LvglComponent* component = lvgl_component_new(layer);
    char* options;

    if (!component) {
        return NULL;
    }

    component->obj = lv_roller_create(lv_port_get_root());
    options = lvgl_build_option_string(json, "options");
    if (!options) {
        options = strdup("A\nB\nC\nD");
    }
    component->widget_data = options;
    lv_roller_set_options(component->obj, options, LV_ROLLER_MODE_NORMAL);
    lvgl_apply_text_style(component->obj, layer, json);
    lvgl_widget_finish_create(layer, component, json);
    return component;
}
LVGL_LAYOUT(lvgl_roller_layout)
#endif

#if LV_USE_BTNMATRIX
static void lvgl_btnmatrix_destroy(Layer* layer)
{
    LvglComponent* component = lvgl_component_from_layer(layer);
    const char** map = NULL;
    uint16_t count = 0;

    if (component && component->widget_data) {
        map = (const char**)component->widget_data;
        while (map[count] && map[count][0] != '\0') {
            count++;
        }
        component->widget_data = NULL;
    }
    lvgl_widget_destroy(layer);
    if (map) {
        lvgl_free_btnmatrix_map(map, count);
    }
}

static void* lvgl_btnmatrix_create(Layer* layer, cJSON* json)
{
    LvglComponent* component = lvgl_component_new(layer);
    const char** map;
    uint16_t count = 0;

    if (!component) {
        return NULL;
    }

    component->obj = lv_btnmatrix_create(lv_port_get_root());
    map = lvgl_build_btnmatrix_map(json, "buttons", &count);
    if (!map) {
        static const char* default_map[] = {"A", "B", "C", ""};
        lv_btnmatrix_set_map(component->obj, default_map);
    } else {
        component->widget_data = (void*)map;
        lv_btnmatrix_set_map(component->obj, map);
    }
    lv_btnmatrix_set_btn_ctrl_all(component->obj, LV_BTNMATRIX_CTRL_CLICK_TRIG);
    lv_obj_set_style_border_width(component->obj, 0, 0);
    lv_obj_set_style_pad_all(component->obj, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_row(component->obj, 6, LV_PART_MAIN);
    lv_obj_set_style_pad_column(component->obj, 6, LV_PART_MAIN);
    lv_obj_set_style_text_align(component->obj, LV_TEXT_ALIGN_CENTER, LV_PART_ITEMS);
    lv_obj_set_style_radius(component->obj, 6, LV_PART_ITEMS);
    lvgl_apply_common_style(component->obj, layer, json);
    lvgl_widget_finish_create(layer, component, json);
    return component;
}

static void lvgl_btnmatrix_layout(Layer* layer)
{
    LvglComponent* component = lvgl_component_from_layer(layer);
    const char** map;

    if (!component || !component->obj) {
        return;
    }

    lvgl_widget_layout(layer);
    lv_obj_set_style_pad_all(component->obj, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_row(component->obj, 6, LV_PART_MAIN);
    lv_obj_set_style_pad_column(component->obj, 6, LV_PART_MAIN);

    map = lv_btnmatrix_get_map(component->obj);
    if (map) {
        lv_btnmatrix_set_map(component->obj, map);
        lv_btnmatrix_set_btn_ctrl_all(component->obj, LV_BTNMATRIX_CTRL_CLICK_TRIG);
    }
}
#endif

#if LV_USE_TEXTAREA
static void* lvgl_textarea_create(Layer* layer, cJSON* json)
{
    LvglComponent* component = lvgl_component_new(layer);
    const char* text;

    if (!component) {
        return NULL;
    }

    component->obj = lv_textarea_create(lv_port_get_root());
    text = layer->text && layer->text[0] ? layer->text
                                           : lvgl_json_string(json, "text", "");
    if (text[0]) {
        lv_textarea_set_text(component->obj, text);
    }
    if (lvgl_json_bool(json, "oneLine", 0)) {
        lv_textarea_set_one_line(component->obj, true);
    }
    lvgl_apply_text_style(component->obj, layer, json);
    lvgl_widget_finish_create(layer, component, json);
    return component;
}
LVGL_LAYOUT(lvgl_textarea_layout)
#endif

#if LV_USE_LINE
typedef struct {
    lv_point_t* points;
    uint16_t count;
} LvglLineData;

static void lvgl_line_destroy(Layer* layer)
{
    LvglComponent* component = lvgl_component_from_layer(layer);
    if (component && component->widget_data) {
        LvglLineData* data = (LvglLineData*)component->widget_data;
        free(data->points);
        free(data);
        component->widget_data = NULL;
    }
    lvgl_widget_destroy(layer);
}

static void* lvgl_line_create(Layer* layer, cJSON* json)
{
    LvglComponent* component = lvgl_component_new(layer);
    cJSON* points_json = lvgl_json_get(json, "points");
    LvglLineData* data = NULL;

    if (!component) {
        return NULL;
    }

    component->obj = lv_line_create(lv_port_get_root());
    if (points_json && cJSON_IsArray(points_json)) {
        int count = cJSON_GetArraySize(points_json);
        int i;

        if (count >= 2) {
            data = (LvglLineData*)calloc(1, sizeof(LvglLineData));
            data->points = (lv_point_t*)calloc((size_t)count, sizeof(lv_point_t));
            data->count = (uint16_t)count;
            for (i = 0; i < count; i++) {
                cJSON* point = cJSON_GetArrayItem(points_json, i);
                if (point && cJSON_IsArray(point) && cJSON_GetArraySize(point) >= 2) {
                    data->points[i].x = (lv_coord_t)cJSON_GetArrayItem(point, 0)->valueint;
                    data->points[i].y = (lv_coord_t)cJSON_GetArrayItem(point, 1)->valueint;
                }
            }
            component->widget_data = data;
            lv_line_set_points(component->obj, data->points, data->count);
        }
    }

    if (!data) {
        static lv_point_t default_points[] = {{0, 0}, {100, 0}};
        lv_line_set_points(component->obj, default_points, 2);
    }

    if (layer->color.a > 0) {
        lv_obj_set_style_line_color(component->obj, lvgl_color_from_yui(layer->color), 0);
    }
    lv_obj_set_style_line_width(component->obj, (lv_coord_t)lvgl_json_int(json, "lineWidth", 2), 0);
    lvgl_widget_finish_create(layer, component, json);
    return component;
}
LVGL_LAYOUT(lvgl_line_layout)
#endif

#if LV_USE_IMG
static void* lvgl_img_create(Layer* layer, cJSON* json)
{
    LvglComponent* component = lvgl_component_new(layer);
    const char* source;

    if (!component) {
        return NULL;
    }

    component->obj = lv_img_create(lv_port_get_root());
    source = layer->source[0] ? layer->source : lvgl_json_string(json, "source", "");
    if (source[0]) {
        lv_img_set_src(component->obj, source);
    } else {
        lv_img_set_src(component->obj, LV_SYMBOL_IMAGE);
    }
    lvgl_apply_common_style(component->obj, layer, json);
    lvgl_widget_finish_create(layer, component, json);
    return component;
}
LVGL_LAYOUT(lvgl_img_layout)
#endif

#if LV_USE_CANVAS
typedef struct {
    lv_color_t* buffer;
    lv_img_dsc_t descriptor;
} LvglCanvasData;

static void lvgl_canvas_destroy(Layer* layer)
{
    LvglComponent* component = lvgl_component_from_layer(layer);
    if (component && component->widget_data) {
        LvglCanvasData* data = (LvglCanvasData*)component->widget_data;
        free(data->buffer);
        free(data);
        component->widget_data = NULL;
    }
    lvgl_widget_destroy(layer);
}

static void* lvgl_canvas_create(Layer* layer, cJSON* json)
{
    LvglComponent* component = lvgl_component_new(layer);
    LvglCanvasData* data;
    int width;
    int height;

    (void)json;

    if (!component) {
        return NULL;
    }

    width = layer->rect.w > 0 ? layer->rect.w : 100;
    height = layer->rect.h > 0 ? layer->rect.h : 60;
    data = (LvglCanvasData*)calloc(1, sizeof(LvglCanvasData));
    data->buffer = (lv_color_t*)calloc((size_t)width * (size_t)height, sizeof(lv_color_t));
    if (!data->buffer) {
        free(data);
        free(component);
        return NULL;
    }

    component->obj = lv_canvas_create(lv_port_get_root());
    lv_canvas_set_buffer(component->obj, data->buffer, width, height, LV_IMG_CF_TRUE_COLOR_ALPHA);
    lv_canvas_fill_bg(component->obj, lv_color_hex(0x334455), LV_OPA_COVER);
    data->descriptor = *(lv_canvas_get_img(component->obj));
    component->widget_data = data;
    lvgl_widget_finish_create(layer, component, json);
    return component;
}
LVGL_LAYOUT(lvgl_canvas_layout)
#endif

#if LV_USE_TABLE
static void* lvgl_table_create(Layer* layer, cJSON* json)
{
    LvglComponent* component = lvgl_component_new(layer);
    int rows;
    int cols;
    cJSON* cells;
    int r;
    int c;

    if (!component) {
        return NULL;
    }

    rows = lvgl_json_int(json, "rows", 2);
    cols = lvgl_json_int(json, "cols", 2);
    component->obj = lv_table_create(lv_port_get_root());
    lv_table_set_row_cnt(component->obj, (uint16_t)rows);
    lv_table_set_col_cnt(component->obj, (uint16_t)cols);

    cells = lvgl_json_get(json, "cells");
    if (cells && cJSON_IsArray(cells)) {
        for (r = 0; r < rows; r++) {
            cJSON* row = cJSON_GetArrayItem(cells, r);
            if (!row || !cJSON_IsArray(row)) {
                continue;
            }
            for (c = 0; c < cols; c++) {
                cJSON* cell = cJSON_GetArrayItem(row, c);
                if (cell && cJSON_IsString(cell)) {
                    lv_table_set_cell_value(component->obj, (uint16_t)r, (uint16_t)c,
                                            cell->valuestring);
                }
            }
        }
    } else {
        lv_table_set_cell_value(component->obj, 0, 0, "A");
        lv_table_set_cell_value(component->obj, 0, 1, "B");
        lv_table_set_cell_value(component->obj, 1, 0, "C");
        lv_table_set_cell_value(component->obj, 1, 1, "D");
    }

    lvgl_apply_text_style(component->obj, layer, json);
    lvgl_widget_finish_create(layer, component, json);
    return component;
}
LVGL_LAYOUT(lvgl_table_layout)
#endif

void lvgl_widgets_register_all(void)
{
#if LV_USE_LABEL
    yui_component_register("LvLabel", &(YuiComponentOps){
        .create = lvgl_label_create, .destroy = lvgl_widget_destroy,
        .on_layout = lvgl_label_layout, .flags = YUI_COMP_LVGL_WIDGET,
        .backend_mask = YUI_BACKEND_LVGL,
    });
#endif
#if LV_USE_BTN
    yui_component_register("LvBtn", &(YuiComponentOps){
        .create = lvgl_btn_create, .destroy = lvgl_widget_destroy,
        .on_layout = lvgl_btn_layout, .flags = YUI_COMP_LVGL_WIDGET,
        .backend_mask = YUI_BACKEND_LVGL,
    });
#endif
#if LV_USE_BAR
    yui_component_register("LvBar", &(YuiComponentOps){
        .create = lvgl_bar_create, .destroy = lvgl_widget_destroy,
        .on_layout = lvgl_bar_layout, .flags = YUI_COMP_LVGL_WIDGET,
        .backend_mask = YUI_BACKEND_LVGL,
    });
#endif
#if LV_USE_SLIDER
    yui_component_register("LvSlider", &(YuiComponentOps){
        .create = lvgl_slider_create, .destroy = lvgl_widget_destroy,
        .on_layout = lvgl_slider_layout, .flags = YUI_COMP_LVGL_WIDGET,
        .backend_mask = YUI_BACKEND_LVGL,
    });
#endif
#if LV_USE_SWITCH
    yui_component_register("LvSwitch", &(YuiComponentOps){
        .create = lvgl_switch_create, .destroy = lvgl_widget_destroy,
        .on_layout = lvgl_switch_layout, .flags = YUI_COMP_LVGL_WIDGET,
        .backend_mask = YUI_BACKEND_LVGL,
    });
#endif
#if LV_USE_CHECKBOX
    yui_component_register("LvCheckbox", &(YuiComponentOps){
        .create = lvgl_checkbox_create, .destroy = lvgl_widget_destroy,
        .on_layout = lvgl_checkbox_layout, .flags = YUI_COMP_LVGL_WIDGET,
        .backend_mask = YUI_BACKEND_LVGL,
    });
#endif
#if LV_USE_ARC
    yui_component_register("LvArc", &(YuiComponentOps){
        .create = lvgl_arc_create, .destroy = lvgl_widget_destroy,
        .on_layout = lvgl_arc_layout, .flags = YUI_COMP_LVGL_WIDGET,
        .backend_mask = YUI_BACKEND_LVGL,
    });
#endif
#if LV_USE_DROPDOWN
    yui_component_register("LvDropdown", &(YuiComponentOps){
        .create = lvgl_dropdown_create, .destroy = lvgl_dropdown_destroy,
        .on_layout = lvgl_dropdown_layout, .flags = YUI_COMP_LVGL_WIDGET,
        .backend_mask = YUI_BACKEND_LVGL,
    });
#endif
#if LV_USE_ROLLER
    yui_component_register("LvRoller", &(YuiComponentOps){
        .create = lvgl_roller_create, .destroy = lvgl_roller_destroy,
        .on_layout = lvgl_roller_layout, .flags = YUI_COMP_LVGL_WIDGET,
        .backend_mask = YUI_BACKEND_LVGL,
    });
#endif
#if LV_USE_BTNMATRIX
    yui_component_register("LvBtnMatrix", &(YuiComponentOps){
        .create = lvgl_btnmatrix_create, .destroy = lvgl_btnmatrix_destroy,
        .on_layout = lvgl_btnmatrix_layout, .flags = YUI_COMP_LVGL_WIDGET,
        .backend_mask = YUI_BACKEND_LVGL,
    });
#endif
#if LV_USE_TEXTAREA
    yui_component_register("LvTextarea", &(YuiComponentOps){
        .create = lvgl_textarea_create, .destroy = lvgl_widget_destroy,
        .on_layout = lvgl_textarea_layout, .flags = YUI_COMP_LVGL_WIDGET,
        .backend_mask = YUI_BACKEND_LVGL,
    });
#endif
#if LV_USE_LINE
    yui_component_register("LvLine", &(YuiComponentOps){
        .create = lvgl_line_create, .destroy = lvgl_line_destroy,
        .on_layout = lvgl_line_layout, .flags = YUI_COMP_LVGL_WIDGET,
        .backend_mask = YUI_BACKEND_LVGL,
    });
#endif
#if LV_USE_IMG
    yui_component_register("LvImg", &(YuiComponentOps){
        .create = lvgl_img_create, .destroy = lvgl_widget_destroy,
        .on_layout = lvgl_img_layout, .flags = YUI_COMP_LVGL_WIDGET,
        .backend_mask = YUI_BACKEND_LVGL,
    });
#endif
#if LV_USE_CANVAS
    yui_component_register("LvCanvas", &(YuiComponentOps){
        .create = lvgl_canvas_create, .destroy = lvgl_canvas_destroy,
        .on_layout = lvgl_canvas_layout, .flags = YUI_COMP_LVGL_WIDGET,
        .backend_mask = YUI_BACKEND_LVGL,
    });
#endif
#if LV_USE_TABLE
    yui_component_register("LvTable", &(YuiComponentOps){
        .create = lvgl_table_create, .destroy = lvgl_widget_destroy,
        .on_layout = lvgl_table_layout, .flags = YUI_COMP_LVGL_WIDGET,
        .backend_mask = YUI_BACKEND_LVGL,
    });
#endif
    lvgl_widgets_extra_register_all();
}
