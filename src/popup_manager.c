#include "popup_manager.h"
#include "event.h"
#include <stdlib.h>
#include <string.h>

// 全局弹出层管理器
PopupManager* g_popup_manager = NULL;

void popup_manager_init(void) {
    if (g_popup_manager) {
        return;
    }
    
    g_popup_manager = (PopupManager*)malloc(sizeof(PopupManager));
    if (!g_popup_manager) {
        return;
    }
    
    memset(g_popup_manager, 0, sizeof(PopupManager));
    g_popup_manager->max_popups = 10;
}

void popup_manager_cleanup(void) {
    if (!g_popup_manager) return;
    
    popup_manager_close_all();
    
    PopupLayer* current = g_popup_manager->active_popups;
    while (current) {
        PopupLayer* next = current->next;
        free(current);
        current = next;
    }
    
    free(g_popup_manager);
    g_popup_manager = NULL;
}

bool popup_manager_add(PopupLayer* popup) {
    if (!g_popup_manager || !popup) {
        return false;
    }
    
    // 检查是否已存在相同的layer
    PopupLayer* current = g_popup_manager->active_popups;
    while (current) {
        if (current && current->layer == popup->layer) {
            return false; // 已存在
        }
        current = current->next;
    }
    
    // 按优先级插入
    PopupLayer* prev = NULL;
    current = g_popup_manager->active_popups;
    
    while (current && current->priority >= popup->priority) {
        prev = current;
        current = current->next;
    }
    
    if (prev) {
        prev->next = popup;
    } else {
        g_popup_manager->active_popups = popup;
    }
    popup->next = current;
    
    // 更新顶部弹出层
    g_popup_manager->top_popup = g_popup_manager->active_popups;
    
    return true;
}

bool popup_manager_remove(Layer* layer) {
    if (!g_popup_manager || !layer) {
        return false;
    }
    
    PopupLayer* prev = NULL;
    PopupLayer* current = g_popup_manager->active_popups;
    
    while (current) {
        if (current && current->layer == layer) {
            // 先从链表中移除
            if (prev) {
                prev->next = current->next;
            } else {
                g_popup_manager->active_popups = current->next;
            }
            
            // 更新顶部弹出层
            g_popup_manager->top_popup = g_popup_manager->active_popups;
            
            // 保存next指针，因为在回调函数中可能会修改链表
            PopupLayer* next_to_free = current;
            
            // 将current的next设为NULL，防止回调函数中误用
            current->next = NULL;
            
            // 调用关闭回调（在释放之前）
            if (next_to_free->close_callback) {
                next_to_free->close_callback(next_to_free);
            }
            
            // 释放内存
            free(next_to_free);
            return true;
        }
        prev = current;
        current = current->next;
    }
    
    return false;
}

PopupLayer* popup_manager_get_top(void) {
    if (!g_popup_manager) return NULL;
    return g_popup_manager->top_popup;
}

void popup_manager_close_all(void) {
    if (!g_popup_manager) return;
    
    PopupLayer* current = g_popup_manager->active_popups;
    while (current) {
        PopupLayer* next = current->next;
        
        // 先将current的next设为NULL，防止回调函数中误用
        current->next = NULL;
        
        if (current->close_callback) {
            current->close_callback(current);
        }
        
        free(current);
        current = next;
    }
    
    g_popup_manager->active_popups = NULL;
    g_popup_manager->top_popup = NULL;
}

void popup_manager_close_by_type(PopupType type) {
    if (!g_popup_manager) return;
    
    PopupLayer* prev = NULL;
    PopupLayer* current = g_popup_manager->active_popups;
    
    while (current) {
        if (current && current->type == type) {
            PopupLayer* to_remove = current;
            current = current->next;
            
            if (prev) {
                prev->next = current;
            } else {
                g_popup_manager->active_popups = current;
            }
            
            // 先将to_remove的next设为NULL，防止回调函数中误用
            to_remove->next = NULL;
            
            if (to_remove->close_callback) {
                to_remove->close_callback(to_remove);
            }
            
            free(to_remove);
        } else {
            prev = current;
            current = current->next;
        }
    }
    
    g_popup_manager->top_popup = g_popup_manager->active_popups;
}

