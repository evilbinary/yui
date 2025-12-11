// 弹出层管理器集成示例
// 在 main.c 的 backend_init() 之后添加以下代码

#include "popup_manager.h"

// 在 main() 函数中的 backend_init() 之后添加：
void init_popup_system(void) {
    popup_manager_init();
    printf("DEBUG: Popup system initialized\n");
}

// 在渲染循环中的适当位置添加：
void render_popups(void) {
    // 在所有常规组件渲染之后渲染弹出层
    popup_manager_render();
}

// 在事件处理中添加：
bool handle_popup_events(MouseEvent* mouse_event, KeyEvent* key_event, int scroll_delta) {
    bool handled = false;
    
    // 优先处理弹出层事件
    if (mouse_event) {
        handled = popup_manager_handle_mouse_event(mouse_event);
    }
    
    if (!handled && key_event) {
        handled = popup_manager_handle_key_event(key_event);
    }
    
    if (!handled && scroll_delta != 0) {
        handled = popup_manager_handle_scroll_event(scroll_delta);
    }
    
    // 如果点击不在任何弹出层内，关闭所有自动关闭的弹出层
    if (mouse_event && mouse_event->state == SDL_PRESSED && 
        mouse_event->button == SDL_BUTTON_LEFT && !handled) {
        
        if (!popup_manager_is_point_in_popups(mouse_event->x, mouse_event->y)) {
            popup_manager_close_all();
        }
    }
    
    return handled;
}

// 在程序退出时添加：
void cleanup_popup_system(void) {
    popup_manager_cleanup();
    printf("DEBUG: Popup system cleaned up\n");
}

/*
在 main.c 中的使用示例：

int main(int argc, char* argv[]) {
    backend_init();
    
    // 初始化弹出层系统
    init_popup_system();
    
    // ... 其他初始化代码 ...
    
    // 在主循环中：
    while (running) {
        // 处理事件时优先处理弹出层
        bool event_handled = handle_popup_events(mouse_event, key_event, scroll_delta);
        
        if (!event_handled) {
            // 处理常规组件事件
            // ...
        }
        
        // 渲染常规组件
        // ...
        
        // 最后渲染弹出层（确保在最上层）
        render_popups();
        
        backend_present();
    }
    
    // 清理弹出层系统
    cleanup_popup_system();
    backend_quit();
    
    return 0;
}
*/