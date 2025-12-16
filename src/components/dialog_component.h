#ifndef YUI_DIALOG_COMPONENT_H
#define YUI_DIALOG_COMPONENT_H

#include "../ytype.h"

// 前向声明
typedef struct DialogComponent DialogComponent;

// 对话框按钮结构体
typedef struct {
    char text[64];                    // 按钮文本
    void (*callback)(DialogComponent* dialog, void* user_data);  // 按钮回调
    void* user_data;                  // 用户数据
    int is_default;                  // 是否为默认按钮（回车键触发）
    int is_cancel;                   // 是否为取消按钮（ESC键触发）
} DialogButton;

// 对话框类型
typedef enum {
    DIALOG_TYPE_INFO,        // 信息对话框
    DIALOG_TYPE_WARNING,     // 警告对话框
    DIALOG_TYPE_ERROR,       // 错误对话框
    DIALOG_TYPE_QUESTION,    // 问题对话框
    DIALOG_TYPE_CUSTOM       // 自定义对话框
} DialogType;

// 对话框组件结构体
struct DialogComponent {
    Layer* layer;              // 关联的图层
    Layer* popup_layer;        // 弹出对话框图层
    DialogType type;           // 对话框类型
    char title[256];           // 对话框标题
    char message[1024];        // 对话框消息
    DialogButton* buttons;     // 按钮数组
    int button_count;          // 按钮数量
    int selected_button;       // 当前选中的按钮索引
    int is_modal;              // 是否为模态对话框
    int is_opened;             // 对话框是否已打开
    void* user_data;           // 用户数据
    
    // 颜色配置
    Color title_color;         // 标题颜色
    Color text_color;          // 文本颜色
    Color bg_color;            // 背景颜色
    Color border_color;        // 边框颜色
    Color button_color;        // 按钮颜色
    Color button_hover_color;  // 按钮悬停颜色
    Color button_text_color;   // 按钮文本颜色
    
    // 回调函数
    void (*on_close)(DialogComponent* dialog, int button_index);
    void (*on_show)(DialogComponent* dialog);
};

// 函数声明
DialogComponent* dialog_component_create(Layer* layer);
void dialog_component_destroy(DialogComponent* component);

// 基本设置函数
void dialog_component_set_title(DialogComponent* component, const char* title);
void dialog_component_set_message(DialogComponent* component, const char* message);
void dialog_component_set_type(DialogComponent* component, DialogType type);
void dialog_component_set_modal(DialogComponent* component, int modal);
void dialog_component_set_user_data(DialogComponent* component, void* data);

// 按钮管理
void dialog_component_add_button(DialogComponent* component, const char* text, 
                                 void (*callback)(DialogComponent* dialog, void* user_data),
                                 void* user_data, int is_default, int is_cancel);
void dialog_component_clear_buttons(DialogComponent* component);
int dialog_component_get_button_count(DialogComponent* component);

// 颜色设置
void dialog_component_set_colors(DialogComponent* component, Color title, Color text, 
                                Color bg, Color border, Color button, 
                                Color button_hover, Color button_text);

// 显示和隐藏
bool dialog_component_show(DialogComponent* component, int x, int y);
void dialog_component_hide(DialogComponent* component);
bool dialog_component_is_opened(DialogComponent* component);

// 事件处理
void dialog_component_handle_mouse_event(Layer* layer, MouseEvent* event);
void dialog_component_handle_key_event(Layer* layer, KeyEvent* event);
void dialog_component_render(Layer* layer);

// 便捷函数
DialogComponent* dialog_component_show_info(const char* title, const char* message);
DialogComponent* dialog_component_show_warning(const char* title, const char* message);
DialogComponent* dialog_component_show_error(const char* title, const char* message);
DialogComponent* dialog_component_show_question(const char* title, const char* message);

// JSON创建
DialogComponent* dialog_component_create_from_json(Layer* layer, cJSON* json);

#endif  // YUI_DIALOG_COMPONENT_H