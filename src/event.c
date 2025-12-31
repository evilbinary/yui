#include "event.h"
#include "util.h"
#include "backend.h"
#include "popup_manager.h"
#include <stdio.h>
#include <string.h>

// 全局事件处理数组
int global_event_handers_size=MAX_EVENT;
EventEntry global_event_handlers[MAX_EVENT];
// 当前已注册事件数量
int event_count = 0;

// 注册事件处理函数
int register_event_handler(const char* name, EventHandler handler) {
    if (event_count >= MAX_EVENT) {
        fprintf(stderr, "错误: 事件处理数组已满\n");
        return -1;
    }
    
    // 检查是否已存在同名事件
    for (int i = 0; i < event_count; i++) {
        if (strcmp(global_event_handlers[i].name, name) == 0) {
            fprintf(stderr, "警告: 事件 '%s' 已存在，将被覆盖\n", name);
            global_event_handlers[i].handler = handler;
            return 0;
        }
    }
    
    // 添加新事件
    strncpy(global_event_handlers[event_count].name, name, sizeof(global_event_handlers[event_count].name) - 1);
    global_event_handlers[event_count].name[sizeof(global_event_handlers[event_count].name) - 1] = '\0';
    global_event_handlers[event_count].handler = handler;
    event_count++;
    
    return 0;
}
// 打印所有已注册事件
void print_registered_events() {
    printf("已注册事件:\n");
    for (int i = 0; i < event_count; i++) {
        printf("  %d: %s\n", i, global_event_handlers[i].name);
    }
    printf("\n");
}

// 根据名称查找函数
EventHandler find_event_by_name(const char* name) {
    for (int i = 0; i < event_count; i++) {
        if (strcmp(global_event_handlers[i].name, name) == 0) {
            return global_event_handlers[i].handler;
        }
    }
    return NULL; // 未找到
}



// ====================== 事件处理器 ======================
// 处理垂直滚动事件
void handle_scroll_event(Layer* layer,int mouse_x,int mouse_y,int scroll_deltax,int scroll_deltay) {
    // 优先处理popup层的滚动事件
    if (popup_manager_handle_scroll_event(scroll_deltay)) {
        return;
    }
    
    // 检查鼠标是否在图层内
    if (!is_point_in_rect(mouse_x, mouse_y, layer->rect)) {
        return; // 鼠标不在图层内，不处理滚动事件
    }
    
    handler_virtical_scroll_event(layer, scroll_deltay);
    handle_horizontal_scroll_event(layer, scroll_deltay); // 水平滚动

    // 递归处理子图层的键盘事件（但只有焦点图层会实际处理事件）
    for (int i = 0; i < layer->child_count; i++) {
        if (layer->children[i]) {
            handle_scroll_event(layer->children[i],mouse_x,mouse_y, scroll_deltax,scroll_deltay);
        }
    }
    
    // 处理sub图层的键盘事件
    if (layer->sub) {
        handle_scroll_event(layer->sub,mouse_x,mouse_y, scroll_deltax,scroll_deltay);
    }
}

// 处理垂直滚动事件
void handler_virtical_scroll_event(Layer* layer, int scroll_delta) {
    // 只有在垂直滚动或双向滚动模式下才处理垂直滚动
    if ((layer->scrollable == 1 || layer->scrollable == 3) && layer->scrollbar_v) {
        layer->scroll_offset += scroll_delta * 20; // 每次滚动20像素
        
        // 限制滚动范围
        int content_height = layer->content_height;    
        int visible_height = layer->rect.h;
        if (layer->layout_manager) {
            visible_height -= layer->layout_manager->padding[0] + layer->layout_manager->padding[2];
        }
        
        // 不允许向上滚动超过顶部
        if (layer->scroll_offset < 0) {
            layer->scroll_offset = 0;
        }
        // 不允许向下滚动超过底部
        else if (content_height > visible_height && layer->scroll_offset > content_height - visible_height) {
            layer->scroll_offset = content_height - visible_height;
        }
        // 如果内容高度小于可见高度，则不能滚动
        else if (content_height <= visible_height) {
            layer->scroll_offset = 0;
        }
        
        // 触发滚动事件回调
        if (layer->event && layer->event->scroll) {
            layer->event->scroll(layer);
        }
        // 重新布局子元素
        layout_layer(layer);
    }
}

