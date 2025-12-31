#include "../render.h"
#include "../backend.h"
#include <stdlib.h>
#include <string.h>
#include "radiobox_component.h"

// 单选框组的全局列表
#define MAX_GROUPS 10
static RadioGroup groups[MAX_GROUPS] = {0};
static int group_count = 0;

// 创建单选框组件
RadioboxComponent* radiobox_component_create(Layer* layer, const char* group_id, int default_checked) {
    if (!layer) {
        return NULL;
    }
    
    RadioboxComponent* component = (RadioboxComponent*)malloc(sizeof(RadioboxComponent));
    if (!component) {
        return NULL;
    }
    
    memset(component, 0, sizeof(RadioboxComponent));
    component->layer = layer;
    component->checked = default_checked;
    
    if(default_checked) {
        radiobox_set_group_checked(component->group_id, component);
    }
    component->user_data = NULL;
    
    // 设置组ID
    if (group_id) {
        strncpy(component->group_id, group_id, sizeof(component->group_id) - 1);
    } else {
        strcpy(component->group_id, "default");
    }
    
    // 设置默认颜色
    component->bg_color = (Color){255, 255, 255, 255};         // 白色背景
    component->border_color = (Color){100, 149, 237, 255};     // 蓝色边框
    component->dot_color = (Color){25, 25, 112, 255};          // 深蓝色圆点
    
    // 设置组件指针和自定义渲染函数
    layer->component = component;
    layer->render = radiobox_component_render;
    
    // 绑定事件处理函数
    layer->handle_mouse_event = radiobox_component_handle_mouse_event;
    
    // 设置组件为可聚焦
    layer->focusable = !HAS_STATE(layer, LAYER_STATE_DISABLED);
    
    // 将单选框添加到组中
    radiobox_add_to_group(component);
    
    return component;
}

RadioboxComponent* radiobox_component_create_from_json(Layer* layer, cJSON* json_obj){
    if (!layer || !json_obj) {
        return NULL;
    }
    
    // 从JSON中获取组ID
    const char* group_id = "default";
    if (cJSON_HasObjectItem(json_obj, "group")) {
        group_id = cJSON_GetObjectItem(json_obj, "group")->valuestring;
    }
    
    // 从JSON中获取默认选中状态
    int default_checked = 0;
    if (cJSON_HasObjectItem(json_obj, "checked")) {
        default_checked = cJSON_IsTrue(cJSON_GetObjectItem(json_obj, "checked"));
    }
    
    // 创建单选框组件
    return radiobox_component_create(layer, group_id, default_checked);
}

// 销毁单选框组件
void radiobox_component_destroy(RadioboxComponent* component) {
    if (component) {
        // 从组中移除
        radiobox_remove_from_group(component);
        free(component);
    }
}

// 设置单选框选中状态
void radiobox_component_set_checked(RadioboxComponent* component, int checked) {
    if (component) {
        if (checked) {
            // 如果要选中当前单选框，取消同组内其他单选框的选中状态
            radiobox_set_group_checked(component->group_id, component);
        } else {
            component->checked = 0;
            // 清除图层状态，确保重绘
            CLEAR_STATE(component->layer, LAYER_STATE_ACTIVE);
        }
    }
}

// 获取单选框选中状态
int radiobox_component_is_checked(RadioboxComponent* component) {
    if (component) {
        return component->checked;
    }
    return 0;
}

// 设置单选框颜色
void radiobox_component_set_colors(RadioboxComponent* component, Color bg_color, Color border_color, Color dot_color) {
    if (component) {
        component->bg_color = bg_color;
        component->border_color = border_color;
        component->dot_color = dot_color;
    }
}

// 设置单选框所属组
void radiobox_component_set_group(RadioboxComponent* component, const char* group_id) {
    if (component && group_id) {
        // 先从当前组移除
        radiobox_remove_from_group(component);
        
        // 设置新的组ID
        strncpy(component->group_id, group_id, sizeof(component->group_id) - 1);
        
        // 添加到新组
        radiobox_add_to_group(component);
    }
}

// 设置用户数据
void radiobox_component_set_user_data(RadioboxComponent* component, void* data) {
    if (component) {
        component->user_data = data;
    }
}

