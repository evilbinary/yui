#ifndef YUI_LAYER_PROPERTIES_H
#define YUI_LAYER_PROPERTIES_H

#include "ytype.h"
#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 从 JSON 对象解析并设置图层的单个属性
 * 
 * @param layer 目标图层
 * @param key 属性名
 * @param value 属性值（cJSON 对象）
 * @param is_creating 是否在创建新图层（影响某些默认值处理）
 * @return 1 成功，0 未处理，-1 失败
 */
int layer_set_property_from_json(Layer* layer, const char* key, cJSON* value, int is_creating);

/**
 * 从 JSON 对象批量设置图层属性
 * 
 * @param layer 目标图层
 * @param json_obj JSON 对象包含多个属性
 * @param is_creating 是否在创建新图层
 * @return 成功设置的属性数量
 */
int layer_set_properties_from_json(Layer* layer, cJSON* json_obj, int is_creating);

#ifdef __cplusplus
}
#endif

#endif // YUI_LAYER_PROPERTIES_H
