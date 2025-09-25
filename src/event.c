#include "event.h"
#include "util.h"
#include "backend.h"
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
void handle_scroll_event(Layer* layer, int scroll_delta) {
    if (layer->scrollable) {
        layer->scroll_offset += scroll_delta * 20; // 每次滚动20像素
        
        // 限制滚动范围
        int content_height = 0;
        for (int i = 0; i < layer->child_count; i++) {
            content_height += layer->children[i]->rect.h;
            if (i > 0) content_height += layer->layout_manager->spacing;
        }
        
        int visible_height = layer->rect.h - layer->layout_manager->padding[0] - layer->layout_manager->padding[2];
        
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

// 处理键盘事件
void handle_key_event(Layer* layer, KeyEvent* event) {
    if (!layer || !event) {
        return;
    }
    
    // 检查是否是INPUT类型的图层，并且有input_component
    if (layer->type == INPUT && layer->component) {
        input_component_handle_key_event(layer->component, event);
    }
    
    // 递归处理子图层的键盘事件
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