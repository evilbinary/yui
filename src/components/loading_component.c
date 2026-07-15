#include "loading_component.h"
#include "../render.h"
#include "../backend.h"
#include "../util.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static void loading_layer_destroy(Layer* layer)
{
    if (!layer || !layer->component) {
        return;
    }
    loading_component_destroy((LoadingComponent*)layer->component);
    layer->component = NULL;
}

static void loading_parse_color(const char* color_str, Color* out)
{
    if (!color_str || !out) {
        return;
    }
    if (color_str[0] == '#') {
        unsigned int r = 0, g = 0, b = 0;
        sscanf(color_str + 1, "%02x%02x%02x", &r, &g, &b);
        *out = (Color){(unsigned char)r, (unsigned char)g, (unsigned char)b, 255};
    } else {
        parse_color((char*)color_str, out);
    }
}

LoadingComponent* loading_component_create(Layer* layer)
{
    if (!layer) {
        return NULL;
    }

    LoadingComponent* component = (LoadingComponent*)calloc(1, sizeof(LoadingComponent));
    if (!component) {
        return NULL;
    }

    component->layer = layer;
    component->variant = LOADING_VARIANT_SPINNER;
    component->color = (Color){33, 150, 243, 255};
    component->track_color = (Color){220, 220, 220, 255};
    component->stroke_width = 4;
    component->speed = 1.0f;

    layer->component = component;
    layer->render = loading_component_render;
    layer->on_destroy = loading_layer_destroy;

    return component;
}

LoadingComponent* loading_component_create_from_json(Layer* layer, cJSON* json_obj)
{
    LoadingComponent* component = loading_component_create(layer);
    if (!component) {
        return NULL;
    }

    cJSON* style = json_obj ? cJSON_GetObjectItem(json_obj, "style") : NULL;

    cJSON* variant_item = json_obj ? cJSON_GetObjectItem(json_obj, "variant") : NULL;
    if (variant_item && cJSON_IsString(variant_item)) {
        if (strcmp(variant_item->valuestring, "dots") == 0) {
            component->variant = LOADING_VARIANT_DOTS;
        } else {
            component->variant = LOADING_VARIANT_SPINNER;
        }
    }

    cJSON* color_item = json_obj ? cJSON_GetObjectItem(json_obj, "color") : NULL;
    if (!color_item && style) {
        color_item = cJSON_GetObjectItem(style, "color");
    }
    if (!color_item && json_obj) {
        color_item = cJSON_GetObjectItem(json_obj, "fillColor");
    }
    if (color_item && cJSON_IsString(color_item)) {
        loading_parse_color(color_item->valuestring, &component->color);
    }

    cJSON* track_item = json_obj ? cJSON_GetObjectItem(json_obj, "trackColor") : NULL;
    if (!track_item && style) {
        track_item = cJSON_GetObjectItem(style, "trackColor");
    }
    if (track_item && cJSON_IsString(track_item)) {
        loading_parse_color(track_item->valuestring, &component->track_color);
    }

    cJSON* stroke_item = json_obj ? cJSON_GetObjectItem(json_obj, "strokeWidth") : NULL;
    if (!stroke_item && style) {
        stroke_item = cJSON_GetObjectItem(style, "strokeWidth");
    }
    if (stroke_item && cJSON_IsNumber(stroke_item)) {
        component->stroke_width = stroke_item->valueint;
        if (component->stroke_width < 1) {
            component->stroke_width = 1;
        }
    }

    cJSON* speed_item = json_obj ? cJSON_GetObjectItem(json_obj, "speed") : NULL;
    if (!speed_item && json_obj) {
        speed_item = cJSON_GetObjectItem(json_obj, "animationSpeed");
    }
    if (speed_item && cJSON_IsNumber(speed_item)) {
        component->speed = (float)speed_item->valuedouble;
        if (component->speed < 0.1f) {
            component->speed = 0.1f;
        }
    }

    return component;
}

void loading_component_destroy(LoadingComponent* component)
{
    if (component) {
        free(component);
    }
}

