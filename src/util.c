#include "util.h"
#include <float.h> // 添加这个头文件以定义 DBL_EPSILON
#include "ytype.h"

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



// 辅助函数：将十六进制字符转换为数值
static int hex_to_nibble(char c) {
    c = tolower(c);
    if (c >= '0' && c <= '9') {
        return c - '0';
    } else if (c >= 'a' && c <= 'f') {
        return 10 + c - 'a';
    }
    return 0; // 默认返回0
}

// 将十六进制颜色字符串转换为Color结构体
Color color_from_hex(const char* hex) {
    Color color = {0, 0, 0, 255}; // 默认为黑色，完全不透明
    
    if (!hex) return color;
    
    // 跳过开头的'#'字符（如果有）
    if (hex[0] == '#') {
        hex++;
    }
    
    size_t len = strlen(hex);
    
    // 支持3位、6位和8位十六进制颜色
    if (len == 3) {
        // 3位十六进制颜色（如"FFF"）
        color.r = (hex_to_nibble(hex[0]) << 4) | hex_to_nibble(hex[0]);
        color.g = (hex_to_nibble(hex[1]) << 4) | hex_to_nibble(hex[1]);
        color.b = (hex_to_nibble(hex[2]) << 4) | hex_to_nibble(hex[2]);
    } else if (len == 6) {
        // 6位十六进制颜色（如"FFFFFF"）
        color.r = (hex_to_nibble(hex[0]) << 4) | hex_to_nibble(hex[1]);
        color.g = (hex_to_nibble(hex[2]) << 4) | hex_to_nibble(hex[3]);
        color.b = (hex_to_nibble(hex[4]) << 4) | hex_to_nibble(hex[5]);
    } else if (len == 8) {
        // 8位十六进制颜色（如"FFFFFFFF"）
        color.r = (hex_to_nibble(hex[0]) << 4) | hex_to_nibble(hex[1]);
        color.g = (hex_to_nibble(hex[2]) << 4) | hex_to_nibble(hex[3]);
        color.b = (hex_to_nibble(hex[4]) << 4) | hex_to_nibble(hex[5]);
        color.a = (hex_to_nibble(hex[6]) << 4) | hex_to_nibble(hex[7]);
    }
    
    return color;
}

// 检查点是否在矩形内
int is_point_in_rect(int x, int y, Rect rect) {
    return (x >= rect.x && x <= rect.x + rect.w) && 
           (y >= rect.y && y <= rect.y + rect.h);
}

void parse_color(char* valuestring, Color* color) {
    // 支持多种颜色格式
    if (strncmp(valuestring, "rgba(", 5) == 0) {
      // 解析 rgba(r, g, b, a) 格式，支持空格
      int r, g, b, a;
      sscanf(valuestring, "rgba(%d,%d,%d,%d)", &r, &g, &b, &a);
      color->r = (unsigned char)r;
      color->g = (unsigned char)g;
      color->b = (unsigned char)b;
      color->a = (unsigned char)a;  // 直接使用0-255的值
    } else if (strncmp(valuestring, "rgb(", 4) == 0) {
      // 解析 rgb(r, g, b) 格式，支持空格
      int r, g, b;
      sscanf(valuestring, "rgb(%d,%d,%d)", &r, &g, &b);
      color->r = (unsigned char)r;
      color->g = (unsigned char)g;
      color->b = (unsigned char)b;
      color->a = 255;  // 默认不透明
    } else if (strlen(valuestring) == 9) {
      // 解析十六进制 #RRGGBBAA 格式
      sscanf(valuestring, "#%02hhx%02hhx%02hhx%02hhx", &color->r, &color->g,
             &color->b, &color->a);
    } else {
      // 解析十六进制 #RRGGBB 格式
      sscanf(valuestring, "#%02hhx%02hhx%02hhx", &color->r, &color->g, &color->b);
      color->a = 255;  // 默认不透明
    }
  }