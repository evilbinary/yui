#ifndef YUI_BUTTON_COMPONENT_H
#define YUI_BUTTON_COMPONENT_H

#include "../ytype.h"

// 按钮组件结构体
typedef struct {
    Layer* layer;          // 关联的图层
    Color colors[16];       // 不同状态下的颜色 [NORMAL, HOVER, PRESSED, DISABLED]
    void* user_data;       // 用户数据
    char* icon_path;       // SVG/图片文件路径
    char* icon_text;       // 图标文本 (emoji等)
    struct Texture* icon_tex;  // 缓存的图标纹理
    int icon_size;         // 图标最大尺寸 (0 = 自动)
    int bg_transparent;    // bgColor 显式设为 transparent
} ButtonComponent;

// 函数声明
ButtonComponent* button_component_create(Layer* layer);
ButtonComponent* button_component_create_from_json(Layer* layer, cJSON* json_obj);
void button_component_destroy(ButtonComponent* component);
void button_component_set_text(ButtonComponent* component, const char* text);
void button_component_set_color(ButtonComponent* component, LayerState state, Color color);
void button_component_set_icon_path(ButtonComponent* component, const char* path);
void button_component_set_icon_text(ButtonComponent* component, const char* text);
void button_component_set_user_data(ButtonComponent* component, void* data);
int button_component_handle_mouse_event(Layer* layer, MouseEvent* event);
void button_component_render(Layer* layer);
int button_component_handle_key_event(Layer* layer, KeyEvent* event);

#endif  // YUI_BUTTON_COMPONENT_H