static void loading_render_spinner(LoadingComponent* component, Layer* layer, int cx, int cy, int size)
{
    int radius = size / 2;
    if (radius < 4) {
        return;
    }

    float angle = (float)(backend_get_ticks() % 36000) / 10.0f * component->speed;
    backend_render_arc(cx, cy, radius, 0.0f, 360.0f, component->track_color, component->stroke_width);
    backend_render_arc(cx, cy, radius, angle - 45.0f, angle + 45.0f,
                       component->color, component->stroke_width);
}

static void loading_render_dots(LoadingComponent* component, Layer* layer, int cx, int cy, int size)
{
    int dot_radius = size / 10;
    if (dot_radius < 2) {
        dot_radius = 2;
    }
    int spacing = dot_radius * 3;
    float phase = (float)backend_get_ticks() * 0.006f * component->speed;

    for (int i = 0; i < 3; i++) {
        float wave = (sinf(phase + (float)i * 1.2f) + 1.0f) * 0.5f;
        int dy = (int)((1.0f - wave) * (size / 8.0f));
        Color dot_color = component->color;
        dot_color.a = (unsigned char)(120 + (int)(135.0f * wave));
        Rect dot = {
            cx + (i - 1) * spacing - dot_radius,
            cy + dy - dot_radius,
            dot_radius * 2,
            dot_radius * 2
        };
        backend_render_fill_rect(&dot, dot_color);
    }
}

static void loading_render_text(LoadingComponent* component, Layer* layer)
{
    if (!layer->text || !layer->text[0]) {
        return;
    }

    DFont* text_font = NULL;
    if (layer->font && layer->font->default_font) {
        text_font = layer->font->default_font;
    } else if (layer->parent && layer->parent->font && layer->parent->font->default_font) {
        text_font = layer->parent->font->default_font;
    }
    if (!text_font) {
        return;
    }

    Color text_color = layer->color.a > 0 ? layer->color : component->color;
    Texture* text_texture = backend_render_texture(text_font, layer->text, text_color);
    if (!text_texture) {
        return;
    }

    int text_width = 0;
    int text_height = 0;
    backend_query_texture(text_texture, NULL, NULL, &text_width, &text_height);

    int draw_w = text_width / scale;
    int draw_h = text_height / scale;
    if (draw_w < 1) draw_w = 1;
    if (draw_h < 1) draw_h = 1;

    int max_w = layer->rect.w - 4;
    if (max_w < 1) max_w = 1;
    if (draw_w > max_w) {
        float fit = (float)max_w / (float)draw_w;
        draw_w = max_w;
        draw_h = (int)(draw_h * fit);
        if (draw_h < 1) draw_h = 1;
    }

    Rect text_rect = {
        layer->rect.x + (layer->rect.w - draw_w) / 2,
        layer->rect.y + layer->rect.h - draw_h - 2,
        draw_w,
        draw_h
    };
    backend_render_text_copy(text_texture, NULL, &text_rect);
    backend_render_text_destroy(text_texture);
}

void loading_component_render(Layer* layer)
{
    if (!layer || !layer->component) {
        return;
    }

    LoadingComponent* component = (LoadingComponent*)layer->component;
    int has_text = layer->text && layer->text[0];
    int indicator_h = has_text ? (layer->rect.h * 2) / 3 : layer->rect.h;
    if (indicator_h < 8) {
        indicator_h = layer->rect.h;
    }

    int size = indicator_h;
    if (layer->rect.w < size) {
        size = layer->rect.w;
    }
    if (size < 8) {
        return;
    }

    int cx = layer->rect.x + layer->rect.w / 2;
    int cy = layer->rect.y + indicator_h / 2;

    if (component->variant == LOADING_VARIANT_DOTS) {
        loading_render_dots(component, layer, cx, cy, size);
    } else {
        loading_render_spinner(component, layer, cx, cy, size - component->stroke_width * 2);
    }

    loading_render_text(component, layer);
}
