#ifndef YUI_THEME_MANAGER_H
#define YUI_THEME_MANAGER_H

#include "theme.h"

// 主题管理器（单例模式）
typedef struct ThemeManager {
    Theme* current_theme;      // 当前主题
    Theme** themes;            // 主题数组
    int theme_count;           // 主题数量
    int theme_capacity;        // 主题数组容量
} ThemeManager;

// 获取主题管理器实例（单例）
ThemeManager* theme_manager_get_instance(void);

// 销毁主题管理器
void theme_manager_destroy(void);

// 加载主题文件
Theme* theme_manager_load_theme(const char* theme_path);

// 从JSON字符串加载主题
Theme* theme_manager_load_theme_from_json(const char* json_str);

// 设置当前主题
int theme_manager_set_current(const char* theme_name);

// 获取当前主题
Theme* theme_manager_get_current();

// 获取主题（按名称）
Theme* theme_manager_get_theme(const char* theme_name);

// 卸载主题
void theme_manager_unload_theme(const char* theme_name);

// 应用当前主题到图层
void theme_manager_apply_to_layer(Layer* layer, const char* id, const char* type);

// 应用当前主题到图层树
void theme_manager_apply_to_tree(Layer* root);

#endif // YUI_THEME_MANAGER_H
