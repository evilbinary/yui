#include "layer_update.h"
#include "layer_properties.h"
#include "layer.h"
#include "layout.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "util.h"


// ====================== 辅助函数 ======================

/**
 * 解析路径：支持 "id"、"children.0"、"children.id"、"a.b.c" 等格式
 * @return 目标图层，失败返回 NULL
 */
static Layer* resolve_path(Layer* root, const char* path) {
    if (!root || !path || path[0] == '\0') {
        return NULL;
    }
    
    // 复制路径字符串用于分割
    char path_copy[256];
    strncpy(path_copy, path, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';
    
    Layer* current = root;
    char* token = strtok(path_copy, ".");
    
    while (token != NULL && current != NULL) {
        // 检查是否是 children 关键字
        if (strcmp(token, "children") == 0) {
            token = strtok(NULL, ".");
            if (!token) break;
            
            // 检查是否是数字索引
            char* endptr;
            long index = strtol(token, &endptr, 10);
            
            if (*endptr == '\0' && index >= 0 && index < current->child_count) {
                // 数字索引：children.0
                current = current->children[index];
            } else {
                // ID 查找：children.id
                Layer* found = NULL;
                for (int i = 0; i < current->child_count; i++) {
                    if (strcmp(current->children[i]->id, token) == 0) {
                        found = current->children[i];
                        break;
                    }
                }
                current = found;
            }
        } else {
            // 普通 ID 查找
            current = find_layer_by_id(current, token);
        }
        
        token = strtok(NULL, ".");
    }
    
    return current;
}

/**
 * 解析整数数组 [a, b]
 */
int parse_int_array(cJSON* array, int* a, int* b) {
    if (!cJSON_IsArray(array) || cJSON_GetArraySize(array) != 2) {
        return -1;
    }
    
    cJSON* item0 = cJSON_GetArrayItem(array, 0);
    cJSON* item1 = cJSON_GetArrayItem(array, 1);
    
    if (!cJSON_IsNumber(item0) || !cJSON_IsNumber(item1)) {
        return -1;
    }
    
    *a = item0->valueint;
    *b = item1->valueint;
    
    return 0;
}

// ====================== 脏标记管理 ======================

void mark_layer_dirty(Layer* layer, unsigned int flags) {
    if (!layer) return;
    layer->dirty_flags |= flags;
}

void clear_dirty_flags(Layer* layer) {
    if (!layer) return;
    layer->dirty_flags = DIRTY_NONE;
}

// ====================== 便捷 API ======================

void yui_set_text(Layer* layer, const char* text) {
    if (!layer || !text) return;
    printf("yui_set_text: layer '%s' text='%s' (old='%s')\n", layer->id, text, layer_get_text(layer));
    layer_set_text(layer, text);
    mark_layer_dirty(layer, DIRTY_TEXT);
    printf("yui_set_text: layer '%s' text updated to '%s'\n", layer->id, layer_get_text(layer));
}

void yui_set_bg_color(Layer* layer, const char* color) {
    if (!layer || !color) return;
    
    Color parsed_color;
    parse_color((char*)color, &parsed_color);
    layer->bg_color = parsed_color;
    mark_layer_dirty(layer, DIRTY_COLOR);
}

void yui_set_visible(Layer* layer, int visible) {
    if (!layer) return;
    layer->visible = visible;
    mark_layer_dirty(layer, DIRTY_VISIBLE);
}

void yui_set_enabled(Layer* layer, int enabled) {
    if (!layer) return;
    layer->state = enabled ? LAYER_STATE_NORMAL : LAYER_STATE_DISABLED;
    mark_layer_dirty(layer, DIRTY_STYLE);
}

// ====================== 删除操作 ======================

int yui_remove_layer(Layer* root, const char* layer_id) {
    if (!root || !layer_id) return -1;
    
    Layer* target = find_layer_by_id(root, layer_id);
    if (!target || !target->parent) return -1;
    
    return yui_remove_child(target->parent, layer_id);
}

int yui_remove_child(Layer* parent, const char* child_id) {
    if (!parent || !child_id || !parent->children) return -1;
    
    // 查找子元素索引
    int index = -1;
    for (int i = 0; i < parent->child_count; i++) {
        if (strcmp(parent->children[i]->id, child_id) == 0) {
            index = i;
            break;
        }
    }
    
    if (index == -1) return -1;
    
    // 销毁子图层
    destroy_layer(parent->children[index]);
    
    // 移动后续元素
    for (int i = index; i < parent->child_count - 1; i++) {
        parent->children[i] = parent->children[i + 1];
    }
    
    parent->child_count--;
    mark_layer_dirty(parent, DIRTY_CHILDREN | DIRTY_LAYOUT);
    
    return 0;
}

int yui_remove_all_children(Layer* parent) {
    if (!parent || !parent->children) return 0;
    
    int count = parent->child_count;
    
    // 销毁所有子图层
    for (int i = 0; i < parent->child_count; i++) {
        if (parent->children[i]) {
            destroy_layer(parent->children[i]);
        }
    }
    
    free(parent->children);
    parent->children = NULL;
    parent->child_count = 0;
    
    mark_layer_dirty(parent, DIRTY_CHILDREN | DIRTY_LAYOUT);
    
    return count;
}

// ====================== 属性更新 ======================

/**
 * 更新单个属性
 * @return 1 表示已处理，0 表示未处理（需要继续处理）
 */
static int update_single_property(Layer* layer, const char* key, cJSON* value) {
    if (!layer || !key) return 0;
    
    // 处理 null 值（删除操作）
    if (cJSON_IsNull(value)) {
        // 特殊处理：删除整个图层
        if (strcmp(key, layer->id) == 0) {
            // 这个需要从父容器删除，这里只标记
            return 1;
        }
        
        // 特殊处理：清空 children
        if (strcmp(key, "children") == 0) {
            yui_remove_all_children(layer);
            return 1;
        }
        
        // 检查是否是 children.xxx 格式
        if (strncmp(key, "children.", 9) == 0) {
            const char* child_spec = key + 9;
            
            // 检查是否是数字索引
            char* endptr;
            long index = strtol(child_spec, &endptr, 10);
            
            if (*endptr == '\0' && index >= 0 && index < layer->child_count) {
                // 删除指定索引的子元素
                char child_id[50];
                strncpy(child_id, layer->children[index]->id, sizeof(child_id) - 1);
                yui_remove_child(layer, child_id);
            } else {
                // 删除指定 ID 的子元素
                yui_remove_child(layer, child_spec);
            }
            return 1;
        }
        
        // 其他属性设置为默认值
        if (strcmp(key, "visible") == 0) {
            layer->visible = 0;
            mark_layer_dirty(layer, DIRTY_VISIBLE);
        } else if (strcmp(key, "text") == 0) {
            layer_set_text(layer, "");
            mark_layer_dirty(layer, DIRTY_TEXT);
        }
        
        return 1;
    }
    
    // 使用共享的属性处理器（is_creating = 0 表示更新模式）
    return layer_set_property_from_json(layer, key, value, 0);
}

/**
 * 更新图层的所有属性
 */
int yui_update_properties(Layer* layer, cJSON* properties) {
    if (!layer || !properties || !cJSON_IsObject(properties)) {
        return 0;
    }
    
    int updated_count = 0;
    cJSON* item = NULL;
    
    cJSON_ArrayForEach(item, properties) {
        const char* key = item->string;
        if (update_single_property(layer, key, item)) {
            updated_count++;
        }
    }
    
    return updated_count;
}

// ====================== 主要 API ======================

/**
 * 从 JSON 对象更新单个图层
 */
int yui_update_from_json(Layer* root, cJSON* update_obj) {
    if (!root || !update_obj || !cJSON_IsObject(update_obj)) {
        printf("yui_update_from_json: 无效参数\n");
        return -1;
    }
    
    // 获取 target
    cJSON* target_json = cJSON_GetObjectItem(update_obj, "target");
    if (!target_json || !cJSON_IsString(target_json)) {
        printf("yui_update_from_json: 缺少 target 字段\n");
        return -1;
    }
    
    const char* target_path = target_json->valuestring;
    
    // 解析路径获取目标图层
    Layer* target_layer = resolve_path(root, target_path);
    if (!target_layer) {
        printf("yui_update_from_json: 未找到目标图层 '%s'\n", target_path);
        return -1;
    }
    
    // 获取 change 对象
    cJSON* change_json = cJSON_GetObjectItem(update_obj, "change");
    if (!change_json) {
        // 如果没有 change 字段，可能是旧格式，直接使用整个对象（除了 target）
        // 为了兼容性，这里可以支持
        printf("yui_update_from_json: 警告 - 缺少 change 字段，跳过\n");
        return -1;
    }
    
    // 应用更新
    int updated = yui_update_properties(target_layer, change_json);
    
    // 如果有布局变化，重新计算布局
    if (target_layer->dirty_flags & (DIRTY_RECT | DIRTY_LAYOUT | DIRTY_CHILDREN)) {
        if (target_layer->parent) {
            layout_layer(target_layer->parent);
        } else {
            layout_layer(target_layer);
        }
    }
    
    printf("yui_update_from_json: 成功更新 %d 个属性\n", updated);
    return 0;
}

/**
 * 应用 JSON 更新（自动识别单个或批量）
 */
int yui_update(Layer* root, const char* update_json) {
    if (!root || !update_json) {
        printf("yui_update: 无效参数\n");
        return -1;
    }
    
    // 解析 JSON
    cJSON* json = cJSON_Parse(update_json);
    if (!json) {
        printf("yui_update: JSON 解析失败\n");
        return -1;
    }
    
    int result = 0;
    
    if (cJSON_IsArray(json)) {
        // 批量更新
        printf("yui_update: 批量更新，共 %d 项\n", cJSON_GetArraySize(json));
        
        cJSON* item = NULL;
        int index = 0;
        cJSON_ArrayForEach(item, json) {
            int ret = yui_update_from_json(root, item);
            if (ret != 0) {
                printf("yui_update: 第 %d 项更新失败\n", index);
            }
            index++;
        }
    } else if (cJSON_IsObject(json)) {
        // 单个更新
        printf("yui_update: 单个更新\n");
        result = yui_update_from_json(root, json);
    } else {
        printf("yui_update: JSON 格式错误（必须是对象或数组）\n");
        result = -1;
    }
    
    cJSON_Delete(json);
    return result;
}