// 设置单选框禁用状态
void radiobox_component_set_disabled(RadioboxComponent* component, int disabled){
    if (component && component->layer) {
        if (disabled) {
            SET_STATE(component->layer, LAYER_STATE_DISABLED);
            // 禁用时不能获得焦点
            component->layer->focusable = 0;
        } else {
            CLEAR_STATE(component->layer, LAYER_STATE_DISABLED);
            // 启用时恢复焦点能力
            component->layer->focusable = 1;
        }
    }
}

// 获取单选框禁用状态
int radiobox_component_is_disabled(RadioboxComponent* component) {
    if (component && component->layer) {
        return HAS_STATE(component->layer, LAYER_STATE_DISABLED);
    }
    return 0;
}

// 处理鼠标事件
void radiobox_component_handle_mouse_event(Layer* layer, MouseEvent* event) {
    if (!layer || !event || !layer->component) {
        return;
    }

    // 如果图层被禁用，则不处理鼠标事件
    if (HAS_STATE(layer, LAYER_STATE_DISABLED)) {
        return;
    }

    RadioboxComponent* component = (RadioboxComponent*)layer->component;

    // 检查鼠标是否在复选框范围内
    int is_inside = (event->x >= layer->rect.x && 
        event->x < layer->rect.x + layer->rect.w &&
        event->y >= layer->rect.y && 
        event->y < layer->rect.y + layer->rect.h);
    // 处理鼠标点击事件
    if (event->button == BUTTON_LEFT && event->state == BUTTON_PRESSED && is_inside) {
        // 选中当前单选框，取消同组内其他单选框的选中状态
        radiobox_set_group_checked(component->group_id, component);

        // 如果有点击事件回调，调用它
        if (layer->event && layer->event->click) {
            layer->event->click(layer);
        }
    }
}

// 将颜色转换为灰度并降低透明度
static Color get_disabled_color(Color color) {
    // 计算灰度值
    int gray = (color.r * 0.3 + color.g * 0.59 + color.b * 0.11);
    // 返回灰度颜色并降低透明度到60%
    return (Color){
        gray,
        gray,
        gray,
        (int)(color.a * 0.6)
    };
}

// 渲染单选框
void radiobox_component_render(Layer* layer) {
    if (!layer || !layer->component) {
        return;
    }

    RadioboxComponent* component = (RadioboxComponent*)layer->component;
    int is_disabled = HAS_STATE(layer, LAYER_STATE_DISABLED);

    // 计算圆形的中心点和半径
    int center_x = layer->rect.x + layer->rect.w / 2;
    int center_y = layer->rect.y + layer->rect.h / 2;
    int radius = (layer->rect.w < layer->rect.h ? layer->rect.w : layer->rect.h) / 2 - 2;

    // 绘制圆形背景
    // 由于backend没有直接绘制圆形的函数，我们使用圆角矩形来模拟
    Rect circle_rect = {
        center_x - radius,
        center_y - radius,
        radius * 2,
        radius * 2
    };

    // 如果禁用，使用灰度和降低透明度的颜色
    Color bg_color = is_disabled ? get_disabled_color(component->bg_color) : component->bg_color;
    Color border_color = is_disabled ? get_disabled_color(component->border_color) : component->border_color;
    Color dot_color = is_disabled ? get_disabled_color(component->dot_color) : component->dot_color;
    Color text_color = is_disabled ? get_disabled_color(layer->color) : layer->color;

    backend_render_fill_rect_color(
        &circle_rect,
        bg_color.r,
        bg_color.g,
        bg_color.b,
        bg_color.a
    );

    // 绘制圆形边框
    backend_render_rect_color(
        &circle_rect,
        border_color.r,
        border_color.g,
        border_color.b,
        border_color.a
    );

    // 如果选中，绘制内部圆点
    if (component->checked) {
        int dot_radius = radius / 2;
        Rect dot_rect = {
            center_x - dot_radius,
            center_y - dot_radius,
            dot_radius * 2,
            dot_radius * 2
        };

        backend_render_fill_rect_color(
            &dot_rect,
            dot_color.r,
            dot_color.g,
            dot_color.b,
            dot_color.a
        );
    }
    // 绘制标签文本，使用layer->label和layer->color
    if (layer->label[0] != '\0' && layer->font->default_font) {
        // 计算标签位置（在复选框右侧，垂直居中）
        int label_x = layer->rect.x + layer->rect.w + 5;  // 5像素间距

        // 使用backend_render_texture渲染文本到纹理
        Texture* text_texture = backend_render_texture(layer->font->default_font, layer->label, text_color);
        if (text_texture) {
            // 获取文本纹理的尺寸
            int text_width, text_height;
            backend_query_texture(text_texture, NULL, NULL, &text_width, &text_height);

            // 计算垂直居中的Y坐标
            int label_y = layer->rect.y + (layer->rect.h - text_height/scale) / 2;

            // 创建目标矩形
            Rect dst_rect = {
                .x = label_x,
                .y = label_y,
                .w = text_width/ scale,
                .h = text_height/ scale
            };

            // 渲染文本纹理
            backend_render_text_copy(text_texture, NULL, &dst_rect);

            // 释放纹理资源
            backend_render_text_destroy(text_texture);
        }
    }
}