// 处理水平滚动事件
void handle_horizontal_scroll_event(Layer* layer, int scroll_delta) {
    // 只有在水平滚动或双向滚动模式下才处理水平滚动
    if ((layer->scrollable == 2 || layer->scrollable == 3) && layer->scrollbar_h) {
        layer->scroll_offset_x += scroll_delta * 20; // 每次滚动20像素
        
        // 限制滚动范围
        int content_width = layer->content_width;

        int visible_width = layer->rect.w;
        if (layer->layout_manager) {
            visible_width -= layer->layout_manager->padding[1] + layer->layout_manager->padding[3];
        }
        
        // 不允许向左滚动超过左侧
        if (layer->scroll_offset_x < 0) {
            layer->scroll_offset_x = 0;
        }
        // 不允许向右滚动超过右侧
        else if (content_width > visible_width && layer->scroll_offset_x > content_width - visible_width) {
            layer->scroll_offset_x = content_width - visible_width;
        }
        // 如果内容宽度小于可见宽度，则不能滚动
        else if (content_width <= visible_width) {
            layer->scroll_offset_x = 0;
        }
        
        // 触发滚动事件回调
        if (layer->event && layer->event->scroll) {
            layer->event->scroll(layer);
        }
        // 重新布局子元素
        layout_layer(layer);
    }
}

// 处理键盘事件
void handle_key_event(Layer* layer, KeyEvent* event) {
    if (!layer || !event) {
        return;
    }

    // 优先处理popup层的键盘事件
    if (popup_manager_handle_key_event(event)) {
        return;
    }

    // 只有当图层是当前焦点图层或者没有焦点图层时，才处理键盘事件
    // 这确保了键盘事件只传递给当前拥有焦点的图层
    if (focused_layer == layer && layer->handle_key_event) {
        layer->handle_key_event(layer, event);
    }
    
    // 递归处理子图层的键盘事件（但只有焦点图层会实际处理事件）
    for (int i = 0; i < layer->child_count; i++) {
        if (layer->children[i]) {
            handle_key_event(layer->children[i], event);
        }
    }
    
    // 处理sub图层的键盘事件
    if (layer->sub) {
        handle_key_event(layer->sub, event);
    }
}

// 处理鼠标事件
void handle_mouse_event(Layer* layer, MouseEvent* event) {
    if (!layer || !event) {
        return;
    }
    
    // 优先处理popup层的事件
    if (popup_manager_handle_mouse_event(event)) {
        return;
    }
    
    Point mouse_pos = {event->x, event->y};
    if (point_in_rect(mouse_pos, layer->rect)) {
        // 处理hover和pressed状态
        if (layer->state != LAYER_STATE_DISABLED) {
            if (event->state == SDL_PRESSED) {
                // 鼠标按下时设置为PRESSED状态
                layer->state = LAYER_STATE_PRESSED;
            } else {
                // 鼠标悬停时设置为HOVER状态
                if (layer->state != LAYER_STATE_FOCUSED) {
                    layer->state = LAYER_STATE_HOVER;
                }
            }
        }
        
        // 处理焦点切换逻辑
        if (layer->focusable && event->state == SDL_PRESSED) {
            // 如果当前有焦点图层，将其状态设置为NORMAL
            if (focused_layer && focused_layer != layer) {
                focused_layer->state = LAYER_STATE_NORMAL;
            }
            // 设置新的焦点图层
            focused_layer = layer;
            layer->state = LAYER_STATE_FOCUSED;
        }

        if (event->state == SDL_PRESSED && layer->event && layer->event->click) {
            layer->event->click(layer);
        }
    } else {
        // 鼠标离开图层时，恢复为NORMAL状态（除非是DISABLED或FOCUSED状态）
        if (layer->state != LAYER_STATE_DISABLED && layer->state != LAYER_STATE_FOCUSED) {
            layer->state = LAYER_STATE_NORMAL;
        }
    }
    if (layer->handle_mouse_event) {
        layer->handle_mouse_event(layer, event);
    }
    
    // 递归处理子图层的事件
    for (int i = 0; i < layer->child_count; i++) {
        if (layer->children[i]) {
            handle_mouse_event(layer->children[i], event);
        }
    }
    
    // 递归处理子图层的事件
    if (layer->sub) {
        handle_mouse_event(layer->sub, event);
    }
}

