#include "js_module.h"


#include "mquickjs.h"
#include "../../src/ytype.h"

#include "event.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Layer 结构的最小定义（只使用我们需要的字段）
#define MAX_TEXT 256
#define MAX_PATH 1024

// 全局 JS 上下文
static JSContext* g_js_ctx = NULL;
static uint8_t* g_js_mem = NULL;
static size_t g_js_mem_size = 256 * 1024; // 256KB 内存

// 全局 UI 根图层
struct Layer* g_layer_root = NULL;


// C 事件处理器类型
typedef void (*CEventHandler)(Layer* layer, const char* event_type);

// C 事件处理器注册表
#define MAX_C_EVENT_HANDLERS 128
typedef struct {
    char event_name[128];
    CEventHandler handler;
} CEventEntry;

static CEventEntry g_c_event_handlers[MAX_C_EVENT_HANDLERS];
static int g_c_event_handler_count = 0;

// JS 事件映射表（存储 JS 函数名）
#define MAX_JS_EVENTS 128
typedef struct {
    char event_name[128];
    char func_name[128];
} JSEventMapping;

static JSEventMapping g_js_event_map[MAX_JS_EVENTS];
static int g_js_event_count = 0;

static void check_timers(void);


extern const JSSTDLibraryDef js_yuistdlib;
  
// ====================== 辅助函数 ======================

int hex_to_int(char c){
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}

// ====================== JS API 函数 ======================

// 设置文本
static JSValue js_set_text(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    if (argc < 2) return JS_UNDEFINED;

    JSCStringBuf buf1, buf2;
    const char* layer_id = JS_ToCString(ctx, argv[0], &buf1);
    const char* text = JS_ToCString(ctx, argv[1], &buf2);

    if (layer_id && text && g_layer_root ) {
        struct Layer* layer = find_layer_by_id(g_layer_root, layer_id);
        if (layer) {
            strncpy(layer->text, text, MAX_TEXT - 1);
            layer->text[MAX_TEXT - 1] = '\0';
            printf("JS: Set text for layer '%s': %s\n", layer_id, text);
        }
    }

    return JS_UNDEFINED;
}

// 获取文本
static JSValue js_get_text(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    if (argc < 1) return JS_UNDEFINED;

    JSCStringBuf buf;
    const char* layer_id = JS_ToCString(ctx, argv[0], &buf);

    if (layer_id && g_layer_root ) {
        struct Layer* layer = find_layer_by_id(g_layer_root, layer_id);
        if (layer) {
            return JS_NewString(ctx, layer->text);
        }
    }

    return JS_UNDEFINED;
}

// 设置背景颜色
static JSValue js_set_bg_color(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    if (argc < 2) return JS_UNDEFINED;

    JSCStringBuf buf1, buf2;
    const char* layer_id = JS_ToCString(ctx, argv[0], &buf1);
    const char* color_hex = JS_ToCString(ctx, argv[1], &buf2);

    if (layer_id && color_hex && g_layer_root ) {
        struct Layer* layer = find_layer_by_id(g_layer_root, layer_id);
        if (layer) {
            // 解析十六进制颜色 #RRGGBB
            if (strlen(color_hex) == 7 && color_hex[0] == '#') {
                layer->bg_color.r = (hex_to_int(color_hex[1]) * 16 + hex_to_int(color_hex[2]));
                layer->bg_color.g = (hex_to_int(color_hex[3]) * 16 + hex_to_int(color_hex[4]));
                layer->bg_color.b = (hex_to_int(color_hex[5]) * 16 + hex_to_int(color_hex[6]));
                layer->bg_color.a = 255;
                printf("JS: Set bg_color for layer '%s': %s\n", layer_id, color_hex);
            }
        }
    }

    return JS_UNDEFINED;
}

// 隐藏图层
static JSValue js_hide(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    if (argc < 1) return JS_UNDEFINED;

    JSCStringBuf buf;
    const char* layer_id = JS_ToCString(ctx, argv[0], &buf);

    if (layer_id && g_layer_root ) {
        struct Layer* layer = find_layer_by_id(g_layer_root, layer_id);
        if (layer) {
            layer->visible = 0; // IN_VISIBLE
            printf("JS: Hide layer '%s'\n", layer_id);
        }
    }

    return JS_UNDEFINED;
}

