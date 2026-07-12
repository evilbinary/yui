#include "list_component.h"
#include "button_component.h"
#include "../backend.h"
#include "../event.h"
#include "../layer_update.h"
#include "../render.h"
#include "../util.h"
#include <stdlib.h>
#include <string.h>

static int list_get_padding(const Layer* layer, int index) {
    if (layer && layer->layout_manager) {
        return layer->layout_manager->padding[index];
    }
    return 0;
}

static int list_get_spacing(ListComponent* component) {
    Layer* layer = component->layer;
    if (layer && layer->layout_manager && layer->layout_manager->spacing > 0) {
        return layer->layout_manager->spacing;
    }
    return component->spacing;
}

static void list_sync_item_metrics(ListComponent* component) {
    if (!component || !component->layer) return;

    Layer* layer = component->layer;
    if (layer->item_template && layer->item_template->rect.h > 0) {
        component->item_height = layer->item_template->rect.h;
    }
    if (layer->layout_manager && layer->layout_manager->spacing > 0) {
        component->spacing = layer->layout_manager->spacing;
    }
}

static char* list_json_value_to_string(cJSON* item) {
    if (!item) return strdup("");
    if (cJSON_IsString(item)) {
        return strdup(item->valuestring ? item->valuestring : "");
    }
    if (cJSON_IsNumber(item)) {
        char buf[64];
        if (is_cjson_float(item)) {
            snprintf(buf, sizeof(buf), "%f", item->valuedouble);
        } else {
            snprintf(buf, sizeof(buf), "%d", item->valueint);
        }
        return strdup(buf);
    }
    if (cJSON_IsTrue(item)) return strdup("true");
    if (cJSON_IsFalse(item)) return strdup("false");
    if (cJSON_IsNull(item)) return strdup("");
    char* printed = cJSON_PrintUnformatted(item);
    return printed ? printed : strdup("");
}

static char* list_resolve_template(ListComponent* component, int index, const char* template_text) {
    Layer* layer = component->layer;
    if (!template_text || !template_text[0]) return strdup("");
    if (!layer || !layer->item_template) return strdup(template_text);
    if (!layer->data || !layer->data->json || !strstr(template_text, "${")) {
        return strdup(template_text);
    }

    cJSON* item = cJSON_GetArrayItem(layer->data->json, index);
    if (!item) return strdup(template_text);

    char* result = strdup(template_text);
    if (!result) return NULL;

    for (cJSON* field = item->child; field; field = field->next) {
        if (!field->string) continue;

        char placeholder[256];
        snprintf(placeholder, sizeof(placeholder), "${%s}", field->string);

        char* value = list_json_value_to_string(field);
        if (!value) continue;

        char* replaced = replace_placeholder(result, placeholder, value);
        free(value);
        if (replaced) {
            free(result);
            result = replaced;
        }
    }

    return result;
}

static Layer* list_resolve_font_layer(Layer* list_layer, Layer* template_layer) {
    if (template_layer && template_layer->font && template_layer->font->default_font) {
        return template_layer;
    }
    return list_layer;
}

static Color list_resolve_text_color(Layer* list_layer, Layer* template_layer) {
    Color text_color = template_layer ? template_layer->color : (Color){0, 0, 0, 0};
    if (text_color.a == 0 && list_layer) {
        text_color = list_layer->color;
    }
    if (text_color.a == 0) {
        text_color = (Color){229, 229, 234, 255};
    }
    return text_color;
}

static void list_fit_texture(Rect* item_rect, int* draw_w, int* draw_h, int avail_w, int avail_h) {
    if (*draw_w <= avail_w && *draw_h <= avail_h) return;
    if (avail_w < 1) avail_w = 1;
    if (avail_h < 1) avail_h = 1;

    float ratio = (float)(*draw_w) / (*draw_h);
    if ((*draw_w) * avail_h > avail_w * (*draw_h)) {
        *draw_w = avail_w;
        *draw_h = (int)(avail_w / ratio);
    } else {
        *draw_h = avail_h;
        *draw_w = (int)(avail_h * ratio);
    }
    if (*draw_w < 1) *draw_w = 1;
    if (*draw_h < 1) *draw_h = 1;
    (void)item_rect;
}

