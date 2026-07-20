#include "event.h"
#include "util.h"
#include "backend.h"
#include "popup_manager.h"
#include "component_registry.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

// 全局事件处理数组
int global_event_handers_size=MAX_EVENT;
EventEntry global_event_handlers[MAX_EVENT];
// 当前已注册事件数量
int event_count = 0;
static PointerEvent current_pointer_event;
static int current_pointer_event_active = 0;
static int pointer_gesture_scrolled = 0;

void pointer_gesture_mark_scrolled(void) {
    pointer_gesture_scrolled = 1;
}

static int pointer_gesture_did_scroll(void) {
    return pointer_gesture_scrolled;
}

static void handler_virtical_scroll_event(Layer* layer, int scroll_delta);
static void handle_horizontal_scroll_event(Layer* layer, int scroll_delta);
static void process_layer_scrollbar(Layer* layer, int mouse_x, int mouse_y,
                                    SDL_EventType event_type);
static void reset_scrollbar_dragging_state(Layer* layer);

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
const PointerEvent* get_current_pointer_event(void) {
    return current_pointer_event_active ? &current_pointer_event : NULL;
}

const char* pointer_phase_to_string(PointerPhase phase) {
    switch (phase) {
        case POINTER_DOWN: return "start";
        case POINTER_MOVE: return "move";
        case POINTER_UP: return "end";
        case POINTER_WHEEL: return "wheel";
        case POINTER_SWIPE: return "swipe";
        case POINTER_DOUBLE_TAP: return "doubleTap";
        case POINTER_LONG_PRESS: return "longPress";
        case POINTER_PINCH: return "pinch";
        case POINTER_ROTATE: return "rotate";
        default: return "unknown";
    }
}

static void handler_virtical_scroll_event(Layer* layer, int scroll_delta) {
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
            EVENT_INVOKE(layer->event->scroll, layer);
        }
        // 重新布局子元素
        layout_layer(layer);
    }
}

