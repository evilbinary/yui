#include "dropdown_component.h"
#include "../backend.h"
#include "../event.h"
#include "../util.h"
#include "../layer.h"
#include <stdlib.h>
#include <string.h>

// 创建下拉菜单组件
DropdownComponent* dropdown_component_create(Layer* layer) {
    DropdownComponent* component = (DropdownComponent*)malloc(sizeof(DropdownComponent));
    if (!component) return NULL;
    
    component->layer = layer;
    component->dropdown_layer = NULL;
    component->items = NULL;
    component->item_count = 0;
    component->selected_index = -1;
    component->expanded = 0;
    component->max_visible_items = 5;
    
    // 直接使用Color结构体赋值，不再使用color_from_hex
    component->bg_color = (Color){255, 255, 255, 255}; // #FFFFFF
    component->text_color = (Color){0, 0, 0, 255};     // #000000
    component->arrow_color = (Color){0, 0, 0, 255};    // #000000
    component->dropdown_bg_color = (Color){255, 255, 255, 255}; // #FFFFFF
    component->selected_bg_color = (Color){0, 120, 215, 255};   // #0078D7
    component->selected_text_color = (Color){255, 255, 255, 255}; // #FFFFFF
    
    component->user_data = NULL;
    component->on_selection_changed = NULL;
    
    // 设置组件指针
    layer->component = component;
    
    return component;
}

// 添加项目
void dropdown_component_add_item(DropdownComponent* component, const char* text, void* user_data) {
    if (!component || !text) return;
    
    // 扩展项目数组
    component->items = (DropdownItem*)realloc(component->items, (component->item_count + 1) * sizeof(DropdownItem));
    if (!component->items) return;
    
    // 添加新项目
    DropdownItem* item = &component->items[component->item_count];
    item->text = strdup(text);
    item->user_data = user_data;
    item->selected = 0;
    
    component->item_count++;
    
    // 如果是第一个项目，自动选中
    if (component->item_count == 1) {
        dropdown_component_set_selected(component, 0);
    }
}

// 移除项目
void dropdown_component_remove_item(DropdownComponent* component, int index) {
    if (!component || index < 0 || index >= component->item_count) return;
    
    // 释放项目文本
    free(component->items[index].text);
    
    // 移动后面的项目
    for (int i = index; i < component->item_count - 1; i++) {
        component->items[i] = component->items[i + 1];
    }
    
    component->item_count--;
    
    // 调整选中索引
    if (component->selected_index >= component->item_count) {
        component->selected_index = component->item_count - 1;
    }
}

// 清空所有项目
void dropdown_component_clear_items(DropdownComponent* component) {
    if (!component) return;
    
    // 释放所有项目文本
    for (int i = 0; i < component->item_count; i++) {
        free(component->items[i].text);
    }
    
    // 释放项目数组
    free(component->items);
    component->items = NULL;
    component->item_count = 0;
    component->selected_index = -1;
}

// 设置选中项
void dropdown_component_set_selected(DropdownComponent* component, int index) {
    if (!component || index < -1 || index >= component->item_count) return;
    
    int old_index = component->selected_index;
    component->selected_index = index;
    
    // 清除所有选中状态
    for (int i = 0; i < component->item_count; i++) {
        component->items[i].selected = 0;
    }
    
    // 设置新的选中状态
    if (index >= 0) {
        component->items[index].selected = 1;
    }
    
    // 调用回调函数
    if (old_index != index && component->on_selection_changed) {
        component->on_selection_changed(index, component->user_data);
    }
}

// 获取选中项索引
int dropdown_component_get_selected(DropdownComponent* component) {
    if (!component) return -1;
    return component->selected_index;
}

// 设置颜色
void dropdown_component_set_colors(DropdownComponent* component, Color bg, Color text, Color arrow, 
                                  Color dropdown_bg, Color selected_bg, Color selected_text) {
    if (!component) return;
    component->bg_color = bg;
    component->text_color = text;
    component->arrow_color = arrow;
    component->dropdown_bg_color = dropdown_bg;
    component->selected_bg_color = selected_bg;
    component->selected_text_color = selected_text;
}

// 设置用户数据
void dropdown_component_set_user_data(DropdownComponent* component, void* data) {
    if (!component) return;
    component->user_data = data;
}

// 设置选择变化回调
void dropdown_component_set_selection_changed_callback(DropdownComponent* component, void (*callback)(int, void*)) {
    if (!component) return;
    component->on_selection_changed = callback;
}

// 创建下拉菜单图层
void dropdown_create_dropdown_layer(DropdownComponent* component) {
    if (!component || !component->layer) return;
    
    // 计算下拉菜单的高度
    int item_height = 25;
    int dropdown_height = item_height * component->item_count;
    if (dropdown_height > component->max_visible_items * item_height) {
        dropdown_height = component->max_visible_items * item_height;
    }
    
    // 由于layer_add_child等函数未定义，简化处理
    component->dropdown_layer = NULL;
}

// 销毁下拉菜单图层
void dropdown_destroy_dropdown_layer(DropdownComponent* component) {
    if (!component) return;

    
    component->dropdown_layer = NULL;
}

// 销毁下拉菜单组件
void dropdown_component_destroy(DropdownComponent* component) {
    if (!component) return;
    
    // 释放所有项目
    dropdown_component_clear_items(component);
    
    free(component);
}

