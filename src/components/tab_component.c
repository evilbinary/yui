#include "tab_component.h"

#include <stdlib.h>
#include <string.h>

#include "../backend.h"
#include "../event.h"
#include "../layout.h"
#include "../render.h"
#include "../util.h"

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

  // 设置组件类型和渲染函数
  layer->component = component;
  layer->render = tab_component_render;
  layer->handle_mouse_event = tab_component_handle_mouse_event;
  layer->handle_key_event = tab_component_handle_key_event;

  return component;
}

TabComponent* tab_component_create_from_json(Layer* layer, cJSON* json_obj) {
  if (!layer || !json_obj) return NULL;

  TabComponent* tabComponent = tab_component_create(layer);
  // 解析选项卡特定属性
  if (cJSON_HasObjectItem(json_obj, "tabHeight")) {
    tab_component_set_tab_height(
        tabComponent, cJSON_GetObjectItem(json_obj, "tabHeight")->valueint);
  }

  if (cJSON_HasObjectItem(json_obj, "activeTab")) {
    tab_component_set_active_tab(
        tabComponent, cJSON_GetObjectItem(json_obj, "activeTab")->valueint);
  }

  if (cJSON_HasObjectItem(json_obj, "closable")) {
    tab_component_set_closable(
        tabComponent, cJSON_IsTrue(cJSON_GetObjectItem(json_obj, "closable")));
  }

  // 解析选项卡颜色
  cJSON* style = cJSON_GetObjectItem(json_obj, "style");
  if (style) {
    Color tabColor = {240, 240, 240, 255};
    Color activeTabColor = {255, 255, 255, 255};
    Color textColor = {51, 51, 51, 255};
    Color activeTextColor = {0, 0, 0, 255};
    Color borderColor = {204, 204, 204, 255};

    if (cJSON_HasObjectItem(style, "tabColor")) {
      parse_color(cJSON_GetObjectItem(style, "tabColor")->valuestring,
                  &tabColor);
    }
    if (cJSON_HasObjectItem(style, "activeTabColor")) {
      parse_color(cJSON_GetObjectItem(style, "activeTabColor")->valuestring,
                  &activeTabColor);
    }
    if (cJSON_HasObjectItem(style, "textColor")) {
      parse_color(cJSON_GetObjectItem(style, "textColor")->valuestring,
                  &textColor);
    }
    if (cJSON_HasObjectItem(style, "activeTextColor")) {
      parse_color(cJSON_GetObjectItem(style, "activeTextColor")->valuestring,
                  &activeTextColor);
    }
    if (cJSON_HasObjectItem(style, "borderColor")) {
      parse_color(cJSON_GetObjectItem(style, "borderColor")->valuestring,
                  &borderColor);
    }

    tab_component_set_colors(tabComponent, tabColor, activeTabColor, textColor,
                             activeTextColor, borderColor);
  }
  // 解析选项卡数组
  cJSON* tabs = cJSON_GetObjectItem(json_obj, "tabs");
  if(tabs==NULL){
    tabs = cJSON_GetObjectItem(json_obj, "children");
  }
  if (tabs && cJSON_IsArray(tabs)) {
    int tab_count = cJSON_GetArraySize(tabs);

    for (int i = 0; i < tab_count; i++) {
      cJSON* tab_json = cJSON_GetArrayItem(tabs, i);
      if (!tab_json) continue;

      cJSON* title = cJSON_GetObjectItem(tab_json, "title");
      if (!title || !title->valuestring) continue;

      // 创建内容层
      Layer* content_layer = parse_layer(tab_json, layer);
      cJSON* content = tab_json;
      if (content) {
        if (content_layer) {
          // 设置内容层的位置和大小
          content_layer->rect.x = layer->rect.x;
          content_layer->rect.y = layer->rect.y + tabComponent->tab_height;
          content_layer->rect.w = layer->rect.w;
          content_layer->rect.h = layer->rect.h - tabComponent->tab_height;
          content_layer->parent = layer;
          // 注意：不添加到父层的children中，由tab组件自己管理渲染
        }
      }

      // 添加选项卡
      int tab_index = tab_component_add_tab(tabComponent, title->valuestring,
                                            content_layer);

      // 设置选项卡为不可见（只有活动选项卡的内容可见）
      if (content_layer && tab_index != tabComponent->active_tab) {
        SET_STATE(content_layer, LAYER_STATE_DISABLED);
      }
    }
  }

  // 设置渲染和事件处理函数
  layer->render = tab_component_render;
  layer->handle_mouse_event = tab_component_handle_mouse_event;
  layer->handle_key_event = tab_component_handle_key_event;

  return tabComponent;
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
int tab_component_add_tab(TabComponent* component, const char* title,
                          Layer* content_layer) {
  if (!component || !title) return -1;

  // 扩展选项卡数组
  TabItem* new_tabs = (TabItem*)realloc(
      component->tabs, (component->tab_count + 1) * sizeof(TabItem));
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
      component->active_tab =
          (index < component->tab_count - 1) ? index : index - 1;
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
    TabItem* new_tabs = (TabItem*)realloc(
        component->tabs, component->tab_count * sizeof(TabItem));
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
  printf("DEBUG: tab_component_set_active_tab called with index=%d (current=%d)\n", index, component->active_tab);
  
  if (!component || index < 0 || index >= component->tab_count ||
      !component->tabs[index].enabled) {
    printf("DEBUG: tab_component_set_active_tab rejected - invalid parameters\n");
    return;
  }

  int old_tab = component->active_tab;

  // 设置新的活动选项卡
  component->active_tab = index;
  printf("DEBUG: Active tab changed from %d to %d\n", old_tab, index);

  // 更新内容层的可见性
  for (int i = 0; i < component->tab_count; i++) {
    if (component->tabs[i].content_layer) {
      component->tabs[i].content_layer->visible = (i == index)?VISIBLE:IN_VISIBLE;
      printf("DEBUG: Tab %d content_layer visible=%d\n", i, component->tabs[i].content_layer->visible);
    }
  }

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
void tab_component_set_tab_title(TabComponent* component, int index,
                                 const char* title) {
  if (!component || !title || index < 0 || index >= component->tab_count)
    return;

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
void tab_component_set_tab_content(TabComponent* component, int index,
                                   Layer* content_layer) {
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
void tab_component_set_tab_enabled(TabComponent* component, int index,
                                   int enabled) {
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
      component->tabs[i].content_layer->rect.y =
          component->layer->rect.y + component->tab_height;
      component->tabs[i].content_layer->rect.h =
          component->layer->rect.h - component->tab_height;
    }
  }
}

// 设置颜色
void tab_component_set_colors(TabComponent* component, Color tab,
                              Color active_tab, Color text, Color active_text,
                              Color border) {
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
void tab_component_set_tab_changed_callback(TabComponent* component,
                                            void (*callback)(int, int, void*)) {
  if (!component) return;
  component->on_tab_changed = callback;
}

// 设置选项卡关闭回调
void tab_component_set_tab_close_callback(TabComponent* component,
                                          void (*callback)(int, void*)) {
  if (!component) return;
  component->on_tab_close = callback;
}

// 计算选项卡宽度
int tab_calculate_tab_width(TabComponent* component, int index) {
  if (!component || index < 0 || index >= component->tab_count) return 0;

  // 计算文本宽度
  int text_width = 100;  // 默认宽度
  if (component->layer && component->layer->font &&
      component->layer->font->default_font) {
    Texture* temp_texture = backend_render_texture(
        component->layer->font->default_font, component->tabs[index].title,
        component->text_color);
    if (temp_texture) {
      int temp_height;
      backend_query_texture(temp_texture, NULL, NULL, &text_width,
                            &temp_height);
      backend_render_text_destroy(temp_texture);
      // text_width /= scale;  // 暂时不考虑缩放因子
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
  if (y < component->layer->rect.y ||
      y >= component->layer->rect.y + component->tab_height) {
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
int tab_is_close_button_clicked(TabComponent* component, int tab_index, int x,
                                int y) {
  if (!component || !component->closable || tab_index < 0 ||
      tab_index >= component->tab_count)
    return 0;

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
      if (component->closable &&
          tab_is_close_button_clicked(component, tab_index, event->x,
                                      event->y)) {
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
  if (component->tab_count <= 0) return;

  // 绘制内容区域背景
  Rect content_rect = {
    layer->rect.x, 
    layer->rect.y + component->tab_height,
    layer->rect.w, 
    layer->rect.h - component->tab_height
  };
  backend_render_fill_rect_color(&content_rect, 255, 255, 255, 255);

  // 计算每个tab的宽度（使用与点击检测相同的计算方式）
  int current_x = layer->rect.x;

  // 绘制每个tab
  for (int i = 0; i < component->tab_count; i++) {
    int tab_width = tab_calculate_tab_width(component, i);
    int tab_x = current_x;
    Rect tab_rect = {tab_x, layer->rect.y, tab_width, component->tab_height};

    // 选择颜色：active tab用蓝色，其他用灰色
    if (i == component->active_tab) {
      backend_render_fill_rect_color(&tab_rect, 33, 150, 243, 255); // 蓝色
    } else {
      backend_render_fill_rect_color(&tab_rect, 240, 240, 240, 255); // 灰色
    }

    // 绘制tab文字
    if (layer->font && layer->font->default_font && component->tabs[i].title) {
      Color text_color = (i == component->active_tab) ? 
        (Color){255, 255, 255, 255} : (Color){51, 51, 51, 255}; // active用白色，其他用黑色
      
      Texture* text_texture = backend_render_texture(
          layer->font->default_font, component->tabs[i].title, text_color);
      
      if (text_texture) {
        // 获取文字纹理的实际尺寸
        int text_width, text_height;
        backend_query_texture(text_texture, NULL, NULL, &text_width, &text_height);
        if (text_width > tab_width - 10) text_width = tab_width - 10;
        
        Rect text_rect = {
          tab_x + (tab_width - text_width / scale) / 2,
          layer->rect.y + (component->tab_height - text_height / scale) / 2,  // 使用实际高度居中
          text_width / scale,
          text_height / scale
        };
        backend_render_text_copy(text_texture, NULL, &text_rect);
        backend_render_text_destroy(text_texture);
      }
    }
    
    current_x += tab_width;  // 移动到下一个tab位置
  }

  // 渲染活动tab的内容
  if (component->active_tab >= 0 && component->active_tab < component->tab_count) {
    Layer* active_content = component->tabs[component->active_tab].content_layer;
    printf("DEBUG: Active tab=%d, content_layer=%p\n", component->active_tab, active_content);
    
    if (active_content) {
      printf("DEBUG: Rendering content for tab %d, layer %s rect: x=%d, y=%d, w=%d, h=%d\n", 
             component->active_tab, active_content->id, active_content->rect.x, active_content->rect.y, 
             active_content->rect.w, active_content->rect.h);
      printf("DEBUG: Content layer has %d children\n", active_content->child_count);
      printf("DEBUG: Content layer visible=%d\n", active_content->visible);
      
      // 打印子元素信息
      for (int i = 0; i < active_content->child_count; i++) {
        Layer* child = active_content->children[i];
        if (child) {
          printf("DEBUG: Child %d: %s, type=%d, rect: x=%d, y=%d, w=%d, h=%d, visible=%d\n", 
                 i, child->id, child->type, child->rect.x, child->rect.y, child->rect.w, child->rect.h, child->visible);
          if (child->type == LABEL && child->text) {
            printf("DEBUG: Child %d text: %s\n", i, child->text);
          }
        }
      }
      
      // 设置内容区域的裁剪
      Rect prev_clip;
      backend_render_get_clip_rect(&prev_clip);
      
      Rect content_clip = {
        layer->rect.x, 
        layer->rect.y + component->tab_height,
        layer->rect.w, 
        layer->rect.h - component->tab_height
      };
      printf("DEBUG: Content clip rect: x=%d, y=%d, w=%d, h=%d\n", 
             content_clip.x, content_clip.y, content_clip.w, content_clip.h);
      backend_render_set_clip_rect(&content_clip);
      
      // 保存内容层原始位置
      int original_x = active_content->rect.x;
      int original_y = active_content->rect.y;
      
      // 设置内容层到正确的位置（tab组件内的内容区域）
      active_content->rect.x = layer->rect.x;
      active_content->rect.y = layer->rect.y + component->tab_height;
      active_content->rect.w = layer->rect.w;
      active_content->rect.h = layer->rect.h - component->tab_height;
      
      printf("DEBUG: After position adjustment - content rect: x=%d, y=%d, w=%d, h=%d\n", 
             active_content->rect.x, active_content->rect.y, active_content->rect.w, active_content->rect.h);
      
      // 确保内容层及其子元素都可见
      active_content->visible = VISIBLE;
      for (int i = 0; i < active_content->child_count; i++) {
        if (active_content->children[i]) {
          active_content->children[i]->visible = VISIBLE;
          printf("DEBUG: Setting child %d visible to VISIBLE\n", i);
        }
      }
      
      // 手动触发布局计算
      printf("DEBUG: Checking layout manager: %p\n", (void*)active_content->layout_manager);
      if (active_content->layout_manager) {
        printf("DEBUG: Triggering layout calculation for content layer\n");
        layout_layer(active_content);
      } else {
        printf("DEBUG: No layout manager found for content layer\n");
        // 手动设置子元素位置作为备选方案
        for (int i = 0; i < active_content->child_count; i++) {
          Layer* child = active_content->children[i];
          if (child) {
            // 简单的垂直布局
            child->rect.x = active_content->rect.x + 20;
            child->rect.y = active_content->rect.y + 20 + i * 30;
            child->rect.w = active_content->rect.w - 40;
            child->rect.h = 25;
            printf("DEBUG: Manually positioned child %d: x=%d, y=%d, w=%d, h=%d\n", 
                   i, child->rect.x, child->rect.y, child->rect.w, child->rect.h);
          }
        }
      }
      
      // 递归渲染内容层及其所有子元素
      render_layer(active_content);
      
      // 恢复内容层的原始位置
      active_content->rect.x = original_x;
      active_content->rect.y = original_y;
      
      // 恢复裁剪区域
      backend_render_set_clip_rect(&prev_clip);
    } else {
      printf("DEBUG: No content layer for active tab %d\n", component->active_tab);
    }
  }
}