// 处理滚动条拖动事件
// 辅助函数：处理单个图层的滚动条事件
static void process_layer_scrollbar(Layer* layer, int mouse_x, int mouse_y, SDL_EventType event_type) {
    // 处理垂直滚动条
    if (layer && (layer->scrollable == 1 || layer->scrollable == 3) && layer->scrollbar_v && layer->scrollbar_v->visible) {
        // 计算内容高度和可见高度
        int content_height = layer->content_height;
        
        int visible_height = layer->rect.h;
        if (layer->layout_manager) {
            visible_height -= layer->layout_manager->padding[0] + layer->layout_manager->padding[2];
        }
        
        if (content_height > visible_height) {
            // 计算滚动条位置和大小
            int scrollbar_width = layer->scrollbar_v->thickness > 0 ? layer->scrollbar_v->thickness : 8;
            int scrollbar_height = (int)((float)visible_height / content_height * visible_height);
            if (scrollbar_height < 20) {
                scrollbar_height = 20;
            }
            int scrollbar_x = layer->rect.x + layer->rect.w - scrollbar_width;
            int scrollbar_y = layer->rect.y + (int)((float)layer->scroll_offset / (content_height - visible_height) * (visible_height - scrollbar_height));
            
            // 处理鼠标按下事件
            if (event_type == SDL_MOUSEBUTTONDOWN) {
                // 检查鼠标是否在滚动条上
                if (mouse_x >= scrollbar_x && mouse_x <= scrollbar_x + scrollbar_width && 
                    mouse_y >= scrollbar_y && mouse_y <= scrollbar_y + scrollbar_height) {
                    layer->scrollbar_v->is_dragging = 1;
                    layer->scrollbar_v->drag_offset = mouse_y - scrollbar_y;
                }
            }
            // 处理鼠标移动事件
            else if (event_type == SDL_MOUSEMOTION && layer->scrollbar_v->is_dragging) {
                // 计算新的滚动条位置
                int new_scrollbar_y = mouse_y - layer->scrollbar_v->drag_offset;
                // 限制滚动条位置在可见区域内
                if (new_scrollbar_y < layer->rect.y) {
                    new_scrollbar_y = layer->rect.y;
                }
                if (new_scrollbar_y > layer->rect.y + visible_height - scrollbar_height) {
                    new_scrollbar_y = layer->rect.y + visible_height - scrollbar_height;
                }
                
                // 根据滚动条位置计算新的滚动偏移
                float scroll_ratio = (float)(new_scrollbar_y - layer->rect.y) / (visible_height - scrollbar_height);
                layer->scroll_offset = (int)(scroll_ratio * (content_height - visible_height));
                
                // 重新布局子元素
                layout_layer(layer);
            }
        }
    }
    
    // 处理水平滚动条
    if (layer && (layer->scrollable == 2 || layer->scrollable == 3) && layer->scrollbar_h && layer->scrollbar_h->visible) {
        // 计算内容宽度和可见宽度
        int content_width = layer->content_width;
        int visible_width = layer->rect.w;
        if (layer->layout_manager) {
            visible_width -= layer->layout_manager->padding[1] + layer->layout_manager->padding[3];
        }
        
        if (content_width > visible_width) {
            // 计算滚动条位置和大小
            int scrollbar_height = layer->scrollbar_h->thickness > 0 ? layer->scrollbar_h->thickness : 8;
            int scrollbar_width = (int)((float)visible_width / content_width * visible_width);
            if (scrollbar_width < 20) {
                scrollbar_width = 20;
            }
            int scrollbar_x = layer->rect.x + (int)((float)layer->scroll_offset_x / (content_width - visible_width) * (visible_width - scrollbar_width));
            int scrollbar_y = layer->rect.y + layer->rect.h - scrollbar_height;
            
            // 处理鼠标按下事件
            if (event_type == SDL_MOUSEBUTTONDOWN) {
                // 检查鼠标是否在滚动条上
                if (mouse_x >= scrollbar_x && mouse_x <= scrollbar_x + scrollbar_width && 
                    mouse_y >= scrollbar_y && mouse_y <= scrollbar_y + scrollbar_height) {
                    layer->scrollbar_h->is_dragging = 1;
                    layer->scrollbar_h->drag_offset = mouse_x - scrollbar_x;
                }
            }
            // 处理鼠标移动事件
            else if (event_type == SDL_MOUSEMOTION && layer->scrollbar_h->is_dragging) {
                // 计算新的滚动条位置
                int new_scrollbar_x = mouse_x - layer->scrollbar_h->drag_offset;
                // 限制滚动条位置在可见区域内
                if (new_scrollbar_x < layer->rect.x) {
                    new_scrollbar_x = layer->rect.x;
                }
                if (new_scrollbar_x > layer->rect.x + visible_width - scrollbar_width) {
                    new_scrollbar_x = layer->rect.x + visible_width - scrollbar_width;
                }
                
                // 根据滚动条位置计算新的滚动偏移
                float scroll_ratio = (float)(new_scrollbar_x - layer->rect.x) / (visible_width - scrollbar_width);
                layer->scroll_offset_x = (int)(scroll_ratio * (content_width - visible_width));
                
                // 重新布局子元素
                layout_layer(layer);
            }
        }
    }
    
    // 兼容性处理：旧的scrollbar字段
    if (layer && layer->scrollable && layer->scrollbar && layer->scrollbar->visible) {
        // 旧的滚动条处理逻辑保持不变，以确保向后兼容
        // 计算内容高度和可见高度
        int content_height = layer->content_height;
        
        
        int visible_height = layer->rect.h;
        if (layer->layout_manager) {
            visible_height -= layer->layout_manager->padding[0] + layer->layout_manager->padding[2];
        }
        
        if (content_height > visible_height) {
            // 计算滚动条位置和大小
            int scrollbar_width = layer->scrollbar->thickness > 0 ? layer->scrollbar->thickness : 8;
            int scrollbar_height = (int)((float)visible_height / content_height * visible_height);
            if (scrollbar_height < 20) {
                scrollbar_height = 20;
            }
            int scrollbar_x = layer->rect.x + layer->rect.w - scrollbar_width;
            int scrollbar_y = layer->rect.y + (int)((float)layer->scroll_offset / (content_height - visible_height) * (visible_height - scrollbar_height));
            
            // 处理鼠标按下事件
            if (event_type == SDL_MOUSEBUTTONDOWN) {
                // 检查鼠标是否在滚动条上
                if (mouse_x >= scrollbar_x && mouse_x <= scrollbar_x + scrollbar_width && 
                    mouse_y >= scrollbar_y && mouse_y <= scrollbar_y + scrollbar_height) {
                    layer->scrollbar->is_dragging = 1;
                    layer->scrollbar->drag_offset = mouse_y - scrollbar_y;
                }
            }
            // 处理鼠标移动事件
            else if (event_type == SDL_MOUSEMOTION && layer->scrollbar->is_dragging) {
                // 计算新的滚动条位置
                int new_scrollbar_y = mouse_y - layer->scrollbar->drag_offset;
                // 限制滚动条位置在可见区域内
                if (new_scrollbar_y < layer->rect.y) {
                    new_scrollbar_y = layer->rect.y;
                }
                if (new_scrollbar_y > layer->rect.y + visible_height - scrollbar_height) {
                    new_scrollbar_y = layer->rect.y + visible_height - scrollbar_height;
                }
                
                // 根据滚动条位置计算新的滚动偏移
                float scroll_ratio = (float)(new_scrollbar_y - layer->rect.y) / (visible_height - scrollbar_height);
                layer->scroll_offset = (int)(scroll_ratio * (content_height - visible_height));
                
                // 重新布局子元素
                layout_layer(layer);
            }
        }
    }
}

