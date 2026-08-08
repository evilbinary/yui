// yui_stdlib_stubs.c
// 为 yui-stdlib-host 工具提供的存根函数
// 这个工具只是用来生成头文件，不需要实际的 YUI 运行时功能

#include "../../src/layer.h"
#include "../../src/layer_lifecycle.h"
#include "../../src/theme_manager.h"
#include "../../src/game/game.h"
#include "../../lib/cjson/cJSON.h"
#include "mquickjs.h"
#include <stdlib.h>

// 全局变量存根
Layer* g_layer_root = NULL;

int yui_inspect_mode_enabled = 0;
int yui_inspect_show_bounds = 0;
int yui_inspect_show_info = 0;

// 图层操作函数存根
Layer* find_layer_by_id(Layer* root, const char* id) {
    printf("find_layer_by_id here=>>>>> not permission\n");
    return NULL;
}

Layer* parse_layer_from_string(const char* json_str, Layer* parent) {
    return NULL;
}

void destroy_layer(Layer* layer) {
}

void layout_layer(Layer* layer) {
}

void load_all_fonts(Layer* layer) {
}

void layer_set_text(Layer* layer, const char* text) {
}

const char* layer_get_text(const Layer* layer) {
    return "";
}

int layer_show(Layer* layer, int recursive) {
    return 0;
}

int layer_hide(Layer* layer) {
    return 0;
}

void layer_lifecycle_before_destroy(Layer* layer) {
}

// JSON 更新存根
int yui_update(Layer* root, const char* update_json) {
    return -1;
}

// 主题管理器存根函数
ThemeManager* theme_manager_get_instance(void) {
    return NULL;
}

Theme* theme_manager_load_theme(const char* theme_path) {
    return NULL;
}

Theme* theme_manager_load_theme_from_json(const char* json_str) {
    return NULL;
}

int theme_manager_set_current(const char* theme_name) {
    return 0;
}

void theme_manager_unload_theme(const char* theme_name) {
}

Theme* theme_manager_get_current(void) {
    return NULL;
}

void theme_manager_apply_to_tree(Layer* root) {
}

// js_module 桥接存根
const char* js_module_get_property_value(const char* layer_id, const char* property_name) {
    return NULL;
}

int js_module_load_from_json(cJSON* root_json, const char* json_file_path, int append) {
    return -1;
}

char* js_module_read_file(const char* file_path) {
    return NULL;
}

int js_module_resolve_path(const char* in, char* out, size_t out_sz) {
    (void)in;
    (void)out;
    (void)out_sz;
    return -1;
}

const char* js_module_get_root(void) {
    return "/spiffs";
}

int js_module_resize_root(int width, int height) {
    return -1;
}

int js_module_set_event(const char* layer_id, const char* event_name, const char* event_func_name) {
    return -1;
}

// mquickjs 无 JS_ToBool / JS_FreeCString / JS_NewNumber / JS_FreeValue /
// JS_IsObject。默认由本文件提供桩；若调用方已 include mqjs_shim.c
// （如 bc_gen.c），则定义 YUI_STDLIB_USE_SHIM 跳过，避免重复定义。
#ifndef YUI_STDLIB_USE_SHIM

int JS_ToBool(JSContext* ctx, JSValue val) {
    if (JS_IsBool(val)) {
        return JS_VALUE_GET_SPECIAL_VALUE(val) != 0;
    }
    if (JS_IsInt(val)) {
        return JS_VALUE_GET_INT(val) != 0;
    }
    return !JS_IsNull(val) && !JS_IsUndefined(val);
}

void JS_FreeCString(JSContext* ctx, JSCStringBuf* buf) {
    (void)ctx;
    (void)buf;
}

JSValue JS_NewNumber(JSContext* ctx, double d) {
    return JS_NewFloat64(ctx, d);
}

void JS_FreeValue(JSContext* ctx, JSValue v) {
    (void)ctx;
    (void)v;
}

int JS_IsObject(JSContext* ctx, JSValue v) {
    (void)ctx;
    (void)v;
    return 0;
}

#endif // YUI_STDLIB_USE_SHIM

// 属性 JSON 读取存根（生成头文件工具不需要运行时功能）
cJSON* layer_get_property_as_json(Layer* layer, const char* key) {
    return NULL;
}

void backend_get_windowsize(int* width, int* height) {
    if (width) *width = 800;
    if (height) *height = 480;
}

// screenshot 存根（生成头文件工具不需要运行时功能）
int backend_screenshot(const char* path) {
    (void)path;
    return -1;
}

// Game 存根：js_game.c 参与 yui-stdlib-host 生成 ROM 表，但工具不需要 game 运行时
void game_init(void) {
}
void game_shutdown(void) {
}
int game_load_scene_json(const char* path) {
    (void)path;
    return 0;
}
void game_clear_scene(void) {
}
GameEntity* game_spawn_from_json(cJSON* obj) {
    (void)obj;
    return NULL;
}
GameEntity* game_spawn(const char* id) {
    (void)id;
    return NULL;
}
void game_destroy(GameEntity* e) {
    (void)e;
}
void game_destroy_by_id(const char* id) {
    (void)id;
}
GameEntity* game_find(const char* id) {
    (void)id;
    return NULL;
}
GameEntity* game_find_by_tag(const char* tag) {
    (void)tag;
    return NULL;
}
int game_find_all_by_tag(const char* tag, GameEntity** out, int max_out) {
    (void)tag;
    (void)out;
    (void)max_out;
    return 0;
}
GameEntity* game_pool_acquire(const char* prefab) {
    (void)prefab;
    return NULL;
}
void game_pool_release(GameEntity* e) {
    (void)e;
}
float game_time_dt(void) {
    return 0;
}
int game_input_down(const char* name) {
    (void)name;
    return 0;
}
int game_input_pressed(const char* name) {
    (void)name;
    return 0;
}
float game_input_axis(const char* name) {
    (void)name;
    return 0;
}
void game_input_pointer(int* x, int* y) {
    if (x) *x = 0;
    if (y) *y = 0;
}
int game_input_mouse_down(int button) {
    (void)button;
    return 0;
}
int game_input_mouse_pressed(int button) {
    (void)button;
    return 0;
}
GameCamera* game_camera(void) {
    return NULL;
}
void game_camera_set(float x, float y) {
    (void)x; (void)y;
}
void game_camera_follow(const char* id) {
    (void)id;
}
int game_entities_overlap(const GameEntity* a, const GameEntity* b) {
    (void)a; (void)b;
    return 0;
}
void game_set_trigger_fn(GameTriggerFn fn) {
    (void)fn;
}
void game_set_script_update_fn(GameScriptUpdateFn fn) {
    (void)fn;
}
int game_play_anim(GameEntity* e, const char* clip) {
    (void)e; (void)clip;
    return 0;
}
int game_audio_play_sfx(const char* path) {
    (void)path;
    return 0;
}
int game_audio_play_bgm(const char* path, int loop) {
    (void)path; (void)loop;
    return 0;
}
void game_audio_stop_bgm(void) {
}
int game_spawn_particles(float x, float y, int count, Color color, float speed, float life) {
    (void)x; (void)y; (void)count; (void)color; (void)speed; (void)life;
    return 0;
}
void game_debug_set_boxes(int enabled) {
    (void)enabled;
}
int game_debug_boxes_enabled(void) {
    return 0;
}
const GamePerfStats* game_perf_get_stats(void) {
    return NULL;
}
int game_scene_generation(void) {
    return 0;
}
