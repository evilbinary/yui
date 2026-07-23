#ifndef YUI_TEXT_COMPONENT_H
#define YUI_TEXT_COMPONENT_H

#include "../ytype.h"
#include "text_syntax.h"

typedef struct Layer Layer;
typedef struct KeyEvent KeyEvent;

// 多行文本组件结构体
typedef struct {
    Layer* layer;          // 关联的图层
    // 注意：文本内容现在存储在 layer->text 中，不再使用固定大小的数组
    char placeholder[MAX_TEXT];  // 占位文本
    int cursor_pos;        // 光标位置
    int cursor_visual_line; // 光标所在视觉行，消除自动换行边界位置歧义
    int selection_start;   // 选择起始位置
    int selection_end;     // 选择结束位置
    int max_length;        // 最大输入长度
    Color cursor_color;    // 光标颜色
    int scroll_x;          // 水平滚动位置
    int scroll_y;          // 垂直滚动位置
    int line_height;       // 行高
    int multiline;         // 是否为多行模式
    int wrap;              // 多行时是否按宽度自动换行
    int editable;          // 是否可编辑
    int show_line_numbers; // 是否显示行号
    int line_number_width; // 行号区域宽度
    Color line_number_color; // 行号颜色
    Color line_number_bg_color; // 行号背景颜色
    Color selection_color; // 选中背景颜色
    int is_selecting;       // 是否正在选择文本
    EventHandler on_change; // onChange 回调函数
    char* change_name;        // 用户数据
    int cached_line_height; // 缓存的行高（用于性能优化）
    int line_height_valid;   // 行高缓存是否有效
    TextSyntaxConfig syntax_config; // 语法高亮配置
    int text_revision;       // 文本变更版本号
    int* layout_starts;      // 视觉行起始偏移缓存
    int layout_count;        // 视觉行数量
    int layout_cache_revision; // 布局缓存对应的文本版本
    int layout_cache_max_width; // 布局缓存对应的最大宽度
    int layout_cache_text_len;  // 布局缓存对应的文本长度
} TextComponent;

// 函数声明
TextComponent* text_component_create(Layer* layer);
TextComponent* text_component_create_from_json(Layer* layer,cJSON* json_obj);
void text_component_destroy(TextComponent* component);
void text_component_set_text(TextComponent* component, const char* text);
void text_component_set_placeholder(TextComponent* component, const char* placeholder);
void text_component_set_max_length(TextComponent* component, int max_length);
void text_component_set_cursor_color(TextComponent* component, Color cursor_color);
void text_component_set_multiline(TextComponent* component, int multiline);
void text_component_set_wrap(TextComponent* component, int wrap);
void text_component_set_editable(TextComponent* component, int editable);
void text_component_set_show_line_numbers(TextComponent* component, int show_line_numbers);
void text_component_set_line_number_width(TextComponent* component, int width);
void text_component_set_line_number_color(TextComponent* component, Color color);
void text_component_set_line_number_bg_color(TextComponent* component, Color color);
void text_component_set_selection_color(TextComponent* component, Color color);
void text_component_set_syntax_highlight(TextComponent* component, const char* language);
void text_component_invalidate_layout(TextComponent* component);
void text_component_set_on_change(TextComponent* component, EventHandler callback, void* user_data);
int text_component_handle_key_event(Layer* layer, KeyEvent* event);
int text_component_handle_pointer_event(Layer* layer, PointerEvent* event);
void text_component_render(Layer* layer);
int text_component_get_position_from_point(TextComponent* component, Point pt, Layer* layer);
int text_component_register_event(Layer* layer, const char* event_name, const char* event_func_name, EventHandler event_handler);
void text_component_trigger_on_change(TextComponent* component);
void text_component_update_scroll_for_cursor(TextComponent* component);

// 通用属性获取 / 设置
cJSON* text_component_get_property(Layer* layer, const char* property_name);
int text_component_set_property_from_json(Layer* layer, const char* key, cJSON* value, int is_creating);
// 辅助函数声明
int get_line_start(TextComponent* component, int pos);
int get_line_end(TextComponent* component, int pos);
int text_component_calculate_content_height(TextComponent* component);
void text_component_update_content_height(TextComponent* component);
#endif  // YUI_TEXT_COMPONENT_H