// 辅助函数：重置图层及其所有子图层的滚动条拖动状态
static void reset_scrollbar_dragging_state(Layer* layer) {
    if (!layer) {
        return;
    }
    
    // 重置当前图层的垂直滚动条拖动状态
    if (layer->scrollbar_v) {
        layer->scrollbar_v->is_dragging = 0;
    }
    
    // 重置当前图层的水平滚动条拖动状态
    if (layer->scrollbar_h) {
        layer->scrollbar_h->is_dragging = 0;
    }
    
    // 重置当前图层的旧滚动条拖动状态（向后兼容）
    if (layer->scrollbar) {
        layer->scrollbar->is_dragging = 0;
    }
    
    // 递归重置所有子图层的滚动条拖动状态
    for (int i = 0; i < layer->child_count; i++) {
        reset_scrollbar_dragging_state(layer->children[i]);
    }
    
    // 重置sub图层的滚动条拖动状态
    if (layer->sub) {
        reset_scrollbar_dragging_state(layer->sub);
    }
}

void handle_scrollbar_drag_event(Layer* root, int mouse_x, int mouse_y, SDL_EventType event_type) {
    if (!root) {
        return;
    }
    
    // 处理鼠标释放事件 - 单独处理，避免递归调用中的重复处理
    if (event_type == SDL_MOUSEBUTTONUP) {
        reset_scrollbar_dragging_state(root);
        return;
    }
    
    // 处理当前图层的滚动条
    process_layer_scrollbar(root, mouse_x, mouse_y, event_type);
    
    // 递归处理所有子图层的滚动条
    for (int i = 0; i < root->child_count; i++) {
        handle_scrollbar_drag_event(root->children[i], mouse_x, mouse_y, event_type);
    }
    
    // 处理sub图层的滚动条
    if (root->sub) {
        handle_scrollbar_drag_event(root->sub, mouse_x, mouse_y, event_type);
    }
}