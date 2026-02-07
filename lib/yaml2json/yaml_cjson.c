#include "yaml_cjson.h"
#include <yaml.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

// 错误处理宏
#define YC_ERROR(ctx, fmt, ...) \
    do { \
        if ((ctx)->error && *(ctx)->error) { \
            free(**(ctx)->error); \
            **(ctx)->error = NULL; \
        } \
        if ((ctx)->error) { \
            char buf[1024]; \
            snprintf(buf, sizeof(buf), fmt, ##__VA_ARGS__); \
            if (*(ctx)->error) { \
                **(ctx)->error = strdup(buf); \
            } \
        } \
    } while(0)

#define YC_CHECK(cond, ctx, fmt, ...) \
    do { \
        if (!(cond)) { \
            YC_ERROR(ctx, fmt, ##__VA_ARGS__); \
            return 0; \
        } \
    } while(0)

// 转换上下文
typedef struct {
    cJSON *root;               // 根节点
    cJSON *current;            // 当前处理节点
    cJSON **stack;             // 节点栈
    int stack_size;           // 栈大小
    int stack_capacity;       // 栈容量
    char *current_key;        // 当前键名
    bool in_mapping_key;      // 是否在映射键中
    bool in_sequence;         // 是否在序列中
    char ***error;           // 错误信息指针
} yaml_to_cjson_ctx;

// 创建上下文
static yaml_to_cjson_ctx* create_yaml_to_cjson_ctx(char ***error) {
    yaml_to_cjson_ctx *ctx = calloc(1, sizeof(yaml_to_cjson_ctx));
    if (!ctx) return NULL;
    
    ctx->stack_capacity = 16;
    ctx->stack = malloc(ctx->stack_capacity * sizeof(cJSON*));
    if (!ctx->stack) {
        free(ctx);
        return NULL;
    }
    
    ctx->error = error;
    return ctx;
}

// 释放上下文
static void free_yaml_to_cjson_ctx(yaml_to_cjson_ctx *ctx) {
    if (!ctx) return;
    
    free(ctx->stack);
    free(ctx->current_key);
    free(ctx);
}

// 压栈
static int push_context(yaml_to_cjson_ctx *ctx, cJSON *value) {
    if (ctx->stack_size >= ctx->stack_capacity) {
        ctx->stack_capacity *= 2;
        cJSON **new_stack = realloc(ctx->stack, 
                                   ctx->stack_capacity * sizeof(cJSON*));
        YC_CHECK(new_stack, ctx, "内存分配失败");
        ctx->stack = new_stack;
    }
    
    ctx->stack[ctx->stack_size++] = ctx->current;
    ctx->current = value;
    return 1;
}

// 弹栈
static int pop_context(yaml_to_cjson_ctx *ctx) {
    YC_CHECK(ctx->stack_size > 0, ctx, "栈下溢");
    ctx->current = ctx->stack[--ctx->stack_size];
    return 1;
}

// 尝试解析为数字
static cJSON* try_parse_number(const char *str, size_t len) {
    char *buf = malloc(len + 1);
    if (!buf) return NULL;
    
    memcpy(buf, str, len);
    buf[len] = '\0';
    
    char *endptr = NULL;
    
    // 尝试整数
    long long int_val = strtoll(buf, &endptr, 0);
    if (*endptr == '\0') {
        free(buf);
        return cJSON_CreateNumber(int_val);
    }
    
    // 尝试浮点数
    double double_val = strtod(buf, &endptr);
    if (*endptr == '\0') {
        free(buf);
        return cJSON_CreateNumber(double_val);
    }
    
    free(buf);
    return NULL;
}

// 创建标量值节点
static cJSON* create_scalar_value(const char *value, size_t length) {
    char *str = malloc(length + 1);
    if (!str) return NULL;
    
    memcpy(str, value, length);
    str[length] = '\0';
    
    // 检查布尔值
    if (strcmp(str, "true") == 0 || strcmp(str, "True") == 0 || 
        strcmp(str, "TRUE") == 0) {
        free(str);
        return cJSON_CreateTrue();
    }
    
    if (strcmp(str, "false") == 0 || strcmp(str, "False") == 0 || 
        strcmp(str, "FALSE") == 0) {
        free(str);
        return cJSON_CreateFalse();
    }
    
    // 检查null
    if (strcmp(str, "null") == 0 || strcmp(str, "Null") == 0 || 
        strcmp(str, "NULL") == 0 || strcmp(str, "~") == 0) {
        free(str);
        return cJSON_CreateNull();
    }
    
    // 尝试解析为数字
    cJSON *num = try_parse_number(str, strlen(str));
    if (num) {
        free(str);
        return num;
    }
    
    // 作为字符串处理
    cJSON *json_str = cJSON_CreateString(str);
    free(str);
    return json_str;
}

// 处理YAML事件
static int handle_yaml_event(yaml_to_cjson_ctx *ctx, yaml_event_t *event) {
    switch (event->type) {
        case YAML_STREAM_START_EVENT:
        case YAML_STREAM_END_EVENT:
        case YAML_DOCUMENT_START_EVENT:
        case YAML_DOCUMENT_END_EVENT:
            break;
            
        case YAML_SEQUENCE_START_EVENT: {
            cJSON *array = cJSON_CreateArray();
            YC_CHECK(array, ctx, "无法创建数组");
            
            if (!ctx->root) {
                ctx->root = array;
                ctx->current = array;
            } else if (ctx->current && cJSON_IsObject(ctx->current) && ctx->current_key) {
                // 对象中的数组
                cJSON_AddItemToObject(ctx->current, ctx->current_key, array);
                free(ctx->current_key);
                ctx->current_key = NULL;
                ctx->in_mapping_key = false;
                push_context(ctx, array);
            } else if (ctx->current && cJSON_IsArray(ctx->current)) {
                // 数组中的数组
                cJSON_AddItemToArray(ctx->current, array);
                push_context(ctx, array);
            } else {
                cJSON_Delete(array);
                YC_CHECK(0, ctx, "无效的序列开始位置");
            }
            
            ctx->in_sequence = true;
            break;
        }
            
        case YAML_SEQUENCE_END_EVENT:
            if (ctx->stack_size > 0) {
                pop_context(ctx);
            }
            ctx->in_sequence = false;
            break;
            
        case YAML_MAPPING_START_EVENT: {
            cJSON *object = cJSON_CreateObject();
            YC_CHECK(object, ctx, "无法创建对象");
            
            if (!ctx->root) {
                ctx->root = object;
                ctx->current = object;
            } else if (ctx->current && cJSON_IsObject(ctx->current) && ctx->current_key) {
                // 对象中的对象
                cJSON_AddItemToObject(ctx->current, ctx->current_key, object);
                free(ctx->current_key);
                ctx->current_key = NULL;
                ctx->in_mapping_key = false;
                push_context(ctx, object);
            } else if (ctx->current && cJSON_IsArray(ctx->current)) {
                // 数组中的对象
                cJSON_AddItemToArray(ctx->current, object);
                push_context(ctx, object);
            } else {
                cJSON_Delete(object);
                YC_CHECK(0, ctx, "无效的映射开始位置");
            }
            break;
        }
            
        case YAML_MAPPING_END_EVENT:
            if (ctx->stack_size > 0) {
                pop_context(ctx);
            }
            break;
            
        case YAML_SCALAR_EVENT: {
            const char *value = (const char*)event->data.scalar.value;
            size_t length = event->data.scalar.length;
            
            cJSON *scalar = create_scalar_value(value, length);
            YC_CHECK(scalar, ctx, "无法创建标量值");
            
            if (!ctx->root) {
                // 文档只有一个标量值
                ctx->root = scalar;
            } else if (ctx->current && cJSON_IsObject(ctx->current) && !ctx->in_mapping_key) {
                // 这是映射的键
                free(ctx->current_key);
                ctx->current_key = malloc(length + 1);
                YC_CHECK(ctx->current_key, ctx, "内存分配失败");
                memcpy(ctx->current_key, value, length);
                ctx->current_key[length] = '\0';
                ctx->in_mapping_key = true;
                cJSON_Delete(scalar);
            } else if (ctx->current && cJSON_IsObject(ctx->current) && ctx->in_mapping_key && ctx->current_key) {
                // 这是映射的值
                cJSON_AddItemToObject(ctx->current, ctx->current_key, scalar);
                free(ctx->current_key);
                ctx->current_key = NULL;
                ctx->in_mapping_key = false;
            } else if (ctx->current && cJSON_IsArray(ctx->current)) {
                // 数组中的值
                cJSON_AddItemToArray(ctx->current, scalar);
            } else {
                cJSON_Delete(scalar);
                YC_CHECK(0, ctx, "无效的标量位置");
            }
            break;
        }
            
        case YAML_ALIAS_EVENT:
            // 暂不支持别名
            break;
            
        default:
            break;
    }
    
    return 1;
}

// YAML转cJSON主函数
cJSON* yaml2cjson(const char *yaml_str, char **error) {
    yaml_parser_t parser;
    yaml_event_t event;
    yaml_to_cjson_ctx *ctx = create_yaml_to_cjson_ctx(&error);
    
    if (!ctx) {
        if (error) *error = strdup("无法创建转换上下文");
        return NULL;
    }
    
    // 初始化YAML解析器
    if (!yaml_parser_initialize(&parser)) {
        YC_ERROR(ctx, "无法初始化YAML解析器");
        goto cleanup;
    }
    
    yaml_parser_set_input_string(&parser, 
        (const unsigned char*)yaml_str, strlen(yaml_str));
    
    int done = 0;
    while (!done) {
        if (!yaml_parser_parse(&parser, &event)) {
            YC_ERROR(ctx, "YAML解析错误: %s", parser.problem);
            goto cleanup_parser;
        }
        
        done = (event.type == YAML_STREAM_END_EVENT);
        
        if (!handle_yaml_event(ctx, &event)) {
            yaml_event_delete(&event);
            goto cleanup_parser;
        }
        
        yaml_event_delete(&event);
    }
    
    yaml_parser_delete(&parser);
    cJSON *result = ctx->root;
    ctx->root = NULL;  // 防止被free_yaml_to_cjson_ctx释放
    
    free_yaml_to_cjson_ctx(ctx);
    return result;
    
cleanup_parser:
    yaml_parser_delete(&parser);
cleanup:
    if (ctx->root) cJSON_Delete(ctx->root);
    free_yaml_to_cjson_ctx(ctx);
    return NULL;
}

// 从文件读取YAML
cJSON* yaml_file2cjson(const char *filename, char **error) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        if (error) *error = strdup("无法打开文件");
        return NULL;
    }
    
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    char *buffer = malloc(size + 1);
    if (!buffer) {
        fclose(file);
        if (error) *error = strdup("内存分配失败");
        return NULL;
    }
    
    fread(buffer, 1, size, file);
    buffer[size] = '\0';
    fclose(file);
    
    cJSON *result = yaml2cjson(buffer, error);
    free(buffer);
    return result;
}

