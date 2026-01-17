#ifndef YUI_THEME_H
#define YUI_THEME_H

#include "ytype.h"
#include "cJSON.h"

// 主题选择器类型
typedef enum {
    THEME_SELECTOR_ID,      // ID选择器（#componentId）
    THEME_SELECTOR_TYPE     // 类型选择器（ComponentType）
} ThemeSelectorType;

// 主题规则（对应一个选择器）
typedef struct ThemeRule {
    char selector[256];           // "#id" 或 "Type"
    ThemeSelectorType selector_type;  // 选择器类型
    
    // 样式属性
    Color color;                  // 文字颜色
    Color bg_color;               // 背景颜色
    int font_size;                // 字体大小
    char font_weight[20];         // 字体粗细
    int radius;                   // 圆角半径
    int border_width;             // 边框宽度
    Color border_color;           // 边框颜色
    int spacing;                  // 间距
    int padding[4];               // 内边距 [top, right, bottom, left]
    int opacity;                  // 透明度 0-255
    int width;                    // 宽度
    int height;                   // 高度
    
    struct ThemeRule* next;       // 链表下一个
} ThemeRule;

// 主题对象
typedef struct Theme {
    char name[256];
    char version[64];
    ThemeRule* rules;             // 样式规则链表（按优先级排序）
} Theme;

// 创建主题对象
Theme* theme_create(const char* name, const char* version);

// 销毁主题对象
void theme_destroy(Theme* theme);

// 从JSON文件加载主题
Theme* theme_load_from_file(const char* json_path);

// 从JSON对象加载主题
Theme* theme_load_from_json(cJSON* json);

// 添加规则到主题
void theme_add_rule(Theme* theme, ThemeRule* rule);

// 从JSON创建规则
ThemeRule* theme_rule_create_from_json(cJSON* json);

// 销毁规则
void theme_rule_destroy(ThemeRule* rule);

// 应用主题样式到图层
void theme_apply_to_layer(Theme* theme, Layer* layer, const char* id, const char* type);

// 合并样式（按优先级）
void theme_merge_style(ThemeRule* rule, Layer* layer);

// 解析选择器类型
ThemeSelectorType theme_parse_selector_type(const char* selector);

#endif // YUI_THEME_H
