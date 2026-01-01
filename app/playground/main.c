#include <stdio.h>


#include <stdbool.h>
#include <limits.h>

#include "cJSON.h"
#include "event.h"
#include "layer.h"
#include "render.h"
#include "layout.h"
#include "backend.h"
#include "popup_manager.h"

#if defined(_WIN32)
#include <windows.h>
int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    int argc = __argc;
    char** argv = __argv;
    return main(argc, argv);
}
#endif

// ====================== 事件回调示例 ======================

// 基础点击事件回调
void hello_world(Layer* layer) {
    if (layer && strlen(layer->text) > 0) {
        printf("你好，世界！ %.254s\n", layer->text);
    } else {
        printf("你好，世界！ (无文本内容)\n");
    }
}

// 触摸事件回调
void hello_touch(Layer* layer) {
    if (layer && strlen(layer->text) > 0) {
        printf("touch %.254s\n", layer->text);
    } else {
        printf("touch (无文本内容)\n");
    }
}

// 自定义事件回调 - 支持参数传递
void custom_callback_with_data(void* data) {
    if (data) {
        Layer* layer = (Layer*)data;
        // 确保字符串有足够的空间并添加安全检查
        const char* id_str = strlen(layer->id) > 0 ? layer->id : "unknown";
        
        printf("自定义回调触发 - Layer ID: %.49s\n", id_str);
    }
}

// 滚动事件回调示例
void on_scroll_callback(void* data) {
    if (data) {
        Layer* layer = (Layer*)data;
        const char* id_str = strlen(layer->id) > 0 ? layer->id : "unknown";
        printf("滚动事件 - Layer: %.49s, 滚动偏移: %d\n", 
               id_str, 
               layer ? layer->scroll_offset : 0);
    }
}

// 按钮点击回调示例
void on_button_click(void* data) {
    if (data) {
        Layer* layer = (Layer*)data;
        const char* text_str = strlen(layer->text) > 0 ? layer->text : "Unknown Button";
        printf("按钮点击 - %.254s\n", text_str);
    }
}

// 输入框变化回调示例
void on_input_change(void* data) {
    if (data) {
        Layer* layer = (Layer*)data;
        const char* id_str = strlen(layer->id) > 0 ? layer->id : "unknown";
        const char* text_str = strlen(layer->text) > 0 ? layer->text : "";
        printf("输入变化 - Layer: %.49s, 新内容: %.254s\n", id_str, text_str);
    }
}

// 焦点变化回调示例
void on_focus_change(void* data) {
    if (data) {
        Layer* layer = (Layer*)data;
        const char* id_str = strlen(layer->id) > 0 ? layer->id : "unknown";
        const char* state_str = (layer->state & LAYER_STATE_FOCUSED) ? "获得焦点" : "失去焦点";
        printf("焦点变化 - Layer: %.49s, 状态: %.49s\n", id_str, state_str);
    }
}

// ====================== 事件回调管理器 ======================

// 定义通用事件回调函数类型
typedef void (*GenericEventCallback)(void* data);

// 事件回调数据结构，支持传递额外参数
typedef struct {
    char* event_name;
    GenericEventCallback callback;
    void* user_data;
    int auto_cleanup;  // 是否自动清理user_data
} EventCallback;

// 简单的回调管理器
static EventCallback* g_callbacks = NULL;
static int g_callback_count = 0;
static int g_callback_capacity = 0;

// 注册带用户数据的回调
int register_event_callback_with_data(const char* event_name, EventHandler callback, void* user_data, int auto_cleanup) {
    if (!event_name || !callback) {
        return -1;
    }
    
    // 扩展数组容量
    if (g_callback_count >= g_callback_capacity) {
        g_callback_capacity = g_callback_capacity == 0 ? 16 : g_callback_capacity * 2;
        g_callbacks = realloc(g_callbacks, g_callback_capacity * sizeof(EventCallback));
        if (!g_callbacks) {
            return -1;
        }
    }
    
    // 添加新回调
    EventCallback* cb = &g_callbacks[g_callback_count];
    cb->event_name = strdup(event_name);
    cb->callback = callback;
    cb->user_data = user_data;
    cb->auto_cleanup = auto_cleanup;
    
    g_callback_count++;
    return g_callback_count - 1;
}

// 根据事件名称触发所有相关回调
void trigger_event_callbacks(const char* event_name, void* trigger_data) {
    if (!event_name) return;
    
    for (int i = 0; i < g_callback_count; i++) {
        EventCallback* cb = &g_callbacks[i];
        if (cb->event_name && strcmp(cb->event_name, event_name) == 0) {
            // 优先使用用户数据，如果没有则使用触发数据
            void* data = cb->user_data ? cb->user_data : trigger_data;
            cb->callback(data);
        }
    }
}

// 清理所有回调
void cleanup_event_callbacks() {
    for (int i = 0; i < g_callback_count; i++) {
        EventCallback* cb = &g_callbacks[i];
        if (cb->event_name) {
            free(cb->event_name);
        }
        if (cb->user_data && cb->auto_cleanup) {
            free(cb->user_data);
        }
    }
    free(g_callbacks);
    g_callbacks = NULL;
    g_callback_count = 0;
    g_callback_capacity = 0;
}