void popup_manager_render(void) {
    if (!g_popup_manager) return;
    
    PopupLayer* current = g_popup_manager->active_popups;
    while (current) {
        // 在调用回调函数前保存 next 指针，因为回调可能修改链表
        PopupLayer* next = current->next;
        
        if (current && current->layer && current->layer->render) {
            current->layer->render(current->layer);
        }
        
        // 使用保存的 next 指针
        current = next;
    }
}

bool popup_manager_handle_mouse_event(MouseEvent* event) {
    if (!g_popup_manager || !event) {
        return false;
    }
    
    if (!g_popup_manager->active_popups) {
        return false;
    }
    
    PopupLayer* current = g_popup_manager->active_popups;
    bool handled = false;
    
    // 处理popup内部的事件
    while (current) {
        // 检查指针是否有效
        if ((uintptr_t)current == 0xabababababababab || 
            (uintptr_t)current == 0xfeeefeeefeeefeee) {
            // 检测到 freed 内存指针，清空列表并返回
            printf("ERROR: popup_manager_handle_mouse_event - detected freed memory pointer, clearing list\n");
            g_popup_manager->active_popups = NULL;
            g_popup_manager->top_popup = NULL;
            return false;
        }
        
        // 在调用回调函数前保存 next 指针，因为回调可能修改链表
        PopupLayer* next = current->next;
        
        if (current && current->layer && current->layer->handle_mouse_event) {
            current->layer->handle_mouse_event(current->layer, event);
            handled = true;
        }
        
        // 使用保存的 next 指针
        current = next;
    }
    
    return handled;
}

bool popup_manager_handle_key_event(KeyEvent* event) {
    if (!g_popup_manager || !event) return false;
    
    PopupLayer* current = g_popup_manager->active_popups;
    bool handled = false;
    
    while (current) {
        // 在调用回调函数前保存 next 指针，因为回调可能修改链表
        PopupLayer* next = current->next;
        
        if (current && current->layer && current->layer->handle_key_event) {
            current->layer->handle_key_event(current->layer, event);
            handled = true;
        }
        
        // 使用保存的 next 指针
        current = next;
    }
    
    return handled;
}

bool popup_manager_handle_scroll_event(int scroll_delta) {
    if (!g_popup_manager) return false;
    
    PopupLayer* current = g_popup_manager->active_popups;
    bool handled = false;
    
    while (current) {
        // 在调用回调函数前保存 next 指针，因为回调可能修改链表
        PopupLayer* next = current->next;
        
        if (current && current->layer && current->layer->handle_scroll_event) {
            current->layer->handle_scroll_event(current->layer, scroll_delta);
            handled = true;
        }
        
        // 使用保存的 next 指针
        current = next;
    }
    
    return handled;
}

bool popup_manager_is_point_in_popups(int x, int y) {
    if (!g_popup_manager) return false;
    
    PopupLayer* current = g_popup_manager->active_popups;
    while (current) {
        if (current && current->layer) {
            Layer* layer = current->layer;
            if (x >= layer->rect.x && x < layer->rect.x + layer->rect.w &&
                y >= layer->rect.y && y < layer->rect.y + layer->rect.h) {
                return true;
            }
        }
        current = current->next;
    }
    
    return false;
}

PopupLayer* popup_layer_create(Layer* layer, PopupType type, int priority) {
    if (!layer) return NULL;
    
    PopupLayer* popup = (PopupLayer*)malloc(sizeof(PopupLayer));
    if (!popup) return NULL;
    
    memset(popup, 0, sizeof(PopupLayer));
    popup->layer = layer;
    popup->type = type;
    popup->priority = priority;
    popup->auto_close = true;
    popup->next = NULL;
    
    return popup;
}

void popup_layer_destroy(PopupLayer* popup) {
    // Note: This function should only be called on PopupLayers that are NOT in the manager's list
    // Use popup_manager_remove to properly remove and free a PopupLayer from the manager
    if (popup) {
        // Make sure the popup is not in the active list before freeing
        if (g_popup_manager) {
            PopupLayer* current = g_popup_manager->active_popups;
            while (current) {
                if (current == popup) {
                    printf("WARNING: Attempting to destroy a PopupLayer that is still in the manager's list\n");
                    printf("Use popup_manager_remove() instead of popup_layer_destroy()\n");
                    return;
                }
                current = current->next;
            }
        }
        free(popup);
    }
}

