#ifndef TAB_COMPONENT_H
#define TAB_COMPONENT_H

#include "../layer.h"
#include "../ytype.h"

#ifdef __cplusplus
extern "C" {
#endif

// 选项卡组件
typedef struct TabItem {
    char* title;
    Layer* content_layer;
    int enabled;
    void* user_data;
} TabItem;

typedef struct TabComponent {
    Layer* layer;
    TabItem* tabs;
    int tab_count;
    int active_tab;
    int tab_height;
    Color tab_color;
    Color active_tab_color;
    Color text_color;
    Color active_text_color;
    Color border_color;
    int closable;
    void* user_data;
    void (*on_tab_changed)(int old_tab, int new_tab, void* user_data);
    void (*on_tab_close)(int tab_index, void* user_data);
} TabComponent;

// 创建选项卡组件
TabComponent* tab_component_create(Layer* layer);

// 从 JSON 创建选项卡组件
TabComponent* tab_component_create_from_json(Layer* layer, cJSON* json);

// 销毁选项卡组件
void tab_component_destroy(TabComponent* component);

// 添加选项卡
int tab_component_add_tab(TabComponent* component, const char* title, Layer* content_layer);

// 移除选项卡
void tab_component_remove_tab(TabComponent* component, int index);

// 设置活动选项卡
void tab_component_set_active_tab(TabComponent* component, int index);

// 获取活动选项卡
int tab_component_get_active_tab(TabComponent* component);

// 设置选项卡标题
void tab_component_set_tab_title(TabComponent* component, int index, const char* title);

// 获取选项卡标题
const char* tab_component_get_tab_title(TabComponent* component, int index);

// 设置选项卡内容
void tab_component_set_tab_content(TabComponent* component, int index, Layer* content_layer);

// 获取选项卡内容
Layer* tab_component_get_tab_content(TabComponent* component, int index);

// 启用/禁用选项卡
void tab_component_set_tab_enabled(TabComponent* component, int index, int enabled);

// 检查选项卡是否启用
int tab_component_is_tab_enabled(TabComponent* component, int index);

// 设置选项卡高度
void tab_component_set_tab_height(TabComponent* component, int height);

// 设置颜色
void tab_component_set_colors(TabComponent* component, Color tab, Color active_tab, Color text, Color active_text, Color border);

// 设置是否可关闭
void tab_component_set_closable(TabComponent* component, int closable);

// 设置用户数据
void tab_component_set_user_data(TabComponent* component, void* data);

// 设置选项卡变化回调
void tab_component_set_tab_changed_callback(TabComponent* component, void (*callback)(int, int, void*));

// 设置选项卡关闭回调
void tab_component_set_tab_close_callback(TabComponent* component, void (*callback)(int, void*));

// 处理鼠标事件
void tab_component_handle_mouse_event(Layer* layer, MouseEvent* event);

// 处理键盘事件
void tab_component_handle_key_event(Layer* layer, KeyEvent* event);

// 渲染选项卡
void tab_component_render(Layer* layer);

#ifdef __cplusplus
}
#endif

#endif // TAB_COMPONENT_H