// ====================== 主入口 ======================
int main(int argc, char* argv[]) {

    backend_init();
    popup_manager_init();
    
    
    char* json_path="app/playground/app.json";
    // 加载UI描述文件
    if(argc>1){
        json_path=argv[1];
    }
    
    printf("DEBUG: Loading JSON from path: %s\n", json_path);

    // ====================== 事件注册 ======================
    // 基础事件注册
    register_event_handler("@hello", hello_world);
    register_event_handler("@helloTouch", hello_touch);
    
    // 扩展事件注册 - 支持更多事件类型
    register_event_handler("@customCallback", custom_callback_with_data);
    register_event_handler("@onScroll", on_scroll_callback);
    register_event_handler("@onButtonClick", on_button_click);
    register_event_handler("@onInputChange", on_input_change);
    register_event_handler("@onFocusChange", on_focus_change);
    
    // 打印所有已注册的事件（调试用）
    printf("=== 已注册的事件回调 ===\n");
    // 注意：这里需要在event.c中实现print_registered_events函数
    printf("基础事件: @hello, @helloTouch\n");
    printf("扩展事件: @customCallback, @onScroll, @onButtonClick, @onInputChange, @onFocusChange\n");
    printf("=======================\n\n");
    
    // ====================== 动态回调注册示例 ======================
    // 这里可以演示如何根据配置动态注册事件
    // 例如：从JSON配置中读取事件映射并注册
    
    // 示例：模拟从配置中读取的事件注册
    printf("=== 动态事件注册示例 ===\n");
    
    // 可以在这里添加根据JSON配置动态注册事件的逻辑
    // 例如：
    // cJSON* events_config = cJSON_GetObjectItem(root_json, "events");
    // if (events_config) {
    //     cJSON* event = NULL;
    //     cJSON_ArrayForEach(event, events_config) {
    //         char* event_name = cJSON_GetStringValue(cJSON_GetObjectItem(event, "name"));
    //         char* callback_name = cJSON_GetStringValue(cJSON_GetObjectItem(event, "callback"));
    //         // 根据callback名称找到对应的函数并注册
    //     }
    // }
    
    printf("动态事件注册功能可根据JSON配置扩展\n");
    printf("========================\n\n");
    
    // ====================== 高级回调管理器使用示例 ======================
    printf("=== 高级回调管理器示例 ===\n");
    
    // 注册带用户数据的回调
    char* custom_data = strdup("这是自定义用户数据");
    register_event_callback_with_data("@advancedCallback", custom_callback_with_data, custom_data, 1);
    
    // 注册按钮点击回调
    register_event_callback_with_data("@buttonAction", on_button_click, NULL, 0);
    
    // 注册输入变化回调
    register_event_callback_with_data("@textChanged", on_input_change, NULL, 0);
    
    // 模拟触发事件（在实际应用中，这些会由具体的用户交互触发）
    printf("模拟触发事件:\n");
    
    // 创建一个模拟Layer用于测试（分配在栈上，确保内存有效）
    static Layer test_layer;  // 使用static确保内存持久化
    memset(&test_layer, 0, sizeof(Layer));  // 清零初始化
    // 安全地初始化字符串字段
    strncpy(test_layer.id, "testLayer", sizeof(test_layer.id) - 1);
    test_layer.id[sizeof(test_layer.id) - 1] = '\0';
    strncpy(test_layer.text, "测试图层", sizeof(test_layer.text) - 1);
    test_layer.text[sizeof(test_layer.text) - 1] = '\0';
    test_layer.state = LAYER_STATE_NORMAL;
    test_layer.scroll_offset = 100;  // 设置一个测试值
    
    // 触发自定义回调
    trigger_event_callbacks("@advancedCallback", &test_layer);
    
    // 触发按钮点击
    trigger_event_callbacks("@buttonAction", &test_layer);
    
    printf("高级回调管理器已注册 %d 个回调\n", g_callback_count);
    printf("=======================\n\n");

    cJSON* root_json=parse_json(json_path);
    Layer* ui_root = parse_layer(root_json,NULL);

    // 初始化字体缓存（backend已经初始化）
    // 在parse_layer后加载所有字体
    load_all_fonts(ui_root);

    // 如果根图层没有设置宽度和高度，则根据窗口大小设置
    if (ui_root->rect.w <= 0 || ui_root->rect.h <= 0) {
        int window_width, window_height;
        backend_get_windowsize(&window_width, &window_height);
        if (ui_root->rect.w <= 0) {
            ui_root->rect.w = window_width;
        }
        if (ui_root->rect.h <= 0) {
            ui_root->rect.h = window_height;
        }
    }else{
        backend_set_windowsize(ui_root->rect.w,ui_root->rect.h);
    }

    // 如果根图层没有设置宽度和高度，则根据窗口大小设置
    printf("ui_root %d,%d\n",ui_root->rect.w,ui_root->rect.h);
    
    
    cJSON_Delete(root_json);
    
    if(ui_root->font==NULL){
        ui_root->font=malloc(sizeof(Font));
        sprintf(ui_root->font->path,"%s","Roboto-Regular.ttf");
    }
    if(ui_root->assets==NULL){
        ui_root->assets=malloc(sizeof(Assets));
        sprintf(ui_root->assets->path,"%s","assets");
    }

    // 加载纹理资源
    // load_all_fonts 已经在 parse_layer 之后调用过了

    load_textures(ui_root);
    layout_layer(ui_root);
    
    backend_run(ui_root);  
    
    // 清理资源
    cleanup_event_callbacks();  // 清理高级回调管理器
    // destroy_layer(ui_root);  // 暂时注释掉以避免内存问题
    popup_manager_cleanup();
    backend_quit();
    return 0;
}