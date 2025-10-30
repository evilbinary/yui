#include "tab_component.h"
#include "../backend.h"
#include "../event.h"
#include "../util.h"
#include <stdlib.h>
#include <string.h>

// 创建选项卡组件
TabComponent* tab_component_create(Layer* layer) {
    TabComponent* component = (TabComponent*)malloc(sizeof(TabComponent));
    if (!component) return NULL;
    
    component->layer = layer;
    component->tabs = NULL;
    component->tab_count = 0;
    component->active_tab = -1;
    component->tab_height = 30;
    component->tab_color = (Color){240, 240, 240, 255};
    component->active_tab_color = (Color){255, 255, 255, 255};
    component->text_color = (Color){51, 51, 51, 255};
    component->active_text_color = (Color){0, 0, 0, 255};
    component->border_color = (Color){204, 204, 204, 255};
    component->closable = 0;
    component->user_data = NULL;
    component->on_tab_changed = NULL;
    component->on_tab_close = NULL;
    
    // 设置组件类型
    layer->component = component;
    
    return component;
}

// 销毁选项卡组件
void tab_component_destroy(TabComponent* component) {
    if (!component) return;
    
    // 释放所有选项卡
    if (component->tabs) {
        for (int i = 0; i < component->tab_count; i++) {
            if (component->tabs[i].title) {
                free(component->tabs[i].title);
            }
        }
        free(component->tabs);
    }
    
    free(component);
}

// 添加选项卡
int tab_component_add_tab(TabComponent* component, const char* title, Layer* content_layer) {
    if (!component || !title) return -1;
    
    // 扩展选项卡数组
    TabItem* new_tabs = (TabItem*)realloc(component->tabs, (component->tab_count + 1) * sizeof(TabItem));
    if (!new_tabs) return -1;
    
    component->tabs = new_tabs;
    
    // 设置新选项卡
    TabItem* tab = &component->tabs[component->tab_count];
    tab->title = strdup(title);
    tab->content_layer = content_layer;
    tab->enabled = 1;
    tab->user_data = NULL;
    
    // 如果是第一个选项卡，设为活动选项卡
    if (component->tab_count == 0) {
        component->active_tab = 0;
    }
    
    // 如果有内容层，调整其位置和大小
    if (content_layer) {
        content_layer->rect.x = component->layer->rect.x;
        content_layer->rect.y = component->layer->rect.y + component->tab_height;
        content_layer->rect.w = component->layer->rect.w;
        content_layer->rect.h = component->layer->rect.h - component->tab_height;
    }
    
    return component->tab_count++;
}

// 移除选项卡
void tab_component_remove_tab(TabComponent* component, int index) {
    if (!component || index < 0 || index >= component->tab_count) return;
    
    // 释放选项卡标题
    if (component->tabs[index].title) {
        free(component->tabs[index].title);
    }
    
    // 调整活动选项卡
    if (component->active_tab == index) {
        // 如果删除的是活动选项卡，选择下一个选项卡
        if (component->tab_count > 1) {
            component->active_tab = (index < component->tab_count - 1) ? index : index - 1;
        } else {
            component->active_tab = -1;
        }
    } else if (component->active_tab > index) {
        // 如果删除的选项卡在活动选项卡之前，调整活动选项卡索引
        component->active_tab--;
    }
    
    // 移动后面的选项卡
    for (int i = index; i < component->tab_count - 1; i++) {
        component->tabs[i] = component->tabs[i + 1];
    }
    
    // 缩小选项卡数组
    component->tab_count--;
    if (component->tab_count > 0) {
        TabItem* new_tabs = (TabItem*)realloc(component->tabs, component->tab_count * sizeof(TabItem));
        if (new_tabs) {
            component->tabs = new_tabs;
        }
    } else {
        free(component->tabs);
        component->tabs = NULL;
        component->active_tab = -1;
    }
}

// 设置活动选项卡
void tab_component_set_active_tab(TabComponent* component, int index) {
    if (!component || index < 0 || index >= component->tab_count || !component->tabs[index].enabled) return;
    
    int old_tab = component->active_tab;
    
    // 设置新的活动选项卡
    component->active_tab = index;
    
    // 调用回调函数
    if (old_tab != index && component->on_tab_changed) {
        component->on_tab_changed(old_tab, index, component->user_data);
    }
}

// 获取活动选项卡
int tab_component_get_active_tab(TabComponent* component) {
    if (!component) return -1;
    return component->active_tab;
}

// 设置选项卡标题
void tab_component_set_tab_title(TabComponent* component, int index, const char* title) {
    if (!component || !title || index < 0 || index >= component->tab_count) return;
    
    if (component->tabs[index].title) {
        free(component->tabs[index].title);
    }
    component->tabs[index].title = strdup(title);
}

// 获取选项卡标题
const char* tab_component_get_tab_title(TabComponent* component, int index) {
    if (!component || index < 0 || index >= component->tab_count) return NULL;
    return component->tabs[index].title;
}