// 显示图层
static JSValue js_show(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    if (argc < 1) return JS_UNDEFINED;

    JSCStringBuf buf;
    const char* layer_id = JS_ToCString(ctx, argv[0], &buf);

    if (layer_id && g_layer_root ) {
        struct Layer* layer = find_layer_by_id(g_layer_root, layer_id);
        if (layer) {
            layer->visible = 1; // VISIBLE
            printf("JS: Show layer '%s'\n", layer_id);
        }
    }

    return JS_UNDEFINED;
}

// 打印日志
static JSValue js_log(JSContext *ctx, JSValue *this_val, int argc, JSValue *argv)
{
    JSCStringBuf buf;
    for (int i = 0; i < argc; i++) {
        if (i != 0) printf(" ");
        const char* str = JS_ToCString(ctx, argv[i], &buf);
        if (str) {
            printf("%s", str);
        }
    }
    printf("\n");
    return JS_UNDEFINED;
}

// ====================== 初始化和清理 ======================

static void js_log_func(void *opaque, const void *buf, size_t buf_len)
{
    fwrite(buf, 1, buf_len, stdout);
}

static uint8_t* load_file(const char *filename, int *plen)
{
    FILE *f;
    uint8_t *buf;
    int buf_len;

    f = fopen(filename, "rb");
    if (!f) {
        printf("JS: Cannot open file %s\n", filename);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    buf_len = ftell(f);
    fseek(f, 0, SEEK_SET);
    buf = malloc(buf_len + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    fread(buf, 1, buf_len, f);
    buf[buf_len] = '\0';
    fclose(f);

    if (plen) *plen = buf_len;
    return buf;
}


// 初始化 JS 引擎
int js_module_init(void)
{
    printf("JS: Initializing JavaScript engine...\n");

    g_js_mem = malloc(g_js_mem_size);
    if (!g_js_mem) {
        fprintf(stderr, "JS: Failed to allocate memory\n");
        return -1;
    }

    g_js_ctx = JS_NewContext(g_js_mem, g_js_mem_size, &js_yuistdlib);
    if (!g_js_ctx) {
        fprintf(stderr, "JS: Failed to create context\n");
        free(g_js_mem);
        return -1;
    }

    JS_SetLogFunc(g_js_ctx, js_log_func);

    js_module_register_api();

    printf("JS: JavaScript engine initialized\n");
    return 0;
}

// 清理 JS 引擎
void js_module_cleanup(void)
{
    if (g_js_ctx) {
        JS_FreeContext(g_js_ctx);
        g_js_ctx = NULL;
    }
    if (g_js_mem) {
        free(g_js_mem);
        g_js_mem = NULL;
    }
}

// 注册 C API 到 JS
void js_module_register_api(void)
{
    if (!g_js_ctx) return;

    
}

// 加载并执行 JS 文件
int js_module_load_file(const char* filename)
{
    if (!g_js_ctx) {
        fprintf(stderr, "JS: Engine not initialized\n");
        return -1;
    }

    printf("JS: Loading file %s...\n", filename);

    int len = 0;
    uint8_t* buf = load_file(filename, &len);
    if (!buf) {
        return -1;
    }

    JSValue val = JS_Eval(g_js_ctx, (const char*)buf, len, filename, 0);
    free(buf);

    if (JS_IsException(val)) {
        JSValue exc = JS_GetException(g_js_ctx);
        fprintf(stderr, "JS: Error executing %s:\n", filename);
        JS_PrintValueF(g_js_ctx, exc, JS_DUMP_LONG);
        printf("\n");
        return -1;
    }

    printf("JS: Successfully loaded %s\n", filename);
    return 0;
}

// 设置 UI 根图层
void js_module_init_layer(Layer* root)
{
    g_layer_root = root;

}

// 辅助函数：获取文件所在的目录
static void get_file_dir(const char* filepath, char* dir, size_t max_len)
{
    if (!filepath || !dir) {
        dir[0] = '\0';
        return;
    }

    // 查找最后一个 '/' 或 '\' 的位置
    const char* last_sep = strrchr(filepath, '/');
    const char* last_sep_win = strrchr(filepath, '\\');

    // 使用最后的分隔符
    const char* sep = (last_sep_win > last_sep) ? last_sep_win : last_sep;

    if (sep) {
        size_t dir_len = sep - filepath;
        if (dir_len >= max_len) dir_len = max_len - 1;
        strncpy(dir, filepath, dir_len);
        dir[dir_len] = '\0';
    } else {
        dir[0] = '.';  // 当前目录
        dir[1] = '\0';
    }
}

// 辅助函数：构建完整的 JS 文件路径（相对于 JSON 文件目录）
static void build_js_path(const char* js_path, const char* json_dir, char* full_path, size_t max_len)
{
    if (js_path[0] == '/' || (js_path[0] == '.' && (js_path[1] == '/' || js_path[1] == '.'))) {
        // 绝对路径或 ./ 开头的路径，直接使用
        strncpy(full_path, js_path, max_len - 1);
    } else {
        // 相对路径，拼接 JSON 文件所在目录
        snprintf(full_path, max_len, "%s/%s", json_dir, js_path);
    }
    full_path[max_len - 1] = '\0';
}


int js_module_set_layer_event(Layer* layer, const char* event_name, const char* event_func_name, void* event_handler)
{
    if (!layer || !event_name) {
        return -1;
    }

    // 确保 layer->event 已分配
    if (layer->event == NULL) {
        layer->event = malloc(sizeof(Event));
        if (!layer->event) {
            printf("JS: Failed to allocate Event structure\n");
            return -1;
        }
        memset(layer->event, 0, sizeof(Event));
    }

    // 检查 click 事件
    if (strcmp(event_name, "click") == 0 || strcmp(event_name, "onClick") == 0) {
        if (event_func_name) {
            strncpy(layer->event->click_name, event_func_name, MAX_PATH - 1);
            layer->event->click_name[MAX_PATH - 1] = '\0';
        }
        layer->event->click = (EventHandler)event_handler;
        return 0;
    }
    // 检查 press 事件
    if (strcmp(event_name, "press") == 0 || strcmp(event_name, "onPress") == 0) {
        layer->event->press = (EventHandler)event_handler;
        return 0;
    }
    // 检查 scroll 事件
    if (strcmp(event_name, "scroll") == 0 || strcmp(event_name, "onScroll") == 0) {
        if (event_func_name) {
            strncpy(layer->event->scroll_name, event_func_name, MAX_PATH - 1);
            layer->event->scroll_name[MAX_PATH - 1] = '\0';
        }
        layer->event->scroll = (EventHandler)event_handler;
        return 0;
    }

    return -1;
}

// 通用的 JS 事件包装函数（用于 YUI 的 register_event_handler）
static void js_module_common_event(void* data)
{
    Layer* layer = (Layer*)data;
    if (layer && layer->event) {
        // 查找 click_name 对应的 JS 函数并调用
        if (layer->event->click_name[0] != '\0') {
            printf("JS: YUI click handler calling '%s'\n", layer->event->click_name);
            js_module_trigger_event(layer->event->click_name, layer);
        }
    }
}

// Click 事件包装函数
static void js_module_click_event(void* data)
{
    Layer* layer = (Layer*)data;
    if (layer) {
        printf("JS: Click event on layer '%s'\n", layer->id);
        js_module_call_layer_event(layer->id, "onClick");
    }
}

// Press 事件包装函数
static void js_module_press_event(void* data)
{
    Layer* layer = (Layer*)data;
    if (layer) {
        printf("JS: Press event on layer '%s'\n", layer->id);
        js_module_call_layer_event(layer->id, "onPress");
    }
}

// Scroll 事件包装函数
static void js_module_scroll_event(void* data)
{
    Layer* layer = (Layer*)data;
    if (layer) {
        printf("JS: Scroll event on layer '%s'\n", layer->id);
        js_module_call_layer_event(layer->id, "onScroll");
    }
}

// 根据事件类型返回对应的处理函数
static EventHandler get_event_handler_by_type(const char* event_type)
{
    if (!event_type) return NULL;

    // 支持多种事件类型格式
    if (strcmp(event_type, "click") == 0 || strcmp(event_type, "onClick") == 0) {
        return js_module_click_event;
    }
    if (strcmp(event_type, "press") == 0 || strcmp(event_type, "onPress") == 0) {
        return js_module_press_event;
    }
    if (strcmp(event_type, "scroll") == 0 || strcmp(event_type, "onScroll") == 0) {
        return js_module_scroll_event;
    }

    return NULL;
}


// 注册事件映射（存储 JS 函数名）
static void register_js_event_mapping(const char* event_name, const char* func_name)
{
    if (g_js_event_count >= MAX_JS_EVENTS) {
        printf("JS: JS event map full, cannot register event: %s\n", event_name);
        return;
    }

    // 移除 @ 前缀（如果有）
    const char* clean_func_name = func_name;
    if (func_name[0] == '@') {
        clean_func_name++;
    }

    strncpy(g_js_event_map[g_js_event_count].event_name, event_name, 127);
    g_js_event_map[g_js_event_count].event_name[127] = '\0';
    strncpy(g_js_event_map[g_js_event_count].func_name, clean_func_name, 127);
    g_js_event_map[g_js_event_count].func_name[127] = '\0';

    printf("JS: Registered JS event: '%s' -> '%s'\n", event_name, clean_func_name);



    // 支持event_name=id+event 这种格式 card1.onClick 这种格式的
    char* dot_pos = strstr(event_name, ".");
    if (dot_pos != NULL && g_layer_root ) {
        // 复制 id（在 . 之前的部分）
        char layer_id[128];
        int id_len = dot_pos - event_name;
        if (id_len > 127) id_len = 127;
        strncpy(layer_id, event_name, id_len);
        layer_id[id_len] = '\0';

        // event_type 在 . 之后的部分
        char* event_type = dot_pos + 1;

        // 根据 event_type 获取对应的事件处理函数
        EventHandler handler = get_event_handler_by_type(event_type);
        if (handler != NULL) {
            Layer * layer = find_layer_by_id(g_layer_root, layer_id);
            if (layer != NULL) {
                js_module_set_layer_event(layer, event_type, clean_func_name, handler);
            }
        } else {
            printf("JS: Warning: Unknown event type '%s' for layer '%s'\n", event_type, layer_id);
        }
    }else{
        EventHandler handler = get_event_handler_by_type(event_name);

        // 注册layer 回调用事件（使用通用事件处理器）
        register_event_handler(clean_func_name, js_module_common_event);
    }
    g_js_event_count++;
}

// 注册 C 事件处理器（内部函数）
static int register_c_event_handler_internal(const char* event_name, CEventHandler handler)
{
    if (g_c_event_handler_count >= MAX_C_EVENT_HANDLERS) {
        printf("JS: C event handler table full, cannot register: %s\n", event_name);
        return -1;
    }

    strncpy(g_c_event_handlers[g_c_event_handler_count].event_name, event_name, 127);
    g_c_event_handlers[g_c_event_handler_count].event_name[127] = '\0';
    g_c_event_handlers[g_c_event_handler_count].handler = handler;

    printf("JS: Registered C event handler: '%s'\n", event_name);
    g_c_event_handler_count++;
    return 0;
}

// 根据 layer id 和事件类型构建完整事件名称
static void build_event_name(const char* layer_id, const char* event_type, char* event_name, size_t max_len)
{
    snprintf(event_name, max_len, "%s.%s", layer_id, event_type);
}

// 扫描并注册事件（从 events 或 event 对象）
static void scan_and_register_events(cJSON* json)
{
    if (!json) return;

    // 检查 "events" 对象
    cJSON* events_obj = cJSON_GetObjectItem(json, "events");
    if (events_obj && cJSON_IsObject(events_obj)) {
        printf("JS: Found 'events' object, registering events...\n");
        cJSON* event = events_obj->child;
        while (event) {
            if (cJSON_IsString(event)) {
                // 注册为 JS 事件（存储函数名）
                register_js_event_mapping(event->string, event->valuestring);
            }
            event = event->next;
        }
    }

    // 检查 "event" 对象（单数形式）
    cJSON* event_obj = cJSON_GetObjectItem(json, "event");
    if (event_obj && cJSON_IsObject(event_obj)) {
        printf("JS: Found 'event' object, registering events...\n");
        cJSON* event = event_obj->child;
        while (event) {
            if (cJSON_IsString(event)) {
                // 注册为 JS 事件（存储函数名）
                register_js_event_mapping(event->string, event->valuestring);
            }
            event = event->next;
        }
    }
}

// 递归遍历 JSON 并加载所有 JS 文件
static int load_js_recursive(cJSON* json, const char* json_dir)
{
    if (!json) return 0;

    int loaded_count = 0;

    // 检查当前节点是否有 "js" 字段
    cJSON* js_file = cJSON_GetObjectItem(json, "js");
    if (js_file && cJSON_IsString(js_file)) {
        const char* js_path = js_file->valuestring;
        char full_path[MAX_PATH];
        build_js_path(js_path, json_dir, full_path, MAX_PATH);

        printf("JS: Loading JS file from config: %s -> %s\n", js_path, full_path);
        if (js_module_load_file(full_path) == 0) {
            loaded_count++;
        }
    }

    // 扫描并注册事件
    scan_and_register_events(json);

    // 递归遍历子对象
    cJSON* child = json->child;
    while (child) {
        // 递归处理子节点
        loaded_count += load_js_recursive(child, json_dir);
        child = child->next;
    }

    return loaded_count;
}

// 从 JSON 加载 JS 文件（递归遍历整个 JSON 树）
int js_module_load_from_json(cJSON* root_json, const char* json_file_path)
{
    if (!root_json) {
        printf("JS: root_json is NULL\n");
        return 0;
    }

    // 清空之前的事件映射表
    js_module_clear_events();

    // 获取 JSON 文件所在目录
    char json_dir[MAX_PATH];
    if (json_file_path && json_file_path[0] != '\0') {
        get_file_dir(json_file_path, json_dir, MAX_PATH);
    } else {
        // 默认目录：app/mquickjs/
        strcpy(json_dir, "app/mquickjs");
    }

    printf("JS: Loading JS from JSON directory: %s\n", json_dir);

    int total_loaded = load_js_recursive(root_json, json_dir);
    printf("JS: Total %d JS file(s) loaded from JSON\n", total_loaded);

    // 自动触发 onLoad 事件（如果有）
    if (total_loaded > 0) {
        printf("JS: Triggering 'onLoad' event...\n");
        js_module_trigger_event("onLoad", NULL);
    }

    return total_loaded;
}

// 调用 JS 事件函数
int js_module_call_event(const char* event_name, Layer* layer)
{
    if (!g_js_ctx || !event_name) return -1;

    // 移除 @ 前缀（如果有）
    const char* func_name = event_name;
    if (func_name[0] == '@') {
        func_name++;
    }

    JSValue global_obj = JS_GetGlobalObject(g_js_ctx);
    JSValue func = JS_GetPropertyStr(g_js_ctx, global_obj, func_name);

    if (JS_IsUndefined(func) || !JS_IsFunction(g_js_ctx, func)) {
        return -1;
    }

    // 调用函数
    if (JS_StackCheck(g_js_ctx, 3)) {
        return -1;
    }

    JSValue layer_id_val = JS_NULL;
    if (layer) {
        layer_id_val = JS_NewString(g_js_ctx, layer->id);
    }

    // 按照正确的顺序 push: 参数 -> 函数 -> this
    JS_PushArg(g_js_ctx, layer_id_val); // 参数
    JS_PushArg(g_js_ctx, func); // 函数对象
    JS_PushArg(g_js_ctx, JS_NULL); // this
    JSValue result = JS_Call(g_js_ctx, 1); // 1 个参数

    if (JS_IsException(result)) {
        JSValue exc = JS_GetException(g_js_ctx);
        fprintf(stderr, "JS: Error calling event %s:\n", event_name);
        JS_PrintValueF(g_js_ctx, exc, JS_DUMP_LONG);
        fprintf(stderr, "\n");
        return -1;
    }

    return 0;
}

// 触发事件（根据事件名称自动查找并调用对应的 JS 函数）
int js_module_trigger_event(const char* event_name, Layer* layer)
{
    if (!g_js_ctx || !event_name) {
        return -1;
    }

    // 在 JS 事件映射表中查找对应的函数名
    for (int i = 0; i < g_js_event_count; i++) {
        if (strcmp(g_js_event_map[i].event_name, event_name) == 0) {
            printf("JS: Triggering JS event '%s' -> calling function '%s'\n",
                   event_name, g_js_event_map[i].func_name);
            return js_module_call_event(g_js_event_map[i].func_name, layer);
        }
    }

    // 未找到事件映射
    printf("JS: JS event '%s' not registered, trying direct call...\n", event_name);
    return js_module_call_event(event_name, layer);
}

// 清空事件映射表
void js_module_clear_events(void)
{
    g_js_event_count = 0;
    g_c_event_handler_count = 0;
    printf("JS: Event maps cleared\n");
}

// 根据 layer id 调用事件处理器
// event_type: 事件类型，如 "onClick", "onLoad", "onPress" 等
int js_module_call_layer_event(const char* layer_id, const char* event_type)
{
    if (!layer_id || !event_type) {
        printf("JS: Invalid layer_id or event_type\n");
        return -1;
    }

    // 查找图层
    Layer* layer = NULL;
    if (g_layer_root ) {
        layer = find_layer_by_id(g_layer_root, layer_id);
    }

    if (!layer) {
        printf("JS: Layer '%s' not found\n", layer_id);
        return -1;
    }

    // 构建完整事件名称
    char full_event_name[128];
    build_event_name(layer_id, event_type, full_event_name, sizeof(full_event_name));

    printf("JS: Calling event '%s' on layer '%s'\n", full_event_name, layer_id);

    // 1. 首先尝试调用 C 事件处理器
    for (int i = 0; i < g_c_event_handler_count; i++) {
        if (strcmp(g_c_event_handlers[i].event_name, full_event_name) == 0) {
            printf("JS: Found C event handler for '%s'\n", full_event_name);
            if (g_c_event_handlers[i].handler) {
                g_c_event_handlers[i].handler(layer, event_type);
                return 0;
            }
        }
    }

    // 2. 如果没有 C 处理器，尝试调用 JS 函数
    // printf("JS: No C handler found, trying JS function for '%s'\n", full_event_name);
    return js_module_trigger_event(full_event_name, layer);
}

// 从 Layer 的事件结构中触发事件（兼容旧的 Event 结构）
// layer: 图层指针
// event_name: 事件类型，如 "click", "press", "scroll" 等
int js_module_trigger_layer_event(Layer* layer, const char* event_name)
{
    if (!layer || !event_name) {
        return -1;
    }

    // 1. 首先尝试调用 Layer 的事件结构（旧的 Event 结构）
    if (layer->event) {
        // 检查 click 事件
        if (strcmp(event_name, "click") == 0 && layer->event->click) {
            layer->event->click(layer);
            return 0;
        }
        // 检查 press 事件
        if (strcmp(event_name, "press") == 0 && layer->event->press) {
            layer->event->press(layer);
            return 0;
        }
        // 检查 scroll 事件
        if (strcmp(event_name, "scroll") == 0 && layer->event->scroll) {
            layer->event->scroll(layer);
            return 0;
        }
    }

    // 2. 调用新的事件系统（通过 layer id）
    if (layer->id[0] != '\0') {
        return js_module_call_layer_event(layer->id, event_name);
    }

    return -1;
}

// 注册 C 事件处理器（公共 API）
int js_module_register_c_event_handler(const char* event_name, CEventHandler handler)
{
    return register_c_event_handler_internal(event_name, handler);
}

// 检查并触发定时器（内部静态函数）
static void check_timers(void)
{
    if (!g_js_ctx) return;
}

// 更新图层文本
void js_module_update_layer_text(Layer* layer, const char* text)
{
    if (layer && text) {
        strncpy(layer->text, text, MAX_TEXT - 1);
        layer->text[MAX_TEXT - 1] = '\0';
    }
}

// 设置按钮样式
void js_module_set_button_style(Layer* layer, const char* bg_color)
{
    if (layer && bg_color) {
        // 解析十六进制颜色
        if (strlen(bg_color) == 7 && bg_color[0] == '#') {
            layer->bg_color.r = (hex_to_int(bg_color[1]) * 16 + hex_to_int(bg_color[2]));
            layer->bg_color.g = (hex_to_int(bg_color[3]) * 16 + hex_to_int(bg_color[4]));
            layer->bg_color.b = (hex_to_int(bg_color[5]) * 16 + hex_to_int(bg_color[6]));
            layer->bg_color.a = 255;
        }
    }
}
