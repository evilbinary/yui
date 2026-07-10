#include "button_component.h"
#include "../render.h"
#include "../backend.h"
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"

// 创建按钮组件（内部通用初始化）
ButtonComponent* button_component_create(Layer* layer) {
    return button_component_create_from_json(layer, NULL);
}

ButtonComponent* button_component_create_from_json(Layer* layer, cJSON* json_obj) {
    if (!layer) {
        return NULL;
    }
    
    ButtonComponent* component = (ButtonComponent*)malloc(sizeof(ButtonComponent));
    if (!component) {
        return NULL;
    }
    
    memset(component, 0, sizeof(ButtonComponent));
    component->layer = layer;
    component->user_data = NULL;
    component->icon_path = NULL;
    component->icon_text = NULL;
    component->icon_tex = NULL;
    component->icon_size = 0;
    
    // 设置默认颜色
    component->colors[LAYER_STATE_NORMAL] = (Color){100, 149, 237, 255};    // 蓝色
    component->colors[LAYER_STATE_HOVER] = (Color){135, 206, 250, 255};     // 亮蓝色
    component->colors[LAYER_STATE_PRESSED] = (Color){70, 130, 180, 255};    // 深蓝色
    component->colors[LAYER_STATE_DISABLED] = (Color){200, 200, 200, 150};  // 灰色半透明

    // 从 JSON 加载图标配置
    if (json_obj) {
        cJSON* icon_json = cJSON_GetObjectItem(json_obj, "icon");
        if (icon_json && icon_json->valuestring) {
            const char* icon_val = icon_json->valuestring;
            // 含路径分隔符或扩展名的视为图片/SVG 文件，否则为文本图标（emoji 等）
            if (strchr(icon_val, '.') || strchr(icon_val, '/') || strchr(icon_val, '\\')) {
                component->icon_path = strdup(icon_val);
            } else {
                component->icon_text = strdup(icon_val);
            }
        }

        // 检测 bgColor 是否显式设为 transparent
        cJSON* style = cJSON_GetObjectItem(json_obj, "style");
        if (style) {
            cJSON* bg = cJSON_GetObjectItem(style, "bgColor");
            if (bg && bg->valuestring && strcmp(bg->valuestring, "transparent") == 0) {
                component->bg_transparent = 1;
            }
        }

        cJSON* iconSize = cJSON_GetObjectItem(json_obj, "iconSize");
        if (iconSize) {
            component->icon_size = iconSize->valueint;
        }
    }

    // 设置组件指针和自定义渲染函数
    layer->component = component;
    layer->render = button_component_render;
    
    // 绑定事件处理函数
    layer->handle_mouse_event = button_component_handle_mouse_event;
    
    // 绑定键盘事件处理函数
    layer->handle_key_event = button_component_handle_key_event;
    
    // 设置组件为可聚焦
    layer->focusable = 1;
    
    return component;
}

// 销毁按钮组件
void button_component_destroy(ButtonComponent* component) {
    if (component) {
        if (component->icon_text) free(component->icon_text);
        if (component->icon_path) free(component->icon_path);
        if (component->icon_tex) backend_render_text_destroy(component->icon_tex);
        free(component);
    }
}

// 设置按钮文本
void button_component_set_text(ButtonComponent* component, const char* text) {
    if (!component || !text) {
        return;
    }
    
    layer_set_text(component->layer, text);
}

// 设置图标路径
void button_component_set_icon_path(ButtonComponent* component, const char* path) {
    if (!component || !path) return;
    if (component->icon_path) free(component->icon_path);
    if (component->icon_text) { free(component->icon_text); component->icon_text = NULL; }
    if (component->icon_tex) { backend_render_text_destroy(component->icon_tex); component->icon_tex = NULL; }
    component->icon_path = strdup(path);
}

// 设置图标文本
void button_component_set_icon_text(ButtonComponent* component, const char* text) {
    if (!component || !text) return;
    if (component->icon_text) free(component->icon_text);
    if (component->icon_path) { free(component->icon_path); component->icon_path = NULL; }
    if (component->icon_tex) { backend_render_text_destroy(component->icon_tex); component->icon_tex = NULL; }
    component->icon_text = strdup(text);
}

// 设置按钮颜色
void button_component_set_color(ButtonComponent* component, LayerState state, Color color) {
    if (!component || state < 0 || state >= 4) {
        return;
    }
    
    component->colors[state] = color;
}

// 设置用户数据
void button_component_set_user_data(ButtonComponent* component, void* data) {
    if (!component) {
        return;
    }
    
    component->user_data = data;
}

