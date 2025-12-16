#ifndef YUI_MENU_COMPONENT_H
#define YUI_MENU_COMPONENT_H

#include "../ytype.h"

// 声明外部scale变量（用于DPI缩放）
extern float scale;

// 前向声明
typedef struct PopupLayer PopupLayer;
typedef struct MenuComponent MenuComponent;

// 菜单项结构体
typedef struct {
    char text[256];           // 菜单项文本
    char shortcut[64];        // 快捷键
    int enabled;              // 是否启用
    int checked;              // 是否选中（用于复选框类型菜单项）
    int separator;            // 是否为分隔符
    void (*callback)(void*);  // 点击回调函数
    void* user_data;          // 用户数据
} MenuItem;

// 菜单组件结构体
struct MenuComponent {
    Layer* layer;              // 关联的图层
    Layer* popup_layer;        // 弹出菜单图层
    MenuItem* items;           // 菜单项数组
    int item_count;            // 菜单项数量
    int hovered_item;          // 当前悬停的菜单项索引
    int opened_item;           // 当前打开的子菜单索引
    int is_popup;              // 是否为弹出菜单
    int is_submenu;            // 是否为子菜单
    struct MenuComponent* parent_menu;  // 父菜单指针
    Color bg_color;            // 背景颜色
    Color text_color;          // 文本颜色
    Color hover_color;         // 悬停颜色
    Color disabled_color;      // 禁用颜色
    Color separator_color;     // 分隔符颜色
    int item_height;           // 菜单项高度
    int min_width;             // 最小宽度
    void* user_data;           // 用户数据
    void (*on_popup_closed)(MenuComponent* menu);  // 弹出菜单关闭回调
};

// 函数声明
MenuComponent* menu_component_create(Layer* layer);
void menu_component_destroy(MenuComponent* component);
MenuComponent* menu_component_create_from_json(Layer* layer, cJSON* json);
void menu_component_add_item(MenuComponent* component, const char* text, const char* shortcut, 
                            int enabled, int checked, int separator, 
                            void (*callback)(void*), void* user_data);
void menu_component_clear_items(MenuComponent* component);
void menu_component_set_colors(MenuComponent* component, Color bg, Color text, 
                              Color hover, Color disabled, Color separator);
void menu_component_set_item_height(MenuComponent* component, int height);
void menu_component_set_min_width(MenuComponent* component, int width);
void menu_component_set_user_data(MenuComponent* component, void* data);
void menu_component_handle_mouse_event(Layer* layer, MouseEvent* event);
void menu_component_handle_key_event(Layer* layer, KeyEvent* event);
void menu_component_render(Layer* layer);
int menu_component_get_item_at_position(MenuComponent* component, int x, int y);

// 弹出菜单相关函数
bool menu_component_show_popup(MenuComponent* component, int x, int y);
void menu_component_hide_popup(MenuComponent* component);
bool menu_component_is_popup_opened(MenuComponent* component);
void menu_component_set_popup_closed_callback(MenuComponent* component, void (*callback)(MenuComponent* menu));

#endif  // YUI_MENU_COMPONENT_H