static void list_get_item_rect(ListComponent* component, int index, Rect* out) {
    Layer* layer = component->layer;
    int spacing = list_get_spacing(component);
    int padding_top = list_get_padding(layer, 0);
    int padding_left = list_get_padding(layer, 3);
    int padding_right = list_get_padding(layer, 1);

    out->x = layer->rect.x + padding_left;
    out->y = layer->rect.y + padding_top +
             index * (component->item_height + spacing) - layer->scroll_offset;
    out->w = layer->rect.w - padding_left - padding_right;
    if (out->w < 0) out->w = 0;
    out->h = component->item_height;
}

static int list_point_in_item(ListComponent* component, int x, int y, int* out_index) {
    if (!component || !component->layer) return 0;

    Layer* layer = component->layer;
    if (x < layer->rect.x || x >= layer->rect.x + layer->rect.w ||
        y < layer->rect.y || y >= layer->rect.y + layer->rect.h) {
        return 0;
    }

    int count = list_component_get_item_count(component);
    for (int i = 0; i < count; i++) {
        Rect item_rect;
        list_get_item_rect(component, i, &item_rect);
        if (x >= item_rect.x && x < item_rect.x + item_rect.w &&
            y >= item_rect.y && y < item_rect.y + item_rect.h) {
            if (out_index) *out_index = i;
            return 1;
        }
    }
    return 0;
}

static void list_dispatch_item_click(ListComponent* component, int index) {
    Layer* layer = component->layer;
    if (!layer || !layer->item_template || !layer->data || !layer->data->json) return;

    Layer* template_layer = layer->item_template;
    if (!template_layer->event || !template_layer->event->click_name[0]) return;

    EventHandler handler = template_layer->event->click;
    if (!handler) {
        handler = find_event_by_name(template_layer->event->click_name);
    }
    if (!handler) return;

    cJSON* item = cJSON_GetArrayItem(layer->data->json, index);
    if (!item) return;

    char* item_json = cJSON_PrintUnformatted(item);
    if (!item_json) return;

    if (!layer->event) {
        layer->event = calloc(1, sizeof(Event));
    }
    if (layer->event) {
        strncpy(layer->event->click_name, template_layer->event->click_name, MAX_PATH - 1);
        layer->event->click_name[MAX_PATH - 1] = '\0';
    }

    if (layer->text) free(layer->text);
    layer->text = item_json;
    handler(layer);
}

static void list_data_update(Layer* layer, cJSON* data) {
    if (!layer || !layer->component || !data || !cJSON_IsArray(data)) return;

    ListComponent* component = (ListComponent*)layer->component;
    list_sync_item_metrics(component);

    if (layer->data) {
        if (layer->data->json) cJSON_Delete(layer->data->json);
        free(layer->data);
        layer->data = NULL;
    }

    layer->data = (Data*)malloc(sizeof(Data));
    if (!layer->data) return;

    layer->data->json = cJSON_Duplicate(data, 1);
    layer->data->size = cJSON_GetArraySize(data);
    component->hovered_index = -1;
    component->pressed_index = -1;

    list_component_update_content_size(component);
    mark_layer_dirty(layer, DIRTY_LAYOUT | DIRTY_TEXT);
}

static void list_layer_destroy(Layer* layer) {
    if (!layer || !layer->component) return;
    list_component_destroy((ListComponent*)layer->component);
    layer->component = NULL;
}

ListComponent* list_component_create(Layer* layer) {
    ListComponent* component = (ListComponent*)calloc(1, sizeof(ListComponent));
    if (!component) return NULL;

    component->layer = layer;
    component->item_height = 30;
    component->spacing = 4;
    component->hovered_index = -1;
    component->pressed_index = -1;

    layer->component = component;
    layer->render = list_component_render;
    layer->handle_mouse_event = list_component_handle_mouse_event;
    layer->handle_key_event = list_component_handle_key_event;
    layer->focusable = 1;
    layer->on_data_update = list_data_update;
    layer->on_destroy = list_layer_destroy;

    return component;
}

void list_component_destroy(ListComponent* component) {
    if (!component) return;
    free(component);
}