// 生成YAML缩进
static void generate_indent(char **output, int indent, int level) {
    for (int i = 0; i < level * indent; i++) {
        *output = realloc(*output, strlen(*output) + 2);
        strcat(*output, " ");
    }
}

// 添加字符串到输出
static void add_to_output(char **output, const char *str) {
    size_t new_len = (*output ? strlen(*output) : 0) + strlen(str) + 1;
    *output = realloc(*output, new_len);
    strcat(*output, str);
}

// 判断字符串是否需要引号
static bool need_quotes(const char *str) {
    if (!str || str[0] == '\0') return true;
    
    // 检查是否以特殊字符开头
    if (strchr("!&*?|>'\"/\\%#@`-", str[0])) return true;
    
    // 检查是否看起来像数字
    char *endptr;
    strtod(str, &endptr);
    if (*endptr == '\0') return true;
    
    // 检查布尔/null关键字
    if (strcmp(str, "true") == 0 || strcmp(str, "false") == 0 || 
        strcmp(str, "null") == 0 || strcmp(str, "True") == 0 || 
        strcmp(str, "False") == 0 || strcmp(str, "NULL") == 0 ||
        strcmp(str, "~") == 0) {
        return true;
    }
    
    // 检查是否包含特殊字符
    for (const char *p = str; *p; p++) {
        if (strchr(":{}[]#&*!|>\\\"'%@-", *p) || isspace(*p)) {
            return true;
        }
    }
    
    return false;
}