// 处理键盘事件
void button_component_handle_key_event(Layer* layer, KeyEvent* event) {
    ButtonComponent* component = (ButtonComponent*)layer->component;
    if (!component || HAS_STATE(layer, LAYER_STATE_DISABLED)) {
        return;
    }

    // 处理回车键或空格键按下事件
    if (event->type == KEY_EVENT_DOWN && (event->data.key.key_code == 13 || event->data.key.key_code == 32)) {
        // 设置 PRESSED 状态
        SET_STATE(layer, LAYER_STATE_PRESSED);
    }

    // 处理按键释放事件，触发点击并恢复按钮状态
    if (event->type == KEY_EVENT_UP && (event->data.key.key_code == 13 || event->data.key.key_code == 32)) {
        // 触发点击事件（如果存在）
        if (HAS_STATE(layer, LAYER_STATE_PRESSED)) {
            CLEAR_STATE(layer, LAYER_STATE_PRESSED);
            // 在按键释放时触发点击事件
            if (layer->event && layer->event->click) {
                layer->event->click(layer);
            }
        }
    }
}

// 处理鼠标事件
void button_component_handle_mouse_event(Layer* layer, MouseEvent* event) {
    ButtonComponent* component = (ButtonComponent*)layer->component;
    if (!component || HAS_STATE(layer, LAYER_STATE_DISABLED)) {
        return;
    }

    // 检查鼠标是否在按钮范围内
    int is_inside = (event->x >= layer->rect.x &&
                     event->x < layer->rect.x + layer->rect.w &&
                     event->y >= layer->rect.y &&
                     event->y < layer->rect.y + layer->rect.h);

    // 更新悬停状态
    if (is_inside) {
        SET_STATE(layer, LAYER_STATE_HOVER);
    } else {
        CLEAR_STATE(layer, LAYER_STATE_HOVER);
    }

    // 鼠标按下事件：设置 PRESSED 状态
    if (event->state == SDL_PRESSED) {
        if (is_inside) {
            SET_STATE(layer, LAYER_STATE_PRESSED);
        }
    }
    // 鼠标释放事件：触发点击
    else if (event->state == SDL_RELEASED) {
        if (HAS_STATE(layer, LAYER_STATE_PRESSED)) {
            if (layer->event && layer->event->click) {
                layer->event->click(layer);
            }
        }
        CLEAR_STATE(layer, LAYER_STATE_PRESSED);
    }
    // 鼠标移动事件：不清除 PRESSED 状态
    else if (event->state == SDL_MOUSEMOTION) {
        // 移动时不清除 PRESSED 状态，保持按下状态
    }
}