// 设置选项卡内容
void tab_component_set_tab_content(TabComponent* component, int index, Layer* content_layer) {
    if (!component || index < 0 || index >= component->tab_count) return;
    
    // 设置新的内容层
    component->tabs[index].content_layer = content_layer;
    
    if (content_layer) {
        // 调整内容层位置和大小
        content_layer->rect.x = component->layer->rect.x;
        content_layer->rect.y = component->layer->rect.y + component->tab_height;
        content_layer->rect.w = component->layer->rect.w;
        content_layer->rect.h = component->layer->rect.h - component->tab_height;
    }
}

// 获取选项卡内容
Layer* tab_component_get_tab_content(TabComponent* component, int index) {
    if (!component || index < 0 || index >= component->tab_count) return NULL;
    return component->tabs[index].content_layer;
}

// 启用/禁用选项卡
void tab_component_set_tab_enabled(TabComponent* component, int index, int enabled) {
    if (!component || index < 0 || index >= component->tab_count) return;
    component->tabs[index].enabled = enabled;
}

// 检查选项卡是否启用
int tab_component_is_tab_enabled(TabComponent* component, int index) {
    if (!component || index < 0 || index >= component->tab_count) return 0;
    return component->tabs[index].enabled;
}

// 设置选项卡高度
void tab_component_set_tab_height(TabComponent* component, int height) {
    if (!component || height <= 0) return;
    component->tab_height = height;
    
    // 调整所有内容层的位置和大小
    for (int i = 0; i < component->tab_count; i++) {
        if (component->tabs[i].content_layer) {
            component->tabs[i].content_layer->rect.y = component->layer->rect.y + component->tab_height;
            component->tabs[i].content_layer->rect.h = component->layer->rect.h - component->tab_height;
        }
    }
}

// 设置颜色
void tab_component_set_colors(TabComponent* component, Color tab, Color active_tab, Color text, Color active_text, Color border) {
    if (!component) return;
    component->tab_color = tab;
    component->active_tab_color = active_tab;
    component->text_color = text;
    component->active_text_color = active_text;
    component->border_color = border;
}

// 设置是否可关闭
void tab_component_set_closable(TabComponent* component, int closable) {
    if (!component) return;
    component->closable = closable;
}

// 设置用户数据
void tab_component_set_user_data(TabComponent* component, void* data) {
    if (!component) return;
    component->user_data = data;
}

// 设置选项卡变化回调
void tab_component_set_tab_changed_callback(TabComponent* component, void (*callback)(int, int, void*)) {
    if (!component) return;
    component->on_tab_changed = callback;
}

// 设置选项卡关闭回调
void tab_component_set_tab_close_callback(TabComponent* component, void (*callback)(int, void*)) {
    if (!component) return;
    component->on_tab_close = callback;
}

// 计算选项卡宽度
int tab_calculate_tab_width(TabComponent* component, int index) {
    if (!component || index < 0 || index >= component->tab_count) return 0;
    
    // 计算文本宽度
    int text_width = 100; // 默认宽度
    if (component->layer && component->layer->font && component->layer->font->default_font) {
        Texture* temp_texture = backend_render_texture(component->layer->font->default_font, component->tabs[index].title, component->layer->color);
        if (temp_texture) {
            int temp_height;
            backend_query_texture(temp_texture, NULL, NULL, &text_width, &temp_height);
            backend_render_text_destroy(temp_texture);
            text_width /= scale; // 考虑缩放因子
        }
    }
    
    // 添加边距
    int tab_width = text_width + 20;
    
    // 如果可关闭，添加关闭按钮宽度
    if (component->closable) {
        tab_width += 20;
    }
    
    return tab_width;
}

// 根据鼠标位置获取选项卡索引
int tab_get_tab_from_position(TabComponent* component, int x, int y) {
    if (!component || component->tab_count == 0) return -1;
    
    // 检查是否在选项卡区域内
    if (y < component->layer->rect.y || y >= component->layer->rect.y + component->tab_height) {
        return -1;
    }
    
    // 遍历选项卡
    int current_x = component->layer->rect.x;
    for (int i = 0; i < component->tab_count; i++) {
        int tab_width = tab_calculate_tab_width(component, i);
        
        if (x >= current_x && x < current_x + tab_width) {
            return i;
        }
        
        current_x += tab_width;
    }
    
    return -1;
}

// 检查是否点击在关闭按钮上
int tab_is_close_button_clicked(TabComponent* component, int tab_index, int x, int y) {
    if (!component || !component->closable || tab_index < 0 || tab_index >= component->tab_count) return 0;
    
    // 计算选项卡位置
    int current_x = component->layer->rect.x;
    for (int i = 0; i < tab_index; i++) {
        current_x += tab_calculate_tab_width(component, i);
    }
    
    int tab_width = tab_calculate_tab_width(component, tab_index);
    int close_x = current_x + tab_width - 18;
    int close_y = component->layer->rect.y + (component->tab_height - 14) / 2;
    
    return (x >= close_x && x < close_x + 14 && y >= close_y && y < close_y + 14);
}