// 处理鼠标事件
void dropdown_component_handle_mouse_event(Layer* layer, MouseEvent* event) {
    if (!layer || !event || !layer->component) return;
    
    DropdownComponent* component = (DropdownComponent*)layer->component;
    
    if (event->state == SDL_PRESSED && event->button == SDL_BUTTON_LEFT) {
        // 检查是否点击在下拉菜单按钮上
        if (event->x >= layer->rect.x && event->x < layer->rect.x + layer->rect.w &&
            event->y >= layer->rect.y && event->y < layer->rect.y + layer->rect.h) {
            // 切换展开状态
            component->expanded = !component->expanded;
            
            if (component->expanded) {
                // 创建下拉菜单图层
                dropdown_create_dropdown_layer(component);
            } else {
                // 销毁下拉菜单图层
                dropdown_destroy_dropdown_layer(component);
            }
        }
        // 检查是否点击在下拉菜单项目上
        else if (component->expanded && component->dropdown_layer) {
            int item_height = 25;
            int clicked_index = (event->y - component->dropdown_layer->rect.y) / item_height;
            
            if (clicked_index >= 0 && clicked_index < component->item_count) {
                dropdown_component_set_selected(component, clicked_index);
                component->expanded = 0;
                dropdown_destroy_dropdown_layer(component);
            }
        }
        // 点击其他地方，关闭下拉菜单
        else if (component->expanded) {
            component->expanded = 0;
            dropdown_destroy_dropdown_layer(component);
        }
    }
}

// 处理键盘事件
void dropdown_component_handle_key_event(Layer* layer, KeyEvent* event) {
    if (!layer || !event || !layer->component) return;
    
    DropdownComponent* component = (DropdownComponent*)layer->component;
    
    if (event->type == KEY_EVENT_DOWN) {
        switch (event->data.key.key_code) {
            case SDLK_UP:
                if (component->selected_index > 0) {
                    dropdown_component_set_selected(component, component->selected_index - 1);
                }
                break;
                
            case SDLK_DOWN:
                if (component->selected_index < component->item_count - 1) {
                    dropdown_component_set_selected(component, component->selected_index + 1);
                }
                break;
                
            case SDLK_RETURN:
            case SDLK_SPACE:
                // 切换展开状态
                component->expanded = !component->expanded;
                
                if (component->expanded) {
                    dropdown_create_dropdown_layer(component);
                } else {
                    dropdown_destroy_dropdown_layer(component);
                }
                break;
                
            case SDLK_ESCAPE:
                // 关闭下拉菜单
                if (component->expanded) {
                    component->expanded = 0;
                    dropdown_destroy_dropdown_layer(component);
                }
                break;
        }
    }
}

// 渲染下拉菜单
void dropdown_component_render(Layer* layer) {
    if (!layer || !layer->component) return;
    
    DropdownComponent* component = (DropdownComponent*)layer->component;
    
    // 绘制背景
    backend_render_fill_rect(&layer->rect, component->bg_color);
    
    // 绘制选中项的文本
    if (component->selected_index >= 0 && component->selected_index < component->item_count) {
        DropdownItem* item = &component->items[component->selected_index];
        
        // 使用backend_render_fill_rect直接绘制文本区域
        Rect text_rect = {layer->rect.x + 10, layer->rect.y + (layer->rect.h - 16) / 2, 0, 0};
        // 由于没有backend_render_text函数，我们使用简单的矩形来模拟文本
        text_rect.w = strlen(item->text) * 10; // 粗略估算文本宽度
        text_rect.h = 16;
        backend_render_fill_rect(&text_rect, component->text_color);
    }
    
    // 绘制下拉箭头 - 使用矩形模拟
    int arrow_width = 12;
    int arrow_height = 8;
    int arrow_x = layer->rect.x + layer->rect.w - 24;
    int arrow_y = layer->rect.y + (layer->rect.h - arrow_height) / 2;
    
    // 绘制一个简单的三角形箭头
    for (int i = 0; i < arrow_height; i++) {
        int bar_width = arrow_width - (i * 2);
        if (bar_width <= 0) break;
        
        Rect bar_rect = {
            arrow_x + i,
            arrow_y + i,
            bar_width,
            1
        };
        backend_render_fill_rect(&bar_rect, component->arrow_color);
    }
    
    // 渲染下拉菜单图层
    if (component->expanded && component->dropdown_layer) {
        // 绘制背景
        backend_render_fill_rect(&component->dropdown_layer->rect, component->dropdown_bg_color);
        
        // 绘制项目
        int item_height = 25;
        int visible_count = component->dropdown_layer->rect.h / item_height;
        if (visible_count > component->item_count) {
            visible_count = component->item_count;
        }
        
        for (int i = 0; i < visible_count; i++) {
            DropdownItem* item = &component->items[i];
            int item_y = component->dropdown_layer->rect.y + i * item_height;
            
            // 绘制选中项背景
            if (item->selected) {
                Rect item_rect = {component->dropdown_layer->rect.x + 2, item_y, component->dropdown_layer->rect.w - 4, item_height};
                backend_render_fill_rect(&item_rect, component->selected_bg_color);
            }
            
            // 绘制项目文本（使用矩形模拟）
            Color text_color = item->selected ? component->selected_text_color : component->text_color;
            Rect text_rect = {component->dropdown_layer->rect.x + 5, item_y + (item_height - 16) / 2, 0, 0};
            text_rect.w = strlen(item->text) * 10; // 粗略估算文本宽度
            text_rect.h = 16;
            backend_render_fill_rect(&text_rect, text_color);
        }
    }
}