// 渲染按钮组件
void button_component_render(Layer* layer) {
    if (!layer || !layer->component) {
        return;
    }
    
    ButtonComponent* component = (ButtonComponent*)layer->component;
    
    // 背景色选择
    Color bg_color;
    int has_bg = 1;
    if (component->bg_transparent) {
        // 透明背景：正常态不画，交互态用中性半透明叠加色
        if (HAS_STATE(layer, LAYER_STATE_HOVER) || HAS_STATE(layer, LAYER_STATE_PRESSED) || HAS_STATE(layer, LAYER_STATE_FOCUSED)) {
            if (HAS_STATE(layer, LAYER_STATE_PRESSED)) {
                bg_color = (Color){128, 128, 128, 50};
            } else if (HAS_STATE(layer, LAYER_STATE_HOVER)) {
                bg_color = (Color){128, 128, 128, 30};
            } else {
                bg_color = (Color){128, 128, 128, 20};
            }
        } else {
            bg_color = (Color){0, 0, 0, 0};  // 正常态透明
            has_bg = 0;
        }
    } else if (layer->bg_color.a > 0) {
        bg_color = layer->bg_color;
    } else {
        // 根据状态使用组件默认色
        if (HAS_STATE(layer, LAYER_STATE_PRESSED)) {
            bg_color = component->colors[LAYER_STATE_PRESSED];
        } else if (HAS_STATE(layer, LAYER_STATE_HOVER)) {
            bg_color = component->colors[LAYER_STATE_HOVER];
        } else if (HAS_STATE(layer, LAYER_STATE_FOCUSED)) {
            bg_color = component->colors[LAYER_STATE_FOCUSED];
        } else if (HAS_STATE(layer, LAYER_STATE_DISABLED)) {
            bg_color = component->colors[LAYER_STATE_DISABLED];
        } else {
            bg_color = component->colors[LAYER_STATE_NORMAL];
        }
    }

    // 绘制背景
    if (bg_color.a > 0) {
        // 先渲染背景色
        if (layer->radius > 0) {
            backend_render_rounded_rect(&layer->rect, bg_color, layer->radius);
        } else {
            backend_render_fill_rect(&layer->rect, bg_color);
        }
        
        // 如果启用了毛玻璃效果，在背景色上应用效果（混合模式）
        if (layer->backdrop_filter) {
            backend_render_backdrop_filter(&layer->rect, layer->blur_radius, layer->saturation, layer->brightness);
        }
    }
    
    // 绘制边框（仅当有背景色时）
    if (has_bg) {
        Color border_color = (Color){200, 200, 200, 255};
        if (HAS_STATE(layer, LAYER_STATE_PRESSED)) {
            border_color = (Color){100, 100, 100, 255};
        } else if (HAS_STATE(layer, LAYER_STATE_HOVER)) {
            border_color = (Color){150, 150, 150, 255};
        } else if (HAS_STATE(layer, LAYER_STATE_FOCUSED)) {
            border_color = (Color){50, 150, 255, 255}; // 聚焦状态使用高亮边框
        }
        
        if (layer->radius > 0) {
            backend_render_rounded_rect_with_border(&layer->rect, bg_color, layer->radius, 1, border_color);
        } else {
            backend_render_rect_color(&layer->rect, border_color.r, border_color.g, border_color.b, border_color.a);
        }
    }
    
    // 渲染图标与文本
    const char* layer_text = layer_get_text(layer);
    int has_text = layer_text && layer_text[0] != '\0';
    int has_icon = component->icon_path || component->icon_text;
    if (!has_text && !has_icon) {
        return;
    }

    Color text_color = layer->color;
    if (HAS_STATE(layer, LAYER_STATE_DISABLED)) {
        text_color = (Color){255, 255, 255, 150};
    }

    int text_x_offset = 0;
    int text_y_center = layer->rect.y + layer->rect.h / 2;

    // --- 渲染图标（优先 SVG/图片，回退到文本图标） ---
    Texture* icon_tex = NULL;
    int icon_tex_owned = 0;
    if (component->icon_path && !component->icon_tex) {
        component->icon_tex = backend_load_texture(component->icon_path);
    }
    if (component->icon_tex) {
        icon_tex = component->icon_tex;
    } else if (component->icon_text) {
        icon_tex = render_text(layer, component->icon_text, text_color);
        icon_tex_owned = 1;
    }

    if (icon_tex) {
        int iw, ih;
        backend_query_texture(icon_tex, NULL, NULL, &iw, &ih);
        int icon_max = component->icon_size > 0 ? component->icon_size : layer->rect.h - 8;
        int icon_w = iw / scale;
        int icon_h = ih / scale;
        if (icon_w > icon_max || icon_h > icon_max) {
            float ratio = (float)icon_w / icon_h;
            if (ratio > 1.0f) {
                icon_w = icon_max;
                icon_h = (int)(icon_max / ratio);
            } else {
                icon_h = icon_max;
                icon_w = (int)(icon_max * ratio);
            }
        }
        int icon_x = has_text ? layer->rect.x + 6 : layer->rect.x + (layer->rect.w - icon_w) / 2;
        int icon_y = text_y_center - icon_h / 2;
        Rect icon_rect = {icon_x, icon_y, icon_w, icon_h};
        backend_render_text_copy(icon_tex, NULL, &icon_rect);
        if (icon_tex_owned) backend_render_text_destroy(icon_tex);
        if (has_text) {
            text_x_offset = icon_w + 6;
        }
    }

    // --- 渲染文本 ---
    if (has_text) {
        Texture* text_texture = render_text(layer, layer_text, text_color);

        if (text_texture) {
            int text_width, text_height;
            backend_query_texture(text_texture, NULL, NULL, &text_width, &text_height);

            int text_x, text_y;
            if (text_x_offset > 0) {
                int total_w = layer->rect.w - text_x_offset - 10;
                text_x = layer->rect.x + text_x_offset + (total_w - text_width / scale) / 2;
                if (text_x < layer->rect.x + text_x_offset) {
                    text_x = layer->rect.x + text_x_offset;
                }
            } else {
                text_x = layer->rect.x + (layer->rect.w - text_width / scale) / 2;
            }
            text_y = layer->rect.y + (layer->rect.h - text_height / scale) / 2;

            Rect text_rect = {text_x, text_y, text_width / scale, text_height / scale};
            backend_render_text_copy(text_texture, NULL, &text_rect);
            backend_render_text_destroy(text_texture);
        }
    }
}