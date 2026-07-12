#include "log.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static YuiLogLevel g_log_level = YUI_LOG_INFO;
static int g_log_initialized = 0;

static const char* level_name(YuiLogLevel level) {
    switch (level) {
        case YUI_LOG_ERROR: return "ERROR";
        case YUI_LOG_WARN: return "WARN";
        case YUI_LOG_INFO: return "INFO";
        case YUI_LOG_DEBUG: return "DEBUG";
        case YUI_LOG_TRACE: return "TRACE";
        default: return "LOG";
    }
}

static void init_log_level(void) {
    if (g_log_initialized) return;
    g_log_initialized = 1;

    const char* env = getenv("YUI_LOG_LEVEL");
    if (!env || env[0] == '\0') return;

    if (strcmp(env, "ERROR") == 0 || strcmp(env, "error") == 0) {
        g_log_level = YUI_LOG_ERROR;
    } else if (strcmp(env, "WARN") == 0 || strcmp(env, "warn") == 0) {
        g_log_level = YUI_LOG_WARN;
    } else if (strcmp(env, "INFO") == 0 || strcmp(env, "info") == 0) {
        g_log_level = YUI_LOG_INFO;
    } else if (strcmp(env, "DEBUG") == 0 || strcmp(env, "debug") == 0) {
        g_log_level = YUI_LOG_DEBUG;
    } else if (strcmp(env, "TRACE") == 0 || strcmp(env, "trace") == 0) {
        g_log_level = YUI_LOG_TRACE;
    }
}

void yui_log_set_level(YuiLogLevel level) {
    g_log_initialized = 1;
    g_log_level = level;
}

YuiLogLevel yui_log_get_level(void) {
    init_log_level();
    return g_log_level;
}

void yui_log(YuiLogLevel level, const char* tag, const char* fmt, ...) {
    init_log_level();
    if (level > g_log_level) return;

    fprintf(level <= YUI_LOG_WARN ? stderr : stdout, "[%s]", level_name(level));
    if (tag && tag[0] != '\0') {
        fprintf(level <= YUI_LOG_WARN ? stderr : stdout, "[%s] ", tag);
    } else {
        fprintf(level <= YUI_LOG_WARN ? stderr : stdout, " ");
    }

    va_list args;
    va_start(args, fmt);
    vfprintf(level <= YUI_LOG_WARN ? stderr : stdout, fmt, args);
    va_end(args);
    fprintf(level <= YUI_LOG_WARN ? stderr : stdout, "\n");
}