ListComponent* list_component_create_from_json(Layer* layer, cJSON* json_obj) {
    ListComponent* component = list_component_create(layer);
    if (!component) return NULL;

    if (json_obj) {
        cJSON* item_height = cJSON_GetObjectItem(json_obj, "itemHeight");
        if (item_height && cJSON_IsNumber(item_height)) {
            component->item_height = item_height->valueint;
        }
    }

    list_sync_item_metrics(component);
    list_component_update_content_size(component);
    return component;
}

void list_component_update_content_size(ListComponent* component) {
    if (!component || !component->layer) return;

    Layer* layer = component->layer;
    int count = list_component_get_item_count(component);
    int spacing = list_get_spacing(component);
    int padding_top = list_get_padding(layer, 0);
    int padding_bottom = list_get_padding(layer, 2);

    if (count > 0) {
        layer->content_height = padding_top + padding_bottom +
                                count * component->item_height +
                                (count - 1) * spacing;
    } else {
        layer->content_height = padding_top + padding_bottom;
    }
    layer->content_width = layer->rect.w;
}

int list_component_get_item_count(ListComponent* component) {
    if (!component || !component->layer || !component->layer->data) return 0;
    return component->layer->data->size;
}

int list_component_index_at_y(ListComponent* component, int x, int y) {
    int index = -1;
    if (list_point_in_item(component, x, y, &index)) {
        return index;
    }
    return -1;
}

static void list_render_item(ListComponent* component, int index, int is_hovered, int is_pressed) {
    Layer* layer = component->layer;
    Layer* template_layer = layer->item_template;
    if (!template_layer) return;

    Rect item_rect;
    list_get_item_rect(component, index, &item_rect);

    int visible_top = layer->rect.y;
    int visible_bottom = layer->rect.y + layer->rect.h;
    if (item_rect.y + item_rect.h < visible_top || item_rect.y > visible_bottom) {
        return;
    }

    Color bg_color = template_layer->bg_color;
    Color text_color = list_resolve_text_color(layer, template_layer);
    int radius = template_layer->radius;

    if (is_pressed) {
        if (template_layer->bg_color.a > 0) {
            bg_color.r = (unsigned char)(bg_color.r * 0.85f);
            bg_color.g = (unsigned char)(bg_color.g * 0.85f);
            bg_color.b = (unsigned char)(bg_color.b * 0.85f);
        } else {
            bg_color = (Color){128, 128, 128, 50};
        }
    } else if (is_hovered) {
        if (template_layer->bg_color.a > 0) {
            bg_color.r = (unsigned char)(bg_color.r * 0.92f + 255 * 0.08f);
            bg_color.g = (unsigned char)(bg_color.g * 0.92f + 255 * 0.08f);
            bg_color.b = (unsigned char)(bg_color.b * 0.92f + 255 * 0.08f);
        } else {
            bg_color = (Color){128, 128, 128, 30};
        }
    }

    if (bg_color.a > 0) {
        if (radius > 0) {
            backend_render_rounded_rect(&item_rect, bg_color, radius);
        } else {
            backend_render_fill_rect(&item_rect, bg_color);
        }
    }

    ButtonComponent* btn_template = NULL;
    if (template_layer->type == BUTTON && template_layer->component) {
        btn_template = (ButtonComponent*)template_layer->component;
    }

    Layer* font_layer = list_resolve_font_layer(layer, template_layer);
    if (font_layer && font_layer->font && !font_layer->font->default_font) {
        load_all_fonts(font_layer);
    }

    int content_x = item_rect.x + 10;
    int text_y_center = item_rect.y + item_rect.h / 2;
    int text_x_offset = 0;

    if (btn_template && btn_template->icon_text && btn_template->icon_text[0]) {
        char* icon_text = list_resolve_template(component, index, btn_template->icon_text);
        if (icon_text && icon_text[0]) {
            Texture* icon_tex = render_text(font_layer, icon_text, text_color);
            if (icon_tex) {
                int iw, ih;
                backend_query_texture(icon_tex, NULL, NULL, &iw, &ih);
                int icon_max = btn_template->icon_size > 0 ? btn_template->icon_size : 18;
                int icon_w = iw / (int)scale;
                int icon_h = ih / (int)scale;
                list_fit_texture(&item_rect, &icon_w, &icon_h, icon_max, icon_max);
                int icon_y = text_y_center - icon_h / 2;
                Rect icon_dst = {content_x, icon_y, icon_w, icon_h};
                backend_render_text_copy(icon_tex, NULL, &icon_dst);
                backend_render_text_destroy(icon_tex);
                text_x_offset = icon_w + 8;
            }
        }
        if (icon_text) free(icon_text);
    }

    const char* text_template = layer_get_text(template_layer);
    char* item_text = list_resolve_template(component, index, text_template);
    if (item_text && item_text[0]) {
        Texture* text_tex = render_text(font_layer, item_text, text_color);
        if (text_tex) {
            int tw, th;
            backend_query_texture(text_tex, NULL, NULL, &tw, &th);
            int draw_w = tw / (int)scale;
            int draw_h = th / (int)scale;
            int avail_w = item_rect.w - text_x_offset - 14;
            int avail_h = item_rect.h - 8;
            list_fit_texture(&item_rect, &draw_w, &draw_h, avail_w, avail_h);
            int text_x = content_x + text_x_offset;
            int text_y = text_y_center - draw_h / 2;
            Rect dst = {text_x, text_y, draw_w, draw_h};
            backend_render_text_copy(text_tex, NULL, &dst);
            backend_render_text_destroy(text_tex);
        }
    }
    if (item_text) free(item_text);
}

