#ifndef YUI_JS_MODULE_H
#define YUI_JS_MODULE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// 前向声明 Layer 类型（避免依赖 ytype.h）
typedef struct Layer Layer;
typedef struct cJSON cJSON;

#ifdef HAS_JS_MODULE
#include "mquickjs.h"
#endif

// 初始化 JS 引擎
int js_module_init(void);

// 清理 JS 引擎
void js_module_cleanup(void);

// 加载并执行 JS 文件
int js_module_load_file(const char* filename);

// 注册 C 函数到 JS
void js_module_register_api(void);

// 从 JSON 中加载 JS 文件（json_file_path 用于相对路径解析）
int js_module_load_from_json(cJSON* root_json, const char* json_file_path);

// 调用 JS 函数（通过事件名）
int js_module_call_event(const char* event_name, Layer* layer);

// 设置 UI 根图层
void js_module_set_ui_root(Layer* root);

// 注册查找图层函数
void js_module_set_find_layer_func(struct Layer* (*func)(struct Layer* root, const char* id));

// 更新 UI 组件
void js_module_update_layer_text(Layer* layer, const char* text);

// 设置按钮样式
void js_module_set_button_style(Layer* layer, const char* bg_color);

#ifdef __cplusplus
}
#endif

#endif
