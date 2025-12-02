#include "listbox_component.h"
#include "../backend.h"
#include "../event.h"
#include "../util.h"
#include <stdlib.h>
#include <string.h>

// 创建列表框组件
ListBoxComponent* listbox_component_create(Layer* layer) {
    ListBoxComponent* component = (ListBoxComponent*)malloc(sizeof(ListBoxComponent));
    if (!component) return NULL;
    
    component->layer = layer;
    component->items = NULL;
    component->item_count = 0;
    component->selected_index = -1;
    component->visible_count = 5;  // 默认显示5个项目
    component->scroll_position = 0;
    component->bg_color = (Color){255, 255, 255, 255};
    component->text_color = (Color){0, 0, 0, 255};
    component->selected_bg_color = (Color){0, 120, 215, 255};
    component->selected_text_color = (Color){255, 255, 255, 255};
    component->multi_select = 0;
    component->user_data = NULL;
    component->on_selection_changed = NULL;
    component->on_item_double_click = NULL;
    
    // 设置组件
    layer->component = component;
    
    return component;
}

// 从 JSON 创建列表框组件
ListBoxComponent* listbox_component_create_from_json(Layer* layer, cJSON* json_obj) {
    if (!layer || !json_obj) return NULL;
    
    ListBoxComponent* listBoxComponent = listbox_component_create(layer);
    // 解析列表框特定属性
    if (cJSON_HasObjectItem(json_obj, "visibleCount")) {
        // listbox_component_set_visible_count(
        //     listBoxComponent, cJSON_GetObjectItem(json_obj, "visibleCount")->valueint);
    }
    if (cJSON_HasObjectItem(json_obj, "multiSelect")) {
        listbox_component_set_multi_select(
            listBoxComponent, cJSON_GetObjectItem(json_obj, "multiSelect")->valueint);
    }

    // 解析列表框特定属性
    cJSON* listboxConfig = cJSON_GetObjectItem(json_obj, "listboxConfig");
    if (listboxConfig) {
      ListBoxComponent* listboxComponent = (ListBoxComponent*)layer->component;

      if (cJSON_HasObjectItem(listboxConfig, "multiSelect")) {
        listbox_component_set_multi_select(
            listboxComponent,
            cJSON_IsTrue(cJSON_GetObjectItem(listboxConfig, "multiSelect")));
      }

      // 解析列表项数据
      cJSON* items = cJSON_GetObjectItem(listboxConfig, "items");
      if (items && cJSON_IsArray(items)) {
        for (int i = 0; i < cJSON_GetArraySize(items); i++) {
          cJSON* item = cJSON_GetArrayItem(items, i);
          if (cJSON_IsString(item)) {
            listbox_component_add_item(listboxComponent, item->valuestring,
                                       NULL);
          } else if (cJSON_IsObject(item)) {
            const char* text = "";
            if (cJSON_HasObjectItem(item, "text")) {
              text = cJSON_GetObjectItem(item, "text")->valuestring;
            }
            listbox_component_add_item(listboxComponent, text, NULL);
          }
        }
      }

      // 解析列表框颜色
      cJSON* colors = cJSON_GetObjectItem(listboxConfig, "colors");
      if (colors) {
        Color bgColor = {255, 255, 255, 255};
        Color textColor = {0, 0, 0, 255};
        Color selectedBgColor = {50, 150, 250, 255};
        Color selectedTextColor = {255, 255, 255, 255};

        if (cJSON_HasObjectItem(colors, "bgColor")) {
          parse_color(cJSON_GetObjectItem(colors, "bgColor")->valuestring,
                      &bgColor);
        }
        if (cJSON_HasObjectItem(colors, "textColor")) {
          parse_color(cJSON_GetObjectItem(colors, "textColor")->valuestring,
                      &textColor);
        }
        if (cJSON_HasObjectItem(colors, "selectedBgColor")) {
          parse_color(
              cJSON_GetObjectItem(colors, "selectedBgColor")->valuestring,
              &selectedBgColor);
        }
        if (cJSON_HasObjectItem(colors, "selectedTextColor")) {
          parse_color(
              cJSON_GetObjectItem(colors, "selectedTextColor")->valuestring,
              &selectedTextColor);
        }

        listbox_component_set_colors(listboxComponent, bgColor, textColor,
                                     selectedBgColor, selectedTextColor);
      }
    }
    
    return listBoxComponent;
}