// 处理鼠标事件
void tab_component_handle_mouse_event(Layer* layer, MouseEvent* event) {
    if (!layer || !event || !layer->component) return;
    
    TabComponent* component = (TabComponent*)layer->component;
    
    if (event->state == SDL_PRESSED && event->button == SDL_BUTTON_LEFT) {
        // 检查点击了哪个标签
        int tab_index = tab_get_tab_from_position(component, event->x, event->y);
        if (tab_index >= 0) {
            // 检查是否点击了关闭按钮
            if (component->closable && tab_is_close_button_clicked(component, tab_index, event->x, event->y)) {
                // 调用关闭回调
                if (component->on_tab_close) {
                    component->on_tab_close(tab_index, component->user_data);
                }
            } else {
                tab_component_set_active_tab(component, tab_index);
            }
        }
    }
}

// 处理键盘事件
void tab_component_handle_key_event(Layer* layer, KeyEvent* event) {
    if (!layer || !event || !layer->component) return;
    
    TabComponent* component = (TabComponent*)layer->component;
    
    if (event->type == KEY_EVENT_DOWN) {
        switch (event->data.key.key_code) {
            case SDLK_LEFT:
                if (component->active_tab > 0) {
                    tab_component_set_active_tab(component, component->active_tab - 1);
                }
                break;
            case SDLK_RIGHT:
                if (component->active_tab < component->tab_count - 1) {
                    tab_component_set_active_tab(component, component->active_tab + 1);
                }
                break;
        }
    }
}

// 渲染选项卡
void tab_component_render(Layer* layer) {
    if (!layer || !layer->component) return;
    
    TabComponent* component = (TabComponent*)layer->component;
    
    // 绘制选项卡背景
    Rect bg_rect = {layer->rect.x, layer->rect.y, layer->rect.w, component->tab_height};
    backend_render_rounded_rect(&bg_rect, component->tab_color, 5);
    
    // 计算文本高度
    int text_height = 20; // 默认高度
    if (layer->font && layer->font->default_font) {
        Texture* temp_texture = backend_render_texture(layer->font->default_font, "A", layer->color);
        if (temp_texture) {
            int temp_width;
            backend_query_texture(temp_texture, NULL, NULL, &temp_width, &text_height);
            backend_render_text_destroy(temp_texture);
        }
    }
    
    // 绘制选项卡
    int current_x = layer->rect.x;
    for (int i = 0; i < component->tab_count; i++) {
        if (!component->tabs[i].title) continue;
        
        int tab_width = tab_calculate_tab_width(component, i);
        
        // 选择颜色
        Color bg_color = (i == component->active_tab) ? component->active_tab_color : component->tab_color;
        Color text_color = (i == component->tabs[i].enabled) ? 
                          ((i == component->active_tab) ? component->active_text_color : component->text_color) : 
                          (Color){153, 153, 153, 255}; // 使用Color结构体而不是color_from_hex
        
        // 绘制选项卡背景
        Rect tab_rect = {current_x, layer->rect.y, tab_width, component->tab_height};
        backend_render_rounded_rect(&tab_rect, bg_color, 5);
        
        // 绘制选项卡文本
        int text_x = current_x + 10;
        int text_y = layer->rect.y + (component->tab_height - text_height) / 2;
        
        if (layer->font && layer->font->default_font) {
            Texture* text_texture = backend_render_texture(layer->font->default_font, component->tabs[i].title, text_color);
            if (text_texture) {
                Rect text_rect = {text_x, text_y, tab_width - 20, text_height};
                backend_render_text_copy(text_texture, NULL, &text_rect);
                backend_render_text_destroy(text_texture);
            }
        }
        
        // 绘制关闭按钮
        if (component->closable) {
            int close_x = current_x + tab_width - 18;
            int close_y = layer->rect.y + (component->tab_height - 14) / 2;
            Rect close_rect = {close_x, close_y, 14, 14};
            backend_render_rounded_rect(&close_rect, (Color){255, 0, 0, 255}, 7);
            
            if (layer->font && layer->font->default_font) {
                Texture* close_texture = backend_render_texture(layer->font->default_font, "×", (Color){255, 255, 255, 255});
                if (close_texture) {
                    Rect close_text_rect = {close_x + 3, close_y + 1, 8, 12};
                    backend_render_text_copy(close_texture, NULL, &close_text_rect);
                    backend_render_text_destroy(close_texture);
                }
            }
        }
        
        current_x += tab_width;
    }
    
    // 绘制边框
    Rect border_rect = {layer->rect.x, layer->rect.y, layer->rect.w, layer->rect.h};
    backend_render_rounded_rect_with_border(&border_rect, (Color){255, 255, 255, 255}, 5, 1, component->border_color);
}