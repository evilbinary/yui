#ifndef YAML_CJSON_H
#define YAML_CJSON_H

#include "cJSON.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 将YAML字符串转换为cJSON对象
 * 
 * @param yaml_str YAML字符串
 * @param error 错误信息输出（可传NULL）
 * @return cJSON* 转换后的cJSON对象，需要调用cJSON_Delete释放
 */
cJSON* yaml2cjson(const char *yaml_str, char **error);

/**
 * @brief 将cJSON对象转换为YAML字符串
 * 
 * @param json cJSON对象
 * @param indent 缩进空格数（0表示不格式化）
 * @param error 错误信息输出（可传NULL）
 * @return char* 转换后的YAML字符串，需要调用free释放
 */
char* cjson2yaml(cJSON *json, int indent, char **error);

/**
 * @brief 从文件读取YAML并转换为cJSON对象
 * 
 * @param filename 文件名
 * @param error 错误信息输出
 * @return cJSON* 转换后的cJSON对象
 */
cJSON* yaml_file2cjson(const char *filename, char **error);

/**
 * @brief 将cJSON对象写入YAML文件
 * 
 * @param json cJSON对象
 * @param filename 文件名
 * @param indent 缩进空格数
 * @param error 错误信息输出
 * @return int 0成功，-1失败
 */
int cjson2yaml_file(cJSON *json, const char *filename, int indent, char **error);

#ifdef __cplusplus
}
#endif

#endif // YAML_CJSON_H