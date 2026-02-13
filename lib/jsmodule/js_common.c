#include "js_module.h"
#include "../../src/ytype.h"

#include "event.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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


// ====================== 辅助函数 ======================

int hex_to_int(char c){
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}


// 更新图层文本
void js_module_update_layer_text(Layer* layer, const char* text)
{
    if (layer && text) {
        layer_set_text(layer, text);
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

int js_module_set_layer_event(Layer* layer, const char* event_name, const char* event_func_name, EventHandler event_handler)
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
    // 检查 change 事件
    if (strcmp(event_name, "change") == 0 || strcmp(event_name, "onChange") == 0) {
        if(layer->register_event!=NULL){
            ((register_event_fun_t )layer->register_event)(layer, event_name, event_func_name, (EventHandler)event_handler);
        }
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

// Change 事件包装函数
static void js_module_change_event(void* data)
{
    Layer* layer = (Layer*)data;
    if (layer) {
        printf("JS: Change event on layer '%s'\n", layer->id);
        js_module_call_layer_event(layer->id, "onChange");
    }
}

// 根据事件类型返回对应的处理函数
static EventHandler get_event_handler_by_type(const char* event_type)
{
    if (!event_type) return NULL;

    // 支持多种事件类型格式
    if (strcmp(event_type, "click") == 0 || strcmp(event_type, "onClick") == 0) {
        return js_module_click_event;
    }else if (strcmp(event_type, "press") == 0 || strcmp(event_type, "onPress") == 0) {
        return js_module_press_event;
    }else if (strcmp(event_type, "scroll") == 0 || strcmp(event_type, "onScroll") == 0) {
        return js_module_scroll_event;
    } else if (strcmp(event_type, "change") == 0 || strcmp(event_type, "onChange") == 0) {
        return js_module_change_event;
    }
    printf("JS: Unknown event type: %s\n", event_type);

    return NULL;
}

// 辅助函数：构建完整的 JS 文件路径（相对于 JSON 文件目录）
static void build_js_path(const char* js_path, const char* json_dir, char* full_path, size_t max_len)
{
    if (js_path[0] == '/') {
        // 绝对路径，直接使用
        strncpy(full_path, js_path, max_len - 1);
    } else if (js_path[0] == '.' && js_path[1] == '/') {
        // ./ 开头的路径，替换为当前目录
        snprintf(full_path, max_len, "%s%s", json_dir, js_path + 1);
    } else if (js_path[0] == '.' && js_path[1] == '.') {
        // ../ 开头的路径，需要处理相对路径
        char temp_dir[MAX_PATH];
        strncpy(temp_dir, json_dir, MAX_PATH - 1);
        temp_dir[MAX_PATH - 1] = '\0';
        
        const char* path_ptr = js_path;
        
        // 处理每个 ../ 
        while (path_ptr[0] == '.' && path_ptr[1] == '.' && (path_ptr[2] == '/' || path_ptr[2] == '\\')) {
            // 从目录中移除最后一部分
            char* last_sep = strrchr(temp_dir, '/');
            char* last_sep_win = strrchr(temp_dir, '\\');
            
            // 使用最后的分隔符
            char* sep = (last_sep_win > last_sep) ? last_sep_win : last_sep;
            
            if (sep) {
                *sep = '\0';  // 截断到最后一个分隔符之前
            } else {
                // 如果没有更多目录，使用根目录
                strcpy(temp_dir, ".");
                break;
            }
            
            // 移动到路径的下一部分
            path_ptr += 3;  // 跳过 "../"
        }
        
        // 添加剩余的路径部分
        if (path_ptr[0] == '/' || path_ptr[0] == '\\') {
            path_ptr++;  // 跳过开头的分隔符
        }
        
        // 构建最终路径
        if (strlen(temp_dir) > 0 && strcmp(temp_dir, ".") != 0) {
            snprintf(full_path, max_len, "%s/%s", temp_dir, path_ptr);
        } else {
            strncpy(full_path, path_ptr, max_len - 1);
        }
    } else {
        // 普通相对路径，拼接 JSON 文件所在目录
        snprintf(full_path, max_len, "%s/%s", json_dir, js_path);
    }
    full_path[max_len - 1] = '\0';
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

uint8_t* load_file(const char *filename, int *plen)
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

    // 获取当前 layer 的 ID
    const char* layer_id = NULL;
    cJSON* id_obj = cJSON_GetObjectItem(json, "id");
    if (id_obj && cJSON_IsString(id_obj)) {
        layer_id = id_obj->valuestring;
    }

    // 检查 "events" 对象
    cJSON* events_obj = cJSON_GetObjectItem(json, "events");
    if (events_obj && cJSON_IsObject(events_obj)) {
        printf("JS: Found 'events' object, registering events...\n");
        cJSON* event = events_obj->child;
        while (event) {
            if (cJSON_IsString(event)) {
                // 构建完整的事件名称：layerId.eventName
                char full_event_name[256];
                
                // 全局事件（如 onLoad）不添加层ID前缀
                // 如果事件名已经包含点号（如 "newGameBtn.onClick"），说明它已经指定了目标层ID
                // 这两种情况都直接使用事件名，不添加前缀
                if (strchr(event->string, '.') != NULL || 
                    strcmp(event->string, "onLoad") == 0) {
                    strncpy(full_event_name, event->string, sizeof(full_event_name) - 1);
                    full_event_name[sizeof(full_event_name) - 1] = '\0';
                } else if (layer_id && layer_id[0] != '\0') {
                    snprintf(full_event_name, sizeof(full_event_name), "%s.%s", layer_id, event->string);
                } else {
                    strncpy(full_event_name, event->string, sizeof(full_event_name) - 1);
                    full_event_name[sizeof(full_event_name) - 1] = '\0';
                }
                // 注册为 JS 事件（存储函数名）
                register_js_event_mapping(full_event_name, event->valuestring);
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
                // 构建完整的事件名称：layerId.eventName
                char full_event_name[256];
                
                // 全局事件（如 onLoad）不添加层ID前缀
                // 如果事件名已经包含点号（如 "newGameBtn.onClick"），说明它已经指定了目标层ID
                // 这两种情况都直接使用事件名，不添加前缀
                if (strchr(event->string, '.') != NULL || 
                    strcmp(event->string, "onLoad") == 0) {
                    strncpy(full_event_name, event->string, sizeof(full_event_name) - 1);
                    full_event_name[sizeof(full_event_name) - 1] = '\0';
                } else if (layer_id && layer_id[0] != '\0') {
                    snprintf(full_event_name, sizeof(full_event_name), "%s.%s", layer_id, event->string);
                } else {
                    strncpy(full_event_name, event->string, sizeof(full_event_name) - 1);
                    full_event_name[sizeof(full_event_name) - 1] = '\0';
                }
                // 注册为 JS 事件（存储函数名）
                register_js_event_mapping(full_event_name, event->valuestring);
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
    if (js_file) {
        // 支持字符串格式（单个JS文件）
        if (cJSON_IsString(js_file)) {
            const char* js_path = js_file->valuestring;
            char full_path[MAX_PATH];
            build_js_path(js_path, json_dir, full_path, MAX_PATH);

            printf("JS: Loading JS file from config: %s -> %s\n", js_path, full_path);
            if (js_module_load_file(full_path) == 0) {
                loaded_count++;
            }
        }
        // 支持数组格式（多个JS文件）
        else if (cJSON_IsArray(js_file)) {
            int array_size = cJSON_GetArraySize(js_file);
            for (int i = 0; i < array_size; i++) {
                cJSON* js_item = cJSON_GetArrayItem(js_file, i);
                if (js_item && cJSON_IsString(js_item)) {
                    const char* js_path = js_item->valuestring;
                    char full_path[MAX_PATH];
                    build_js_path(js_path, json_dir, full_path, MAX_PATH);

                    printf("JS: Loading JS files from config[%d]: %s -> %s\n", i, js_path, full_path);
                    if (js_module_load_file(full_path) == 0) {
                        loaded_count++;
                    }
                }
            }
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

// 清空事件映射表
void js_module_clear_events(void)
{
    g_js_event_count = 0;
    g_c_event_handler_count = 0;
    printf("JS: Event maps cleared\n");
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

// 触发事件（根据事件名称自动查找并调用对应的 JS 函数）
int js_module_trigger_event(const char* event_name, Layer* layer)
{
    if (!event_name) {
        return -1;
    }

    printf("JS: Searching for event '%s' in %d registered events\n", event_name, g_js_event_count);

    // 在 JS 事件映射表中查找对应的函数名
    for (int i = 0; i < g_js_event_count; i++) {
        printf("JS:   [%d] '%s' -> '%s' (comparing with '%s')\n", 
               i, g_js_event_map[i].event_name, g_js_event_map[i].func_name, event_name);
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

// YUI.setEvent() 的公共实现
// layer_id: 图层ID
// event_name: 事件名称（如 "click", "onClick"）
// event_func_name: JS 函数名
int js_module_set_event(const char* layer_id, const char* event_name, const char* event_func_name)
{
    if (!layer_id || !event_name || !event_func_name || !g_layer_root) {
        printf("JS: set_event invalid parameters\n");
        return -1;
    }

    // 查找图层
    Layer* layer = find_layer_by_id(g_layer_root, layer_id);
    if (!layer) {
        printf("JS: Layer '%s' not found\n", layer_id);
        return -1;
    }

    // 获取事件处理器
    EventHandler handler = get_event_handler_by_type(event_name);
    if (!handler) {
        printf("JS: Unknown event type: %s\n", event_name);
        return -1;
    }

    // 设置图层事件
    int result = js_module_set_layer_event(layer, event_name, event_func_name, (void*)handler);
    if (result == 0) {
        printf("JS: Set event '%s' for layer '%s' -> '%s'\n", event_name, layer_id, event_func_name);
        
        // 构建完整事件名称（如 "toggle_bounds.onClick"）
        char full_event_name[128];
        char event_type_buf[128];
        const char* event_type = event_name;
        
        // 如果 event_name 不是以 "on" 开头，则自动添加 "on" 前缀并首字母大写
        if (strncmp(event_name, "on", 2) != 0) {
            snprintf(event_type_buf, sizeof(event_type_buf), "on%c%s", 
                     toupper(event_name[0]), event_name + 1);
            event_type = event_type_buf;
        }
        
        build_event_name(layer_id, event_type, full_event_name, sizeof(full_event_name));
        
        // 注册事件映射到全局事件表
        register_js_event_mapping(full_event_name, event_func_name);
    }

    return result;
}

// 获取图层的属性值（通用）
const char* js_module_get_property_value(const char* layer_id, const char* property_name) {
    if (!layer_id || !property_name || !g_layer_root) {
        return NULL;
    }
    
    // 查找图层
    Layer* layer = find_layer_by_id(g_layer_root, layer_id);
    if (!layer) {
        printf("JS: Layer '%s' not found\n", layer_id);
        return NULL;
    }
    
    // 调用 layer_get_property_as_json 获取属性值
    extern cJSON* layer_get_property_as_json(Layer* layer, const char* key);
    cJSON* json_value = layer_get_property_as_json(layer, property_name);
    
    if (!json_value) {
        printf("JS: Property '%s' not found for layer '%s'\n", property_name, layer_id);
        return NULL;
    }
    
    // 将 JSON 值转换为字符串
    char* result = NULL;
    if (cJSON_IsString(json_value)) {
        // 如果是字符串，直接返回
        result = strdup(json_value->valuestring);
    }
    else if (cJSON_IsNumber(json_value)) {
        // 如果是数字，转换为字符串
        char num_str[64];
        snprintf(num_str, sizeof(num_str), "%.16g", json_value->valuedouble);
        result = strdup(num_str);
    }
    else if (cJSON_IsBool(json_value)) {
        // 如果是布尔值，转换为字符串
        result = strdup(cJSON_IsTrue(json_value) ? "true" : "false");
    }
    else if (cJSON_IsNull(json_value)) {
        // 如果是 null，返回空字符串
        result = strdup("");
    }
    else if (cJSON_IsArray(json_value)) {
        // 如果是数组，转换为 JSON 字符串
        result = cJSON_PrintUnformatted(json_value);
    }
    else if (cJSON_IsObject(json_value)) {
        // 如果是对象，转换为 JSON 字符串
        result = cJSON_PrintUnformatted(json_value);
    }
    
    cJSON_Delete(json_value);
    
    if (!result) {
        printf("JS: Failed to convert property '%s' to string for layer '%s'\n", property_name, layer_id);
        return NULL;
    }
    
    printf("JS: Got property '%s' value '%s' for layer '%s'\n", property_name, result, layer_id);
    return result;
}

// 获取 Select 组件的值（向后兼容）
const char* js_module_get_select_value(const char* layer_id) {
    return js_module_get_property_value(layer_id, "value");
}

// ====================== 文件读取功能 ======================

// 文件读取函数，用于JavaScript环境
char* js_module_read_file(const char* file_path) {
    if (!file_path) return NULL;
    
    FILE *f = fopen(file_path, "r");
    if (!f) {
        printf("JS: Cannot open file %s\n", file_path);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buffer = malloc(file_size + 1);
    if (!buffer) {
        fclose(f);
        return NULL;
    }

    size_t bytes_read = fread(buffer, 1, file_size, f);
    buffer[bytes_read] = '\0';
    fclose(f);

    printf("JS: Successfully read %ld bytes from file %s\n", bytes_read, file_path);
    return buffer;
}