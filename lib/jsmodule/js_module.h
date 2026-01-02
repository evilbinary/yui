#ifndef YUI_JS_MODULE_H
#define YUI_JS_MODULE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "layer.h"
// 前向声明 Layer 类型（避免依赖 ytype.h）
typedef struct Layer Layer;
typedef struct cJSON cJSON;


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

// 触发事件（根据事件名称自动查找并调用对应的 JS 函数）
int js_module_trigger_event(const char* event_name, Layer* layer);

// 清空事件映射表
void js_module_clear_events(void);

// 设置 UI 根图层
void js_module_init_layer(Layer* root);


// 更新 UI 组件
void js_module_update_layer_text(Layer* layer, const char* text);

// 设置按钮样式
void js_module_set_button_style(Layer* layer, const char* bg_color);

// C 事件处理器类型
typedef void (*CEventHandler)(Layer* layer, const char* event_type);

// 注册 C 事件处理器
int js_module_register_c_event_handler(const char* event_name, CEventHandler handler);

// 根据 layer id 和事件类型调用事件处理器
// event_type: 事件类型，如 "onClick", "onLoad", "onPress" 等
int js_module_call_layer_event(const char* layer_id, const char* event_type);

// 从 Layer 的事件结构中触发事件（兼容旧的 Event 结构）
// layer: 图层指针
// event_name: 事件类型，如 "click", "press", "scroll" 等
int js_module_trigger_layer_event(Layer* layer, const char* event_name);

#ifdef __cplusplus
}
#endif

#endif