// 转义字符串
static char* escape_string(const char *str) {
    size_t len = strlen(str);
    char *result = malloc(len * 4 + 3);  // 足够空间用于转义
    char *ptr = result;
    
    *ptr++ = '"';
    
    for (size_t i = 0; i < len; i++) {
        switch (str[i]) {
            case '\\': *ptr++ = '\\'; *ptr++ = '\\'; break;
            case '"':  *ptr++ = '\\'; *ptr++ = '"'; break;
            case '\n': *ptr++ = '\\'; *ptr++ = 'n'; break;
            case '\t': *ptr++ = '\\'; *ptr++ = 't'; break;
            case '\r': *ptr++ = '\\'; *ptr++ = 'r'; break;
            case '\b': *ptr++ = '\\'; *ptr++ = 'b'; break;
            case '\f': *ptr++ = '\\'; *ptr++ = 'f'; break;
            default:   *ptr++ = str[i]; break;
        }
    }
    
    *ptr++ = '"';
    *ptr = '\0';
    return result;
}

// 递归转换cJSON到YAML
static int cjson_to_yaml_recursive(cJSON *json, char **output, int indent, 
                                  int level, bool is_array_item, char ***error) {
    if (!json) return 1;
    
    switch (json->type & 0xFF) {  // 清除标志位
        case cJSON_NULL:
            if (is_array_item) {
                generate_indent(output, indent, level);
            }
            add_to_output(output, "null\n");
            break;
            
        case cJSON_False:
            if (is_array_item) {
                generate_indent(output, indent, level);
            }
            add_to_output(output, "false\n");
            break;
            
        case cJSON_True:
            if (is_array_item) {
                generate_indent(output, indent, level);
            }
            add_to_output(output, "true\n");
            break;
            
        case cJSON_Number:
            if (is_array_item) {
                generate_indent(output, indent, level);
            }
            if (json->valueint == json->valuedouble) {
                char buf[64];
                snprintf(buf, sizeof(buf), "%d\n", json->valueint);
                add_to_output(output, buf);
            } else {
                char buf[64];
                snprintf(buf, sizeof(buf), "%.15g\n", json->valuedouble);
                add_to_output(output, buf);
            }
            break;
            
        case cJSON_String:
            if (is_array_item) {
                generate_indent(output, indent, level);
            }
            if (need_quotes(json->valuestring)) {
                char *escaped = escape_string(json->valuestring);
                add_to_output(output, escaped);
                add_to_output(output, "\n");
                free(escaped);
            } else {
                add_to_output(output, json->valuestring);
                add_to_output(output, "\n");
            }
            break;
            
        case cJSON_Array: {
            if (!is_array_item) {
                add_to_output(output, "\n");
            }
            
            cJSON *child = json->child;
            while (child) {
                generate_indent(output, indent, level);
                add_to_output(output, "- ");
                
                if (cjson_to_yaml_recursive(child, output, indent, level + 1, true, error) != 1) {
                    return 0;
                }
                
                child = child->next;
            }
            break;
        }
            
        case cJSON_Object: {
            if (!is_array_item) {
                add_to_output(output, "\n");
            }
            
            cJSON *child = json->child;
            while (child) {
                generate_indent(output, indent, level);
                
                // 键
                if (need_quotes(child->string)) {
                    char *escaped = escape_string(child->string);
                    add_to_output(output, escaped);
                    add_to_output(output, ": ");
                    free(escaped);
                } else {
                    add_to_output(output, child->string);
                    add_to_output(output, ": ");
                }
                
                // 值
                if (cjson_to_yaml_recursive(child, output, indent, level + 1, false, error) != 1) {
                    return 0;
                }
                
                child = child->next;
            }
            break;
        }
            
        default:
            if (error && *error) {
                **error = strdup("未知的JSON类型");
            }
            return 0;
    }
    
    return 1;
}

// cJSON转YAML主函数
char* cjson2yaml(cJSON *json, int indent, char **error) {
    if (!json) {
        if (error) *error = strdup("JSON对象为空");
        return NULL;
    }
    
    if (indent < 0) indent = 2;
    if (indent > 8) indent = 8;
    
    char *output = malloc(1);
    if (!output) {
        if (error) *error = strdup("内存分配失败");
        return NULL;
    }
    output[0] = '\0';
    
    // 添加YAML头
    if (json->type == cJSON_Object || json->type == cJSON_Array) {
        add_to_output(&output, "---\n");
    }
    
    if (cjson_to_yaml_recursive(json, &output, indent, 0, false, &error) != 1) {
        free(output);
        return NULL;
    }
    
    return output;
}

// 写入YAML文件
int cjson2yaml_file(cJSON *json, const char *filename, int indent, char **error) {
    char *yaml = cjson2yaml(json, indent, error);
    if (!yaml) return -1;
    
    FILE *file = fopen(filename, "w");
    if (!file) {
        if (error) *error = strdup("无法打开文件");
        free(yaml);
        return -1;
    }
    
    fputs(yaml, file);
    fclose(file);
    free(yaml);
    return 0;
}