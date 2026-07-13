#include "lvgl_widget.h"
#include "component_registry.h"
#include "../../lib/lvgl/lv_port.h"

#include <stdlib.h>
#include <string.h>

#define LVGL_EXTRA_LAYOUT(name) static void name(Layer* layer) { lvgl_widget_layout(layer); }

#if LV_USE_CHART
static void* lvgl_chart_create(Layer* layer, cJSON* json)
{
    LvglComponent* component = lvgl_component_new(layer);
    cJSON* points;
    lv_chart_series_t* series;

    if (!component) {
        return NULL;
    }

    component->obj = lv_chart_create(lv_port_get_root());
    lv_chart_set_type(component->obj, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(component->obj, 8);
    series = lv_chart_add_series(component->obj, lv_palette_main(LV_PALETTE_RED),
                                 LV_CHART_AXIS_PRIMARY_Y);

    points = lvgl_json_get(json, "points");
    if (points && cJSON_IsArray(points)) {
        int count = cJSON_GetArraySize(points);
        int i;
        if (count > 0) {
            lv_chart_set_point_count(component->obj, (uint16_t)count);
        }
        for (i = 0; i < count; i++) {
            cJSON* item = cJSON_GetArrayItem(points, i);
            if (item && cJSON_IsNumber(item)) {
                lv_chart_set_next_value(component->obj, series, item->valueint);
            }
        }
    } else {
        lv_chart_set_next_value(component->obj, series, 10);
        lv_chart_set_next_value(component->obj, series, 20);
        lv_chart_set_next_value(component->obj, series, 40);
        lv_chart_set_next_value(component->obj, series, 30);
    }

    lvgl_apply_common_style(component->obj, layer, json);
    lvgl_component_sync_rect(component);
    return component;
}
LVGL_EXTRA_LAYOUT(lvgl_chart_layout)
#endif

#if LV_USE_LED
static void* lvgl_led_create(Layer* layer, cJSON* json)
{
    LvglComponent* component = lvgl_component_new(layer);

    if (!component) {
        return NULL;
    }

    component->obj = lv_led_create(lv_port_get_root());
    lv_led_set_brightness(component->obj, (uint8_t)lvgl_json_int(json, "brightness", 200));
    if (layer->color.a > 0) {
        lv_led_set_color(component->obj, lvgl_color_from_yui(layer->color));
    }
    if (lvgl_json_bool(json, "on", 1)) {
        lv_led_on(component->obj);
    } else {
        lv_led_off(component->obj);
    }
    lvgl_component_sync_rect(component);
    return component;
}
LVGL_EXTRA_LAYOUT(lvgl_led_layout)
#endif

#if LV_USE_SPINNER
static void* lvgl_spinner_create(Layer* layer, cJSON* json)
{
    LvglComponent* component = lvgl_component_new(layer);

    if (!component) {
        return NULL;
    }

    component->obj = lv_spinner_create(lv_port_get_root(),
                                       (uint32_t)lvgl_json_int(json, "time", 1000),
                                       (uint32_t)lvgl_json_int(json, "arcLength", 60));
    lvgl_apply_common_style(component->obj, layer, json);
    lvgl_component_sync_rect(component);
    return component;
}
LVGL_EXTRA_LAYOUT(lvgl_spinner_layout)
#endif

#if LV_USE_SPINBOX
static void* lvgl_spinbox_create(Layer* layer, cJSON* json)
{
    LvglComponent* component = lvgl_component_new(layer);

    if (!component) {
        return NULL;
    }

    component->obj = lv_spinbox_create(lv_port_get_root());
    lv_spinbox_set_range(component->obj, lvgl_json_int(json, "min", 0),
                         lvgl_json_int(json, "max", 100));
    lv_spinbox_set_value(component->obj, lvgl_json_int(json, "value", 0));
    lvgl_apply_text_style(component->obj, layer, json);
    lvgl_component_sync_rect(component);
    return component;
}
LVGL_EXTRA_LAYOUT(lvgl_spinbox_layout)
#endif

#if LV_USE_LIST
static void* lvgl_list_create(Layer* layer, cJSON* json)
{
    LvglComponent* component = lvgl_component_new(layer);
    cJSON* items;
    int i;

    if (!component) {
        return NULL;
    }

    component->obj = lv_list_create(lv_port_get_root());
    items = lvgl_json_get(json, "items");
    if (items && cJSON_IsArray(items)) {
        for (i = 0; i < cJSON_GetArraySize(items); i++) {
            cJSON* item = cJSON_GetArrayItem(items, i);
            if (item && cJSON_IsString(item)) {
                lv_list_add_text(component->obj, item->valuestring);
            }
        }
    } else if (layer->text && layer->text[0]) {
        lv_list_add_text(component->obj, layer->text);
    }
    lvgl_apply_text_style(component->obj, layer, json);
    lvgl_component_sync_rect(component);
    return component;
}
LVGL_EXTRA_LAYOUT(lvgl_list_layout)
#endif

#if LV_USE_TABVIEW
static void* lvgl_tabview_create(Layer* layer, cJSON* json)
{
    LvglComponent* component = lvgl_component_new(layer);
    cJSON* tabs;
    int i;

    if (!component) {
        return NULL;
    }

    component->obj = lv_tabview_create(lv_port_get_root(), LV_DIR_TOP,
                                       (lv_coord_t)lvgl_json_int(json, "tabHeight", 32));
    tabs = lvgl_json_get(json, "tabs");
    if (tabs && cJSON_IsArray(tabs)) {
        for (i = 0; i < cJSON_GetArraySize(tabs); i++) {
            cJSON* tab = cJSON_GetArrayItem(tabs, i);
            if (tab && cJSON_IsString(tab)) {
                lv_tabview_add_tab(component->obj, tab->valuestring);
            }
        }
    } else {
        lv_tabview_add_tab(component->obj, "Tab 1");
        lv_tabview_add_tab(component->obj, "Tab 2");
    }
    lvgl_apply_common_style(component->obj, layer, json);
    lvgl_component_sync_rect(component);
    return component;
}
LVGL_EXTRA_LAYOUT(lvgl_tabview_layout)
#endif

#if LV_USE_TILEVIEW
static void* lvgl_tileview_create(Layer* layer, cJSON* json)
{
    LvglComponent* component = lvgl_component_new(layer);

    if (!component) {
        return NULL;
    }

    component->obj = lv_tileview_create(lv_port_get_root());
    lv_tileview_add_tile(component->obj, 0, 0, LV_DIR_BOTTOM);
    lv_tileview_add_tile(component->obj, 1, 0, LV_DIR_LEFT | LV_DIR_RIGHT);
    (void)json;
    lvgl_apply_common_style(component->obj, layer, json);
    lvgl_component_sync_rect(component);
    return component;
}
LVGL_EXTRA_LAYOUT(lvgl_tileview_layout)
#endif

#if LV_USE_WIN
static void* lvgl_win_create(Layer* layer, cJSON* json)
{
    LvglComponent* component = lvgl_component_new(layer);
    const char* title;

    if (!component) {
        return NULL;
    }

    component->obj = lv_win_create(lv_port_get_root(),
                                 (lv_coord_t)lvgl_json_int(json, "headerHeight", 32));
    title = layer->text && layer->text[0] ? layer->text : lvgl_json_string(json, "text", "Window");
    lv_win_add_title(component->obj, title);
    lvgl_apply_common_style(component->obj, layer, json);
    lvgl_component_sync_rect(component);
    return component;
}
LVGL_EXTRA_LAYOUT(lvgl_win_layout)
#endif

#if LV_USE_MSGBOX
static void* lvgl_msgbox_create(Layer* layer, cJSON* json)
{
    LvglComponent* component = lvgl_component_new(layer);
    const char* title;
    const char* text;
    static const char* btns[] = {"OK", ""};

    if (!component) {
        return NULL;
    }

    title = lvgl_json_string(json, "title", "Message");
    text = layer->text && layer->text[0] ? layer->text : lvgl_json_string(json, "text", "Hello");
    component->obj = lv_msgbox_create(lv_port_get_root(), title, text, btns, true);
    lvgl_apply_text_style(component->obj, layer, json);
    lvgl_component_sync_rect(component);
    return component;
}
LVGL_EXTRA_LAYOUT(lvgl_msgbox_layout)
#endif

#if LV_USE_KEYBOARD
static Layer* lvgl_layer_root(Layer* layer)
{
    while (layer && layer->parent) {
        layer = layer->parent;
    }
    return layer;
}

static void* lvgl_keyboard_create(Layer* layer, cJSON* json)
{
    LvglComponent* component = lvgl_component_new(layer);
    const char* textarea_id;
    Layer* root;
    Layer* ta_layer;
    LvglComponent* ta_component;

    if (!component) {
        return NULL;
    }

    component->obj = lv_keyboard_create(lv_port_get_root());
    lv_keyboard_set_mode(component->obj, LV_KEYBOARD_MODE_TEXT_LOWER);

    textarea_id = lvgl_json_string(json, "textarea", "demo_textarea");
    root = lvgl_layer_root(layer);
    if (root && textarea_id[0]) {
        ta_layer = find_layer_by_id(root, textarea_id);
        ta_component = ta_layer ? lvgl_component_from_layer(ta_layer) : NULL;
        if (ta_component && ta_component->obj) {
            lv_keyboard_set_textarea(component->obj, ta_component->obj);
        }
    }

    lvgl_apply_common_style(component->obj, layer, json);
    lvgl_component_sync_rect(component);
    return component;
}
LVGL_EXTRA_LAYOUT(lvgl_keyboard_layout)
#endif

#if LV_USE_CALENDAR
static void* lvgl_calendar_create(Layer* layer, cJSON* json)
{
    LvglComponent* component = lvgl_component_new(layer);

    if (!component) {
        return NULL;
    }

    component->obj = lv_calendar_create(lv_port_get_root());
    (void)json;
    lvgl_apply_common_style(component->obj, layer, json);
    lvgl_component_sync_rect(component);
    return component;
}
LVGL_EXTRA_LAYOUT(lvgl_calendar_layout)
#endif

#if LV_USE_COLORWHEEL
static void* lvgl_colorwheel_create(Layer* layer, cJSON* json)
{
    LvglComponent* component = lvgl_component_new(layer);

    if (!component) {
        return NULL;
    }

    component->obj = lv_colorwheel_create(lv_port_get_root(), true);
    (void)json;
    lvgl_component_sync_rect(component);
    return component;
}
LVGL_EXTRA_LAYOUT(lvgl_colorwheel_layout)
#endif

#if LV_USE_METER
static void* lvgl_meter_create(Layer* layer, cJSON* json)
{
    LvglComponent* component = lvgl_component_new(layer);
    lv_meter_scale_t* scale;
    lv_meter_indicator_t* indic;

    if (!component) {
        return NULL;
    }

    component->obj = lv_meter_create(lv_port_get_root());
    scale = lv_meter_add_scale(component->obj);
    lv_meter_set_scale_range(component->obj, scale, 0, 100, 270, 90);
    indic = lv_meter_add_needle_line(component->obj, scale, 4,
                                       lv_palette_main(LV_PALETTE_GREY), -10);
    lv_meter_set_indicator_value(component->obj, indic, lvgl_json_int(json, "value", 40));
    lvgl_apply_common_style(component->obj, layer, json);
    lvgl_component_sync_rect(component);
    return component;
}
LVGL_EXTRA_LAYOUT(lvgl_meter_layout)
#endif

#if LV_USE_IMGBTN
static void* lvgl_imgbtn_create(Layer* layer, cJSON* json)
{
    LvglComponent* component = lvgl_component_new(layer);
    const char* source;

    if (!component) {
        return NULL;
    }

    component->obj = lv_imgbtn_create(lv_port_get_root());
    source = layer->source[0] ? layer->source : lvgl_json_string(json, "source", "");
    if (source[0]) {
        lv_imgbtn_set_src(component->obj, LV_IMGBTN_STATE_RELEASED, NULL, source, NULL);
    } else {
        /* imgbtn uses lv_draw_img; LV_SYMBOL_* is not valid here (crashes in decoder). */
        lv_obj_set_style_bg_color(component->obj, lv_palette_main(LV_PALETTE_BLUE), 0);
        lv_obj_set_style_bg_opa(component->obj, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(component->obj, 8, 0);
    }
    lvgl_apply_common_style(component->obj, layer, json);
    lvgl_component_sync_rect(component);
    return component;
}
LVGL_EXTRA_LAYOUT(lvgl_imgbtn_layout)
#endif

#if LV_USE_MENU
static void* lvgl_menu_create(Layer* layer, cJSON* json)
{
    LvglComponent* component = lvgl_component_new(layer);
    lv_obj_t* page;

    if (!component) {
        return NULL;
    }

    component->obj = lv_menu_create(lv_port_get_root());
    {
        const char* title_src = (layer->text && layer->text[0]) ? layer->text : "Menu";
        char* title = lvgl_strdup_lv(title_src);
        if (!title) {
            lv_obj_del(component->obj);
            free(component);
            return NULL;
        }
        page = lv_menu_page_create(component->obj, title);
    }
    lv_menu_set_page(component->obj, page);
    (void)json;
    lvgl_apply_common_style(component->obj, layer, json);
    lvgl_component_sync_rect(component);
    return component;
}
LVGL_EXTRA_LAYOUT(lvgl_menu_layout)
#endif

#if LV_USE_SPAN
static void* lvgl_span_create(Layer* layer, cJSON* json)
{
    LvglComponent* component = lvgl_component_new(layer);
    lv_span_t* span;
    const char* text;

    if (!component) {
        return NULL;
    }

    component->obj = lv_spangroup_create(lv_port_get_root());
    span = lv_spangroup_new_span(component->obj);
    text = layer->text && layer->text[0] ? layer->text : lvgl_json_string(json, "text", "Span text");
    lv_span_set_text(span, text);
    lv_spangroup_refr_mode(component->obj);
    lvgl_apply_text_style(component->obj, layer, json);
    lvgl_component_sync_rect(component);
    return component;
}
LVGL_EXTRA_LAYOUT(lvgl_span_layout)
#endif

#if LV_USE_ANIMIMG
static void* lvgl_animimg_create(Layer* layer, cJSON* json)
{
    LvglComponent* component = lvgl_component_new(layer);

    if (!component) {
        return NULL;
    }

    component->obj = lv_animimg_create(lv_port_get_root());
    (void)json;
    lvgl_component_sync_rect(component);
    return component;
}
LVGL_EXTRA_LAYOUT(lvgl_animimg_layout)
#endif

void lvgl_widgets_extra_register_all(void)
{
#if LV_USE_CHART
    yui_component_register("LvChart", &(YuiComponentOps){
        .create = lvgl_chart_create, .destroy = lvgl_widget_destroy,
        .on_layout = lvgl_chart_layout, .flags = YUI_COMP_LVGL_WIDGET,
        .backend_mask = YUI_BACKEND_LVGL,
    });
#endif
#if LV_USE_LED
    yui_component_register("LvLed", &(YuiComponentOps){
        .create = lvgl_led_create, .destroy = lvgl_widget_destroy,
        .on_layout = lvgl_led_layout, .flags = YUI_COMP_LVGL_WIDGET,
        .backend_mask = YUI_BACKEND_LVGL,
    });
#endif
#if LV_USE_SPINNER
    yui_component_register("LvSpinner", &(YuiComponentOps){
        .create = lvgl_spinner_create, .destroy = lvgl_widget_destroy,
        .on_layout = lvgl_spinner_layout, .flags = YUI_COMP_LVGL_WIDGET,
        .backend_mask = YUI_BACKEND_LVGL,
    });
#endif
#if LV_USE_SPINBOX
    yui_component_register("LvSpinbox", &(YuiComponentOps){
        .create = lvgl_spinbox_create, .destroy = lvgl_widget_destroy,
        .on_layout = lvgl_spinbox_layout, .flags = YUI_COMP_LVGL_WIDGET,
        .backend_mask = YUI_BACKEND_LVGL,
    });
#endif
#if LV_USE_LIST
    yui_component_register("LvList", &(YuiComponentOps){
        .create = lvgl_list_create, .destroy = lvgl_widget_destroy,
        .on_layout = lvgl_list_layout, .flags = YUI_COMP_LVGL_WIDGET,
        .backend_mask = YUI_BACKEND_LVGL,
    });
#endif
#if LV_USE_TABVIEW
    yui_component_register("LvTabview", &(YuiComponentOps){
        .create = lvgl_tabview_create, .destroy = lvgl_widget_destroy,
        .on_layout = lvgl_tabview_layout, .flags = YUI_COMP_LVGL_WIDGET,
        .backend_mask = YUI_BACKEND_LVGL,
    });
#endif
#if LV_USE_TILEVIEW
    yui_component_register("LvTileview", &(YuiComponentOps){
        .create = lvgl_tileview_create, .destroy = lvgl_widget_destroy,
        .on_layout = lvgl_tileview_layout, .flags = YUI_COMP_LVGL_WIDGET,
        .backend_mask = YUI_BACKEND_LVGL,
    });
#endif
#if LV_USE_WIN
    yui_component_register("LvWin", &(YuiComponentOps){
        .create = lvgl_win_create, .destroy = lvgl_widget_destroy,
        .on_layout = lvgl_win_layout, .flags = YUI_COMP_LVGL_WIDGET,
        .backend_mask = YUI_BACKEND_LVGL,
    });
#endif
#if LV_USE_MSGBOX
    yui_component_register("LvMsgbox", &(YuiComponentOps){
        .create = lvgl_msgbox_create, .destroy = lvgl_widget_destroy,
        .on_layout = lvgl_msgbox_layout, .flags = YUI_COMP_LVGL_WIDGET,
        .backend_mask = YUI_BACKEND_LVGL,
    });
#endif
#if LV_USE_KEYBOARD
    yui_component_register("LvKeyboard", &(YuiComponentOps){
        .create = lvgl_keyboard_create, .destroy = lvgl_widget_destroy,
        .on_layout = lvgl_keyboard_layout, .flags = YUI_COMP_LVGL_WIDGET,
        .backend_mask = YUI_BACKEND_LVGL,
    });
#endif
#if LV_USE_CALENDAR
    yui_component_register("LvCalendar", &(YuiComponentOps){
        .create = lvgl_calendar_create, .destroy = lvgl_widget_destroy,
        .on_layout = lvgl_calendar_layout, .flags = YUI_COMP_LVGL_WIDGET,
        .backend_mask = YUI_BACKEND_LVGL,
    });
#endif
#if LV_USE_COLORWHEEL
    yui_component_register("LvColorwheel", &(YuiComponentOps){
        .create = lvgl_colorwheel_create, .destroy = lvgl_widget_destroy,
        .on_layout = lvgl_colorwheel_layout, .flags = YUI_COMP_LVGL_WIDGET,
        .backend_mask = YUI_BACKEND_LVGL,
    });
#endif
#if LV_USE_METER
    yui_component_register("LvMeter", &(YuiComponentOps){
        .create = lvgl_meter_create, .destroy = lvgl_widget_destroy,
        .on_layout = lvgl_meter_layout, .flags = YUI_COMP_LVGL_WIDGET,
        .backend_mask = YUI_BACKEND_LVGL,
    });
#endif
#if LV_USE_IMGBTN
    yui_component_register("LvImgbtn", &(YuiComponentOps){
        .create = lvgl_imgbtn_create, .destroy = lvgl_widget_destroy,
        .on_layout = lvgl_imgbtn_layout, .flags = YUI_COMP_LVGL_WIDGET,
        .backend_mask = YUI_BACKEND_LVGL,
    });
#endif
#if LV_USE_MENU
    yui_component_register("LvMenu", &(YuiComponentOps){
        .create = lvgl_menu_create, .destroy = lvgl_widget_destroy,
        .on_layout = lvgl_menu_layout, .flags = YUI_COMP_LVGL_WIDGET,
        .backend_mask = YUI_BACKEND_LVGL,
    });
#endif
#if LV_USE_SPAN
    yui_component_register("LvSpan", &(YuiComponentOps){
        .create = lvgl_span_create, .destroy = lvgl_widget_destroy,
        .on_layout = lvgl_span_layout, .flags = YUI_COMP_LVGL_WIDGET,
        .backend_mask = YUI_BACKEND_LVGL,
    });
#endif
#if LV_USE_ANIMIMG
    yui_component_register("LvAnimimg", &(YuiComponentOps){
        .create = lvgl_animimg_create, .destroy = lvgl_widget_destroy,
        .on_layout = lvgl_animimg_layout, .flags = YUI_COMP_LVGL_WIDGET,
        .backend_mask = YUI_BACKEND_LVGL,
    });
#endif
}
