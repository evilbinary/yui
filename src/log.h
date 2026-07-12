#ifndef YUI_LOG_H
#define YUI_LOG_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    YUI_LOG_ERROR = 0,
    YUI_LOG_WARN,
    YUI_LOG_INFO,
    YUI_LOG_DEBUG,
    YUI_LOG_TRACE
} YuiLogLevel;

void yui_log_set_level(YuiLogLevel level);
YuiLogLevel yui_log_get_level(void);
void yui_log(YuiLogLevel level, const char* tag, const char* fmt, ...);

#define LOGE(tag, fmt, ...) yui_log(YUI_LOG_ERROR, tag, fmt, ##__VA_ARGS__)
#define LOGW(tag, fmt, ...) yui_log(YUI_LOG_WARN,  tag, fmt, ##__VA_ARGS__)
#define LOGI(tag, fmt, ...) yui_log(YUI_LOG_INFO,  tag, fmt, ##__VA_ARGS__)
#define LOGD(tag, fmt, ...) yui_log(YUI_LOG_DEBUG, tag, fmt, ##__VA_ARGS__)
#define LOGT(tag, fmt, ...) yui_log(YUI_LOG_TRACE, tag, fmt, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif
