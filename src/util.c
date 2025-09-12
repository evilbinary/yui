#include "util.h"
#include <float.h> // 添加这个头文件以定义 DBL_EPSILON


int is_cjson_float(const cJSON *item) {
    if (item == NULL || !cJSON_IsNumber(item)) {
        return 0;
    }
    
    // 获取双精度值
    double value = item->valuedouble;
    
    // 检查是否为有限数（非无穷大和非NaN）
    if (!isfinite(value)) {
        return 0;
    }
    
    // 检查值是否超出整数范围
    if (value > INT_MAX || value < INT_MIN) {
        return 1; // 超出整数范围，肯定是浮点数
    }
    
    // 检查值是否包含小数部分
    double int_part;
    double frac_part = modf(value, &int_part);
    
    // 如果小数部分的绝对值大于一个很小的容差值，则是浮点数
    if (fabs(frac_part) > DBL_EPSILON) {
        return 1;
    }
    
    return 0; // 是整数
}

// 替换字符串中的占位符
char* replace_placeholder(const char* template, const char* placeholder, const char* value) {
    // 查找占位符位置
    char* pos = strstr(template, placeholder);
    if (pos == NULL) return NULL;
    
    // 计算新字符串长度
    size_t template_len = strlen(template);
    size_t placeholder_len = strlen(placeholder);
    size_t value_len = strlen(value);
    size_t new_len = template_len - placeholder_len + value_len + 1;
    
    // 分配内存
    char* result = (char*)malloc(new_len);
    if (result == NULL) return NULL;
    
    // 复制第一部分
    size_t prefix_len = pos - template;
    strncpy(result, template, prefix_len);
    result[prefix_len] = '\0';
    
    // 追加替换值
    strcat(result, value);
    
    // 追加剩余部分
    strcat(result, pos + placeholder_len);
    
    return result;
}

// 替换所有出现的占位符
char* replace_all_placeholders(const char* template, const char* placeholder, const char* value) {
    char* result = strdup(template);
    if (result == NULL) return NULL;
    
    char* pos = strstr(result, placeholder);
    while (pos != NULL) {
        // 计算新字符串长度
        size_t result_len = strlen(result);
        size_t placeholder_len = strlen(placeholder);
        size_t value_len = strlen(value);
        size_t new_len = result_len - placeholder_len + value_len + 1;
        
        // 分配临时内存
        char* temp = (char*)malloc(new_len);
        if (temp == NULL) {
            free(result);
            return NULL;
        }
        
        // 复制第一部分
        size_t prefix_len = pos - result;
        strncpy(temp, result, prefix_len);
        temp[prefix_len] = '\0';
        
        // 追加替换值
        strcat(temp, value);
        
        // 追加剩余部分
        strcat(temp, pos + placeholder_len);
        
        // 释放旧结果，使用新结果
        free(result);
        result = temp;
        
        // 查找下一个占位符
        pos = strstr(result, placeholder);
    }
    
    return result;
}
