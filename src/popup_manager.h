#ifndef POPUP_MANAGER_H
#define POPUP_MANAGER_H

#include "layer.h"
#include "backend.h"
#include <stdbool.h>

// 弹出层类型
typedef enum {
    POPUP_TYPE_DROPDOWN,
    POPUP_TYPE_MENU,
    POPUP_TYPE_DIALOG,
    POPUP_TYPE_TOOLTIP
} PopupType;

// 弹出层结构
typedef struct PopupLayer {
    Layer* layer;
    PopupType type;
    int priority;  // 优先级，数字越大层级越高
    bool auto_close;
    void (*close_callback)(struct PopupLayer* popup);
    struct PopupLayer* next;
} PopupLayer;

// 弹出层管理器
typedef struct {
    PopupLayer* active_popups;
    PopupLayer* top_popup;
    int max_popups;
} PopupManager;

// 全局弹出层管理器
extern PopupManager* g_popup_manager;

// 弹出层管理函数
void popup_manager_init(void);
void popup_manager_cleanup(void);

// 添加弹出层
bool popup_manager_add(PopupLayer* popup);

// 移除弹出层
bool popup_manager_remove(Layer* layer);

// 获取顶部弹出层
PopupLayer* popup_manager_get_top(void);

// 关闭所有弹出层
void popup_manager_close_all(void);

// 关闭指定类型的弹出层
void popup_manager_close_by_type(PopupType type);

// 渲染所有弹出层
void popup_manager_render(void);

// 处理弹出层事件
bool popup_manager_handle_mouse_event(MouseEvent* event);
bool popup_manager_handle_key_event(KeyEvent* event);
bool popup_manager_handle_scroll_event(int scroll_delta);

// 检查点击是否在任何弹出层内
bool popup_manager_is_point_in_popups(int x, int y);

// 创建弹出层辅助函数
PopupLayer* popup_layer_create(Layer* layer, PopupType type, int priority);
void popup_layer_destroy(PopupLayer* popup);

#endif // POPUP_MANAGER_H