static void handle_horizontal_scroll_event(Layer* layer, int scroll_delta) {
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
            EVENT_INVOKE(layer->event->scroll, layer);
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

    // 如果当前图层就是焦点图层且有键盘事件处理函数，则处理事件
    if (focused_layer == layer && layer->handle_key_event) {
        if (layer->handle_key_event(layer, event)) return;
    }

    // 递归处理子图层的键盘事件，寻找焦点图层
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

// 递归检查指定位置是否有子图层可以处理点击事件
static bool has_child_handler_at_point(Layer* layer, Point point) {
    if (!layer) return false;
    for (int i = 0; i < layer->child_count; i++) {
        Layer* child = layer->children[i];
        if (!child) continue;
        if (point_in_rect(point, child->rect)) {
            if (child->handle_pointer_event) return true;
            if (child->event && child->event->click) return true;
            if (has_child_handler_at_point(child, point)) return true;
        }
    }
    if (layer->sub && point_in_rect(point, layer->sub->rect)) {
        if (layer->sub->handle_pointer_event) return true;
        if (layer->sub->event && layer->sub->event->click) return true;
        if (has_child_handler_at_point(layer->sub, point)) return true;
    }
    return false;
}

// 默认指针事件处理（无自定义 handle_pointer_event 的图层）
int default_layer_handle_pointer_event(Layer* layer, PointerEvent* event) {
    if (!layer || !event) return 0;

    Point mouse_pos = {event->x, event->y};

    if (point_in_rect(mouse_pos, layer->rect)) {
        if (layer->state != LAYER_STATE_FOCUSED && layer->state != LAYER_STATE_DISABLED) {
            if (event->phase == POINTER_DOWN) {
                layer->state = LAYER_STATE_PRESSED;
            } else if (event->phase == POINTER_MOVE) {
                layer->state = LAYER_STATE_HOVER;
            }
        }
    } else {
        if (layer->state != LAYER_STATE_DISABLED && layer->state != LAYER_STATE_FOCUSED) {
            layer->state = LAYER_STATE_NORMAL;
        }
    }

    if (layer->event && layer->event->click) {
        const YuiComponentOps* ops = yui_type_get_ops(layer->type);
        if (ops && (ops->flags & YUI_COMP_NATIVE_RENDER) &&
            event->phase == POINTER_UP && event->button == SDL_BUTTON_LEFT &&
            point_in_rect(mouse_pos, layer->rect)) {
            bool has_handler = has_child_handler_at_point(layer, mouse_pos);
            if (!has_handler && layer->parent) {
                for (int i = 0; i < layer->parent->child_count && !has_handler; i++) {
                    Layer* sibling = layer->parent->children[i];
                    if (sibling && sibling != layer && sibling->handle_pointer_event &&
                        point_in_rect(mouse_pos, sibling->rect)) {
                        has_handler = true;
                    }
                }
            }
            if (!has_handler) {
                EVENT_INVOKE(layer->event->click, layer);
            }
        }
    }
    if (event->phase == POINTER_DOWN || event->phase == POINTER_UP) {
        if (layer->child_count > 0) {
            return 0;
        }
        return point_in_rect(mouse_pos, layer->rect) ? 1 : 0;
    }
    return 0;
}

/* 内容区拖动滚动（mouse / touch 共用） */
static int apply_content_drag_scroll(Layer* layer, int dx, int dy) {
    int adx;
    int ady;
    if (!layer) {
        return 0;
    }
    if (layer->scrollable != 1 && layer->scrollable != 3) {
        return 0;
    }
    adx = dx < 0 ? -dx : dx;
    ady = dy < 0 ? -dy : dy;
    if (adx < 2 && ady < 2) {
        return 1;
    }
    if (adx > ady) {
        return 0;
    }
    if (layout_scroll_vertical(layer, dy)) {
        pointer_gesture_mark_scrolled();
        return 1;
    }
    return 0;
}

static int layer_is_scrollable_container(const Layer* layer) {
    return layer && (layer->type == VIEW || layer->type == GRID) &&
           (layer->scrollable == 1 || layer->scrollable == 3);
}

static int layer_scrollbar_dragging(const Layer* layer) {
    if (!layer) {
        return 0;
    }
    return (layer->scrollbar_v && layer->scrollbar_v->is_dragging) ||
           (layer->scrollbar_h && layer->scrollbar_h->is_dragging) ||
           (layer->scrollbar && layer->scrollbar->is_dragging);
}

static int default_scrollable_pointer_handler(Layer* layer, PointerEvent* event) {
    if (!layer || !event) {
        return 0;
    }
    if (!layer_is_scrollable_container(layer)) {
        return 0;
    }

    if (event->phase == POINTER_WHEEL) {
        if (!is_point_in_rect(event->x, event->y, layer->rect)) {
            return 0;
        }
        if (layer->handle_scroll_event) {
            layer->handle_scroll_event(layer, event->delta_y);
        }
        handler_virtical_scroll_event(layer, event->delta_y);
        handle_horizontal_scroll_event(layer, event->delta_x);
        return layer->handle_scroll_event || layer->scrollable ? 1 : 0;
    }

    if (event->phase == POINTER_DOWN) {
        return is_point_in_rect(event->x, event->y, layer->rect);
    }

    if (event->phase == POINTER_MOVE) {
        if (event->delta_x == 0 && event->delta_y == 0) {
            return 0;
        }
        if (event->device == POINTER_DEVICE_TOUCH && event->finger_count > 1) {
            return 0;
        }
        if (layer_scrollbar_dragging(layer)) {
            return 0;
        }
        return apply_content_drag_scroll(layer, event->delta_x, event->delta_y);
    }

    if (event->phase == POINTER_UP) {
        return is_point_in_rect(event->x, event->y, layer->rect);
    }

    return 0;
}

static SDL_EventType pointer_phase_to_sdl_type(PointerPhase phase) {
    if (phase == POINTER_DOWN) {
        return SDL_MOUSEBUTTONDOWN;
    }
    if (phase == POINTER_UP) {
        return SDL_MOUSEBUTTONUP;
    }
    return SDL_MOUSEMOTION;
}

int handle_pointer_event(Layer* layer, PointerEvent* event) {
    PointerEvent scroll_cancel_event;
    PointerEvent* pe = event;

    if (!layer || !event) {
        return 0;
    }

    if (layer->parent == NULL) {
        if (event->phase == POINTER_DOWN) {
            pointer_gesture_scrolled = 0;
        }
        if (event->phase == POINTER_WHEEL &&
            popup_manager_handle_scroll_event(event->delta_y)) {
            return 1;
        }
        if (popup_manager_handle_pointer_event(event)) {
            return 1;
        }
        if (event->device == POINTER_DEVICE_MOUSE && event->phase == POINTER_UP) {
            reset_scrollbar_dragging_state(layer);
        }
        /* 滚动过的手势：UP 统一改写为 CANCEL，子组件只收尾、不触发 click */
        if (event->phase == POINTER_UP && pointer_gesture_did_scroll()) {
            scroll_cancel_event = *event;
            scroll_cancel_event.phase = POINTER_CANCEL;
            pe = &scroll_cancel_event;
        }
    }

    Point pos = {pe->x, pe->y};

    for (int i = layer->child_count - 1; i >= 0; i--) {
        if (layer->children[i] && layer->children[i]->visible == VISIBLE) {
            int consumed = handle_pointer_event(layer->children[i], pe);
            if (consumed) return 1;
        }
    }

    int child_has_focus = 0;
    if (pe->phase == POINTER_DOWN && focused_layer) {
        for (int i = 0; i < layer->child_count; i++) {
            if (layer->children[i] == focused_layer) {
                child_has_focus = 1;
                break;
            }
        }
    }

    if (point_in_rect(pos, layer->rect)) {
        if (!child_has_focus && layer->focusable && layer->visible == VISIBLE &&
            pe->phase == POINTER_DOWN) {
            if (focused_layer && focused_layer != layer) {
                focused_layer->state = LAYER_STATE_NORMAL;
            }
            focused_layer = layer;
            layer->state = LAYER_STATE_FOCUSED;
        }
    }

    if (layer->sub && layer->sub->visible == VISIBLE) {
        int consumed = handle_pointer_event(layer->sub, pe);
        if (consumed) return 1;
    }

    if (event->device == POINTER_DEVICE_MOUSE &&
        pe->phase != POINTER_UP && pe->phase != POINTER_CANCEL) {
        process_layer_scrollbar(layer, pe->x, pe->y,
                                pointer_phase_to_sdl_type(pe->phase));
        if (layer_scrollbar_dragging(layer)) {
            return 1;
        }
    }

    if (layer->handle_pointer_event) {
        int consumed = layer->handle_pointer_event(layer, pe);
        if (consumed) return 1;
    }

    if (default_scrollable_pointer_handler(layer, pe)) {
        return 1;
    }

    if (layer->event && layer->event->touch && pe->device == POINTER_DEVICE_TOUCH) {
        current_pointer_event = *pe;
        current_pointer_event_active = 1;
        EVENT_INVOKE(layer->event->touch, layer);
        current_pointer_event_active = 0;
        return 1;
    }

    return default_layer_handle_pointer_event(layer, pe);
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

                if (layer->layout_manager && layer->child_count > 0) {
                    layout_layer(layer);
                }
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

                if (layer->layout_manager && layer->child_count > 0) {
                    layout_layer(layer);
                }
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

                if (layer->layout_manager && layer->child_count > 0) {
                    layout_layer(layer);
                }
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
