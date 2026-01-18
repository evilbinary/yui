#include "theme_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 静态单例实例
static ThemeManager* g_theme_manager = NULL;

// 获取主题管理器实例（单例）
ThemeManager* theme_manager_get_instance(void) {
    if (!g_theme_manager) {
        g_theme_manager = (ThemeManager*)malloc(sizeof(ThemeManager));
        if (!g_theme_manager) {
            return NULL;
        }
        
        memset(g_theme_manager, 0, sizeof(ThemeManager));
        
        // 初始容量
        g_theme_manager->theme_capacity = 10;
        g_theme_manager->themes = (Theme**)malloc(sizeof(Theme*) * g_theme_manager->theme_capacity);
        if (!g_theme_manager->themes) {
            free(g_theme_manager);
            g_theme_manager = NULL;
            return NULL;
        }
        
        memset(g_theme_manager->themes, 0, sizeof(Theme*) * g_theme_manager->theme_capacity);
    }
    
    return g_theme_manager;
}

// 销毁主题管理器
void theme_manager_destroy(void) {
    if (!g_theme_manager) {
        return;
    }
    
    // 销毁所有主题
    for (int i = 0; i < g_theme_manager->theme_count; i++) {
        if (g_theme_manager->themes[i]) {
            theme_destroy(g_theme_manager->themes[i]);
        }
    }
    
    // 释放数组
    if (g_theme_manager->themes) {
        free(g_theme_manager->themes);
    }
    
    // 释放管理器
    free(g_theme_manager);
    g_theme_manager = NULL;
}

// 加载主题文件
Theme* theme_manager_load_theme(const char* theme_path) {
    if (!theme_path) {
        return NULL;
    }
    
    ThemeManager* manager = theme_manager_get_instance();
    if (!manager) {
        return NULL;
    }
    
    // 从文件加载主题
    Theme* theme = theme_load_from_file(theme_path);
    if (!theme) {
        printf("Failed to load theme from: %s\n", theme_path);
        return NULL;
    }
    
    // 检查是否需要扩展数组
    if (manager->theme_count >= manager->theme_capacity) {
        int new_capacity = manager->theme_capacity * 2;
        Theme** new_themes = (Theme**)realloc(manager->themes, sizeof(Theme*) * new_capacity);
        if (!new_themes) {
            theme_destroy(theme);
            return NULL;
        }
        
        manager->themes = new_themes;
        manager->theme_capacity = new_capacity;
        
        // 初始化新分配的空间
        for (int i = manager->theme_count; i < new_capacity; i++) {
            manager->themes[i] = NULL;
        }
    }
    
    // 添加到数组
    manager->themes[manager->theme_count++] = theme;
    
    printf("Loaded theme: %s (version: %s)\n", theme->name, theme->version);
    
    return theme;
}

// 从JSON字符串加载主题
Theme* theme_manager_load_theme_from_json(const char* json_str) {
    if (!json_str) {
        return NULL;
    }
    
    ThemeManager* manager = theme_manager_get_instance();
    if (!manager) {
        return NULL;
    }
    
    // 解析JSON字符串
    cJSON* json = cJSON_Parse(json_str);
    if (!json) {
        printf("Failed to parse theme JSON string\n");
        return NULL;
    }
    
    // 从JSON创建主题
    Theme* theme = theme_load_from_json(json);
    cJSON_Delete(json);
    
    if (!theme) {
        printf("Failed to load theme from JSON string\n");
        return NULL;
    }
    
    // 检查是否需要扩展数组
    if (manager->theme_count >= manager->theme_capacity) {
        int new_capacity = manager->theme_capacity * 2;
        Theme** new_themes = (Theme**)realloc(manager->themes, sizeof(Theme*) * new_capacity);
        if (!new_themes) {
            theme_destroy(theme);
            return NULL;
        }
        
        manager->themes = new_themes;
        manager->theme_capacity = new_capacity;
        
        // 初始化新分配的空间
        for (int i = manager->theme_count; i < new_capacity; i++) {
            manager->themes[i] = NULL;
        }
    }
    
    // 添加到数组
    manager->themes[manager->theme_count++] = theme;
    
    printf("Loaded theme from JSON: %s (version: %s)\n", theme->name, theme->version);
    
    return theme;
}

