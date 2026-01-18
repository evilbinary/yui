#ifndef YUI_JSON_UPDATE_H
#define YUI_JSON_UPDATE_H

#include "ytype.h"
#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

// 脏标记常量
#define DIRTY_NONE       0x0000
#define DIRTY_RECT       0x0001  // 位置/尺寸变化
#define DIRTY_COLOR      0x0002  // 颜色变化
#define DIRTY_TEXT       0x0004  // 文本变化
#define DIRTY_CHILDREN   0x0008  // 子节点变化
#define DIRTY_VISIBLE    0x0010  // 可见性变化
#define DIRTY_STYLE      0x0020  // 样式变化
#define DIRTY_LAYOUT     0x0040  // 布局变化
#define DIRTY_ALL        0xFFFF

// ====================== 主要 API ======================

/**
 * 应用 JSON 更新（自动识别单个或批量）
 * @param root 根图层
 * @param update_json JSON 字符串，格式：
 *   单个: { "target": "id", "text": "...", ... }
 *   批量: [{ "target": "id1", ... }, { "target": "id2", ... }]
 * @return 0 成功，-1 失败
 */
int yui_update(Layer* root, const char* update_json);

/**
 * 从 JSON 对象更新单个图层
 * @param root 根图层
 * @param update_obj cJSON 对象，必须包含 "target" 字段
 * @return 0 成功，-1 失败
 */
int yui_update_from_json(Layer* root, cJSON* update_obj);

/**
 * 直接更新图层属性（自动标记脏）
 * @param layer 目标图层
 * @param properties cJSON 对象包含要更改的属性
 * @return 更新的属性数量
 */
int yui_update_properties(Layer* layer, cJSON* properties);

// ====================== 便捷 API ======================

/**
 * 设置文本并自动标记脏
 */
void yui_set_text(Layer* layer, const char* text);

/**
 * 设置背景颜色并自动标记脏
 */
void yui_set_bg_color(Layer* layer, const char* color);

/**
 * 设置可见性并自动标记脏
 */
void yui_set_visible(Layer* layer, int visible);

/**
 * 设置启用状态并自动标记脏
 */
void yui_set_enabled(Layer* layer, int enabled);

// ====================== 删除操作 ======================

/**
 * 从父容器中删除指定图层
 * @param root 根图层（用于查找目标）
 * @param layer_id 要删除的图层 ID
 * @return 0 成功，-1 失败
 */
int yui_remove_layer(Layer* root, const char* layer_id);

/**
 * 从父图层删除指定子元素
 * @param parent 父图层
 * @param child_id 要删除的子元素 ID
 * @return 0 成功，-1 失败
 */
int yui_remove_child(Layer* parent, const char* child_id);

/**
 * 清空父图层的所有子元素
 * @param parent 父图层
 * @return 删除的子元素数量
 */
int yui_remove_all_children(Layer* parent);

// ====================== 脏标记管理 ======================

/**
 * 标记图层为脏
 */
void mark_layer_dirty(Layer* layer, unsigned int flags);

/**
 * 清除脏标记
 */
void clear_dirty_flags(Layer* layer);

// ====================== 辅助函数 ======================


/**
 * 解析数组 [a, b]
 */
int parse_int_array(cJSON* array, int* a, int* b);

#ifdef __cplusplus
}
#endif

#endif // YUI_JSON_UPDATE_H