// 销毁列表框组件
void listbox_component_destroy(ListBoxComponent* component) {
    if (!component) return;
    
    // 释放所有项目
    listbox_component_clear_items(component);
    
    free(component);
}

// 添加项目
void listbox_component_add_item(ListBoxComponent* component, const char* text, void* user_data) {
    if (!component || !text) return;
    
    // 扩展项目数组
    component->items = (ListBoxItem*)realloc(component->items, (component->item_count + 1) * sizeof(ListBoxItem));
    if (!component->items) return;
    
    // 添加新项目
    ListBoxItem* item = &component->items[component->item_count];
    item->text = strdup(text);
    item->user_data = user_data;
    item->selected = 0;
    
    component->item_count++;
}

// 移除项目
void listbox_component_remove_item(ListBoxComponent* component, int index) {
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
void listbox_component_clear_items(ListBoxComponent* component) {
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
    component->scroll_position = 0;
}

// 设置选中项
void listbox_component_set_selected(ListBoxComponent* component, int index) {
    if (!component || index < -1 || index >= component->item_count) return;
    
    int old_index = component->selected_index;
    component->selected_index = index;
    
    // 如果不是多选模式，清除其他选中状态
    if (!component->multi_select) {
        for (int i = 0; i < component->item_count; i++) {
            component->items[i].selected = 0;
        }
        
        if (index >= 0) {
            component->items[index].selected = 1;
        }
    } else if (index >= 0) {
        // 多选模式，切换选中状态
        component->items[index].selected = !component->items[index].selected;
    }
    
    // 调用回调函数
    if (old_index != index && component->on_selection_changed) {
        component->on_selection_changed(index, component->user_data);
    }
}

// 获取选中项索引
int listbox_component_get_selected(ListBoxComponent* component) {
    if (!component) return -1;
    return component->selected_index;
}

// 设置是否允许多选
void listbox_component_set_multi_select(ListBoxComponent* component, int multi_select) {
    if (!component) return;
    component->multi_select = multi_select;
    
    // 如果从多选切换到单选，只保留第一个选中项
    if (!multi_select) {
        int first_selected = -1;
        for (int i = 0; i < component->item_count; i++) {
            if (component->items[i].selected) {
                if (first_selected == -1) {
                    first_selected = i;
                } else {
                    component->items[i].selected = 0;
                }
            }
        }
        component->selected_index = first_selected;
    }
}

// 设置颜色
void listbox_component_set_colors(ListBoxComponent* component, Color bg, Color text, Color selected_bg, Color selected_text) {
    if (!component) return;
    component->bg_color = bg;
    component->text_color = text;
    component->selected_bg_color = selected_bg;
    component->selected_text_color = selected_text;
}

// 设置用户数据
void listbox_component_set_user_data(ListBoxComponent* component, void* data) {
    if (!component) return;
    component->user_data = data;
}

// 设置选择变化回调
void listbox_component_set_selection_changed_callback(ListBoxComponent* component, void (*callback)(int, void*)) {
    if (!component) return;
    component->on_selection_changed = callback;
}

// 设置项目双击回调
void listbox_component_set_item_double_click_callback(ListBoxComponent* component, void (*callback)(int, void*)) {
    if (!component) return;
    component->on_item_double_click = callback;
}

// 处理鼠标事件
void listbox_component_handle_mouse_event(Layer* layer, MouseEvent* event) {
    if (!layer || !event || !layer->component) return;
    
    ListBoxComponent* component = (ListBoxComponent*)layer->component;
    
    if (event->state == SDL_PRESSED && event->button == SDL_BUTTON_LEFT) {
        // 计算点击的项目索引
        int item_height = 20;  // 每个项目的高度
        int clicked_index = (event->y - layer->rect.y) / item_height + component->scroll_position;
        
        if (clicked_index >= 0 && clicked_index < component->item_count) {
            listbox_component_set_selected(component, clicked_index);
        }
    } else if (event->state == SDL_PRESSED  && event->button == SDL_BUTTON_LEFT) {
        // 计算点击的项目索引
        int item_height = 20;  // 每个项目的高度
        int clicked_index = (event->y - layer->rect.y) / item_height + component->scroll_position;
        
        if (clicked_index >= 0 && clicked_index < component->item_count) {
            listbox_component_set_selected(component, clicked_index);
            
            // 调用双击回调
            if (component->on_item_double_click) {
                component->on_item_double_click(clicked_index, component->user_data);
            }
        }
    }
}

// 处理键盘事件
void listbox_component_handle_key_event(Layer* layer, KeyEvent* event) {
    if (!layer || !event || !layer->component) return;
    
    ListBoxComponent* component = (ListBoxComponent*)layer->component;
    
    if (event->type == KEY_EVENT_DOWN) {
        switch (event->data.key.key_code) {
            case SDLK_UP:
                if (component->selected_index > 0) {
                    listbox_component_set_selected(component, component->selected_index - 1);
                    
                    // 自动滚动以确保选中项可见
                    if (component->selected_index < component->scroll_position) {
                        component->scroll_position = component->selected_index;
                    }
                }
                break;
                
            case SDLK_DOWN:
                if (component->selected_index < component->item_count - 1) {
                    listbox_component_set_selected(component, component->selected_index + 1);
                    
                    // 自动滚动以确保选中项可见
                    if (component->selected_index >= component->scroll_position + component->visible_count) {
                        component->scroll_position = component->selected_index - component->visible_count + 1;
                    }
                }
                break;
                
            case SDLK_HOME:
                if (component->item_count > 0) {
                    listbox_component_set_selected(component, 0);
                    component->scroll_position = 0;
                }
                break;
                
            case SDLK_END:
                if (component->item_count > 0) {
                    listbox_component_set_selected(component, component->item_count - 1);
                    component->scroll_position = component->item_count - component->visible_count;
                    if (component->scroll_position < 0) {
                        component->scroll_position = 0;
                    }
                }
                break;
        }
    }
}

// 渲染列表框
void listbox_component_render(Layer* layer) {
    if (!layer || !layer->component) return;
    
    ListBoxComponent* component = (ListBoxComponent*)layer->component;
    
    // 绘制背景
    Rect rect = {layer->rect.x, layer->rect.y, layer->rect.w, layer->rect.h};
    backend_render_rounded_rect(&rect, component->bg_color, layer->radius);
    
    // 绘制边框
    if (layer->radius > 0) {
        backend_render_rounded_rect_with_border(&rect, component->bg_color, layer->radius, 1, layer->color);
    }
    
    // 计算可见项目数量
    int item_height = 20;  // 每个项目的高度
    int visible_count = layer->rect.h / item_height;
    if (visible_count > component->item_count - component->scroll_position) {
        visible_count = component->item_count - component->scroll_position;
    }
    
    // 绘制可见项目
    for (int i = 0; i < visible_count; i++) {
        int item_index = i + component->scroll_position;
        if (item_index >= component->item_count) break;
        
        ListBoxItem* item = &component->items[item_index];
        int item_y = layer->rect.y + i * item_height;
        
        // 绘制选中项背景
        if (item->selected) {
            Rect item_rect = {layer->rect.x + 2, item_y, layer->rect.w - 4, item_height};
            backend_render_rect(&item_rect, component->selected_bg_color);
        }
        
        // 绘制项目文本
        Color text_color = item->selected ? component->selected_text_color : component->text_color;
        // 创建文本纹理
        Texture* text_texture = backend_render_texture(layer->font->default_font, item->text, text_color);
        if (text_texture) {
            int text_width, text_height;
            backend_query_texture(text_texture, NULL, NULL, &text_width, &text_height);
            
            Rect text_rect = {layer->rect.x + 5, item_y + (item_height - text_height) / 2, text_width, text_height};
            backend_render_text_copy(text_texture, NULL, &text_rect);
            backend_render_text_destroy(text_texture);
        }
    }
    
    // 绘制滚动条（如果需要）
    if (component->item_count > visible_count) {
        int scrollbar_width = 10;
        int scrollbar_height = (layer->rect.h * visible_count) / component->item_count;
        int scrollbar_y = layer->rect.y + (layer->rect.h * component->scroll_position) / component->item_count;
        int scrollbar_x = layer->rect.x + layer->rect.w - scrollbar_width - 2;
        
        // 绘制滚动条背景
        Color scrollbar_bg_color = {240, 240, 240, 255};
        Rect scrollbar_bg_rect = {scrollbar_x, layer->rect.y, scrollbar_width, layer->rect.h};
        backend_render_rect(&scrollbar_bg_rect, scrollbar_bg_color);
        
        // 绘制滚动条
        Color scrollbar_color = {192, 192, 192, 255};
        Rect scrollbar_rect = {scrollbar_x, scrollbar_y, scrollbar_width, scrollbar_height};
        backend_render_rounded_rect(&scrollbar_rect, scrollbar_color, 5);
    }
}