// 设置当前主题
int theme_manager_set_current(const char* theme_name) {
    if (!theme_name) {
        return 0;
    }
    
    ThemeManager* manager = theme_manager_get_instance();
    if (!manager) {
        return 0;
    }
    
    // 查找主题
    Theme* theme = theme_manager_get_theme(theme_name);
    if (!theme) {
        printf("Theme not found: %s\n", theme_name);
        return 0;
    }
    
    // 设置当前主题
    manager->current_theme = theme;
    
    printf("Current theme set to: %s\n", theme_name);
    
    return 1;
}

// 获取当前主题
Theme* theme_manager_get_current(void) {
    ThemeManager* manager = theme_manager_get_instance();
    if (!manager) {
        return NULL;
    }
    
    return manager->current_theme;
}

// 获取主题（按名称）
Theme* theme_manager_get_theme(const char* theme_name) {
    if (!theme_name) {
        return NULL;
    }
    
    ThemeManager* manager = theme_manager_get_instance();
    if (!manager) {
        return NULL;
    }
    
    // 查找主题
    for (int i = 0; i < manager->theme_count; i++) {
        if (manager->themes[i] && strcmp(manager->themes[i]->name, theme_name) == 0) {
            return manager->themes[i];
        }
    }
    
    return NULL;
}

// 卸载主题
void theme_manager_unload_theme(const char* theme_name) {
    if (!theme_name) {
        return;
    }
    
    ThemeManager* manager = theme_manager_get_instance();
    if (!manager) {
        return;
    }
    
    // 查找并移除主题
    for (int i = 0; i < manager->theme_count; i++) {
        if (manager->themes[i] && strcmp(manager->themes[i]->name, theme_name) == 0) {
            // 如果这是当前主题，清空当前主题
            if (manager->current_theme == manager->themes[i]) {
                manager->current_theme = NULL;
            }
            
            // 销毁主题
            theme_destroy(manager->themes[i]);
            
            // 从数组中移除
            for (int j = i; j < manager->theme_count - 1; j++) {
                manager->themes[j] = manager->themes[j + 1];
            }
            manager->theme_count--;
            
            printf("Unloaded theme: %s\n", theme_name);
            return;
        }
    }
}

// 应用当前主题到图层
void theme_manager_apply_to_layer(Layer* layer, const char* id, const char* type) {
    if (!layer || !id || !type) {
        return;
    }
    
    ThemeManager* manager = theme_manager_get_instance();
    if (!manager) {
        return;
    }
    
    Theme* current_theme = manager->current_theme;
    if (!current_theme) {
        return;
    }
    
    // 应用主题
    theme_apply_to_layer(current_theme, layer, id, type);
}

// 递归应用主题到图层树
static void theme_manager_apply_to_layer_recursive(Layer* layer) {
    if (!layer) {
        return;
    }
    
    // 获取组件类型名称
    const char* type_name = layer_type_name[layer->type];
    
    // 应用主题到当前图层
    theme_manager_apply_to_layer(layer, layer->id, type_name);
    
    // 递归应用到子图层
    for (int i = 0; i < layer->child_count; i++) {
        if (layer->children[i]) {
            theme_manager_apply_to_layer_recursive(layer->children[i]);
        }
    }
}

// 应用当前主题到图层树
void theme_manager_apply_to_tree(Layer* root) {
    if (!root) {
        return;
    }
    
    ThemeManager* manager = theme_manager_get_instance();
    if (!manager) {
        return;
    }
    
    Theme* current_theme = manager->current_theme;
    if (!current_theme) {
        printf("No current theme set\n");
        return;
    }
    
    printf("Applying theme '%s' to layer tree\n", current_theme->name);
    
    // 递归应用
    theme_manager_apply_to_layer_recursive(root);
}