// 获取单选框组
RadioGroup* radiobox_get_group(const char* group_id) {
    for (int i = 0; i < group_count; i++) {
        if (strcmp(groups[i].group_id, group_id) == 0) {
            return &groups[i];
        }
    }
    return NULL;
}

// 将单选框添加到组中
void radiobox_add_to_group(RadioboxComponent* component) {
    if (!component) {
        return;
    }
    
    // 查找或创建组
    RadioGroup* group = radiobox_get_group(component->group_id);
    if (!group) {
        // 创建新组
        if (group_count >= MAX_GROUPS) {
            return;  // 组数量已达上限
        }
        
        group = &groups[group_count];
        strcpy(group->group_id, component->group_id);
        group->radio_count = 0;
        group->radios = NULL;
        group_count++;
    }
    
    // 扩展组内单选框列表
    group->radios = (CheckboxComponent**)realloc(
        group->radios,
        (group->radio_count + 1) * sizeof(CheckboxComponent*)
    );
    
    if (group->radios) {
        group->radios[group->radio_count] = (CheckboxComponent*)component;
        group->radio_count++;
    }
}

// 从组中移除单选框
void radiobox_remove_from_group(RadioboxComponent* component) {
    if (!component) {
        return;
    }
    
    RadioGroup* group = radiobox_get_group(component->group_id);
    if (!group || group->radio_count == 0) {
        return;
    }
    
    // 查找并移除单选框
    int found_index = -1;
    for (int i = 0; i < group->radio_count; i++) {
        if (group->radios[i] == (CheckboxComponent*)component) {
            found_index = i;
            break;
        }
    }
    
    if (found_index != -1) {
        // 移动其他单选框填补空缺
        for (int i = found_index; i < group->radio_count - 1; i++) {
            group->radios[i] = group->radios[i + 1];
        }
        
        group->radio_count--;
        
        // 如果组为空，释放内存
        if (group->radio_count == 0) {
            free(group->radios);
            group->radios = NULL;
            
            // 移除组（可选）
            // 这里简化处理，不实际移除组结构，只清空其内容
        }
    }
}

// 设置组内某个单选框为选中状态，取消其他单选框的选中状态
void radiobox_set_group_checked(const char* group_id, RadioboxComponent* component) {
    RadioGroup* group = radiobox_get_group(group_id);
    if (!group || !component) {
        return;
    }
    
    // 检查目标单选框是否被禁用，如果是，则不执行任何操作
    if (HAS_STATE(component->layer, LAYER_STATE_DISABLED)) {
        return;
    }
    
    // 取消组内所有单选框的选中状态，但保留禁用状态
    for (int i = 0; i < group->radio_count; i++) {
        RadioboxComponent* radio = (RadioboxComponent*)group->radios[i];
        radio->checked = 0;
        // 清除图层激活状态，确保重绘，但保留禁用状态
        radio->layer->state &= ~LAYER_STATE_ACTIVE; // 只清除激活位，保留其他位
    }
    
    // 设置指定单选框为选中状态
    component->checked = 1;
    // 设置图层激活状态，确保重绘，但保留禁用状态
    component->layer->state |= LAYER_STATE_ACTIVE; // 只设置激活位，保留其他位
}