void list_component_render(Layer* layer) {
    if (!layer || !layer->component) return;

    ListComponent* component = (ListComponent*)layer->component;
    list_sync_item_metrics(component);
    list_component_update_content_size(component);

    Rect prev_clip;
    backend_render_get_clip_rect(&prev_clip);
    backend_render_set_clip_rect(&layer->rect);

    if (layer->bg_color.a > 0) {
        if (layer->radius > 0) {
            backend_render_rounded_rect(&layer->rect, layer->bg_color, layer->radius);
        } else {
            backend_render_fill_rect(&layer->rect, layer->bg_color);
        }
    }

    int count = list_component_get_item_count(component);
    for (int i = 0; i < count; i++) {
        list_render_item(component, i,
                         component->hovered_index == i,
                         component->pressed_index == i);
    }

    backend_render_set_clip_rect(&prev_clip);
}

int list_component_handle_mouse_event(Layer* layer, MouseEvent* event) {
    if (!layer || !event || !layer->component) return 0;

    ListComponent* component = (ListComponent*)layer->component;
    int index = -1;
    int inside = list_point_in_item(component, event->x, event->y, &index);

    if (event->state == SDL_MOUSEMOTION) {
        component->hovered_index = inside ? index : -1;
        if (!inside || component->pressed_index < 0) {
            if (!inside) component->pressed_index = -1;
        }
        return inside;
    }

    if (event->button != SDL_BUTTON_LEFT) {
        return inside;
    }

    if (event->state == SDL_PRESSED) {
        component->pressed_index = inside ? index : -1;
        component->hovered_index = inside ? index : -1;
        return inside;
    }

    if (event->state == SDL_RELEASED) {
        int clicked = inside && component->pressed_index == index && index >= 0;
        component->pressed_index = -1;
        if (clicked) {
            list_dispatch_item_click(component, index);
        }
        return inside;
    }

    return inside;
}

int list_component_handle_key_event(Layer* layer, KeyEvent* event) {
    if (!layer || !event || !layer->component) return 0;

    ListComponent* component = (ListComponent*)layer->component;
    if (event->type != KEY_EVENT_DOWN) return 0;

    int step = component->item_height + list_get_spacing(component);
    if (step <= 0) step = component->item_height;

    switch (event->data.key.key_code) {
        case SDLK_UP:
            layer->scroll_offset -= step;
            if (layer->scroll_offset < 0) layer->scroll_offset = 0;
            return 1;
        case SDLK_DOWN:
            layer->scroll_offset += step;
            {
                int max_offset = layer->content_height - layer->rect.h;
                if (max_offset < 0) max_offset = 0;
                if (layer->scroll_offset > max_offset) layer->scroll_offset = max_offset;
            }
            return 1;
        case SDLK_RETURN:
        case SDLK_SPACE:
            if (component->hovered_index >= 0) {
                list_dispatch_item_click(component, component->hovered_index);
                return 1;
            }
            return 0;
        default:
            return 0;
    }
}
