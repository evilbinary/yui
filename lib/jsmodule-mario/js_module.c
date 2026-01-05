#include "lib/jsmodule/js_module.h"
#include "../../lib/mario/mario.h"
#include "../../src/ytype.h"
#include "../../src/layer.h"
#include "../../src/layout.h"
#include "../../src/render.h"
#include "event.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>


extern uint8_t* load_file(const char *filename, int *plen);
extern int hex_to_int(char c);

/* ====================== 全局变量 ====================== */

static vm_t* g_vm = NULL;
extern struct Layer* g_layer_root;


/* ====================== Mario 原生函数 ====================== */

// Mario 原生函数包装器类型
typedef var_t* (*mario_native_func_t)(vm_t* vm, var_t* env, void* data);

// 设置文本
static var_t* mario_set_text(vm_t* vm, var_t* env, void* data)
{
    var_t* args = get_func_args(env);
    uint32_t argc = get_func_args_num(env);

    if (argc < 2) return var_new_null(vm);

    const char* layer_id = get_func_arg_str(env, 0);
    const char* text = get_func_arg_str(env, 1);

    if (layer_id && text && g_layer_root) {
        struct Layer* layer = find_layer_by_id(g_layer_root, layer_id);
        if (layer) {
            layer_set_text(layer, text);
            printf("JS(Mario): Set text for layer '%s': %s\n", layer_id, text);
        }
    }

    return var_new_null(vm);
}

// 获取文本
static var_t* mario_get_text(vm_t* vm, var_t* env, void* data)
{
    var_t* args = get_func_args(env);
    uint32_t argc = get_func_args_num(env);

    if (argc < 1) return var_new_null(vm);

    const char* layer_id = get_func_arg_str(env, 0);

    if (layer_id && g_layer_root) {
        struct Layer* layer = find_layer_by_id(g_layer_root, layer_id);
        if (layer) {
            return var_new_str(vm, layer->text);
        }
    }

    return var_new_null(vm);
}

// 设置背景颜色
static var_t* mario_set_bg_color(vm_t* vm, var_t* env, void* data)
{
    var_t* args = get_func_args(env);
    uint32_t argc = get_func_args_num(env);

    if (argc < 2) return var_new_null(vm);

    const char* layer_id = get_func_arg_str(env, 0);
    const char* color_hex = get_func_arg_str(env, 1);

    if (layer_id && color_hex && g_layer_root) {
        struct Layer* layer = find_layer_by_id(g_layer_root, layer_id);
        if (layer) {
            // 解析十六进制颜色 #RRGGBB
            if (strlen(color_hex) == 7 && color_hex[0] == '#') {
                layer->bg_color.r = (hex_to_int(color_hex[1]) * 16 + hex_to_int(color_hex[2]));
                layer->bg_color.g = (hex_to_int(color_hex[3]) * 16 + hex_to_int(color_hex[4]));
                layer->bg_color.b = (hex_to_int(color_hex[5]) * 16 + hex_to_int(color_hex[6]));
                layer->bg_color.a = 255;
                printf("JS(Mario): Set bg_color for layer '%s': %s\n", layer_id, color_hex);
            }
        }
    }

    return var_new_null(vm);
}

// 隐藏图层
static var_t* mario_hide(vm_t* vm, var_t* env, void* data)
{
    var_t* args = get_func_args(env);
    uint32_t argc = get_func_args_num(env);

    if (argc < 1) return var_new_null(vm);

    const char* layer_id = get_func_arg_str(env, 0);

    if (layer_id && g_layer_root) {
        struct Layer* layer = find_layer_by_id(g_layer_root, layer_id);
        if (layer) {
            layer->visible = 0; // IN_VISIBLE
            printf("JS(Mario): Hide layer '%s'\n", layer_id);
        }
    }

    return var_new_null(vm);
}

// 显示图层
static var_t* mario_show(vm_t* vm, var_t* env, void* data)
{
    var_t* args = get_func_args(env);
    uint32_t argc = get_func_args_num(env);

    if (argc < 1) return var_new_null(vm);

    const char* layer_id = get_func_arg_str(env, 0);

    if (layer_id && g_layer_root) {
        struct Layer* layer = find_layer_by_id(g_layer_root, layer_id);
        if (layer) {
            layer->visible = 1; // VISIBLE
            printf("JS(Mario): Show layer '%s'\n", layer_id);
        }
    }

    return var_new_null(vm);
}

// 打印日志
static var_t* mario_log(vm_t* vm, var_t* env, void* data)
{
    var_t* args = get_func_args(env);
    uint32_t argc = get_func_args_num(env);

    // 使用 Mario 的 var_to_str 来安全地转换任意类型为字符串
    mstr_t* output = mstr_new("");
    mstr_t* temp = mstr_new("");

    for (uint32_t i = 0; i < argc; i++) {
        if (i != 0) {
            mstr_append(output, " ");
        }
        node_t* n = var_array_get(args, i);
        if (n != NULL && n->var != NULL) {
            var_to_str(n->var, temp);
            mstr_append(output, temp->cstr);
        }
    }

    mstr_add(output, '\n');
    printf("%s", output->cstr);

    mstr_free(temp);
    mstr_free(output);

    return var_new_null(vm);
}

// 从 JSON 字符串动态渲染到指定图层
static var_t* mario_render_from_json(vm_t* vm, var_t* env, void* data)
{
    var_t* args = get_func_args(env);
    uint32_t argc = get_func_args_num(env);

    if (argc < 2) {
        printf("JS(Mario): ERROR - Expected 2 arguments: layer_id and json_string\n");
        return var_new_int(vm, -1);
    }

    const char* layer_id = get_func_arg_str(env, 0);
    const char* json_str = get_func_arg_str(env, 1);

    if (!layer_id || !json_str) {
        printf("JS(Mario): ERROR - Invalid arguments\n");
        return var_new_int(vm, -1);
    }

    printf("JS(Mario): renderFromJson called with layer_id='%s'\n", layer_id);

    if (g_layer_root) {
        // 查找目标图层
        Layer* parent_layer = find_layer_by_id(g_layer_root, layer_id);
        if (!parent_layer) {
            printf("JS(Mario): ERROR - Layer '%s' not found\n", layer_id);
            return var_new_int(vm, -1);
        }

        printf("JS(Mario): Found parent layer '%s'\n", layer_id);

        // 清除父图层的所有子图层
        if (parent_layer->children) {
            for (int i = 0; i < parent_layer->child_count; i++) {
                if (parent_layer->children[i]) {
                    destroy_layer(parent_layer->children[i]);
                }
            }
            free(parent_layer->children);
            parent_layer->children = NULL;
        }
        parent_layer->child_count = 0;

        // 从 JSON 字符串创建新图层
        Layer* new_layer = parse_layer_from_string(json_str, parent_layer);

        if (new_layer) {
            // 为子图层数组分配空间（初始分配1个，可以根据需要扩展）
            parent_layer->children = malloc(sizeof(Layer*));
            if (!parent_layer->children) {
                printf("JS(Mario): ERROR - Failed to allocate memory for children array\n");
                destroy_layer(new_layer);
                return var_new_int(vm, -2);
            }

            parent_layer->children[0] = new_layer;
            parent_layer->child_count = 1;

            // 重新布局父图层和新的子图层
            printf("JS(Mario): Reloading layout for parent layer '%s'\n", layer_id);
            layout_layer(parent_layer);
            printf("JS(Mario): Layout updated successfully\n");

            // 为新创建的图层加载字体
            printf("JS(Mario): Loading fonts for new layer\n");
            load_all_fonts(new_layer);
            printf("JS(Mario): Fonts loaded successfully\n");

            printf("JS(Mario): Successfully rendered JSON to layer '%s', new layer id: '%s'\n",
                   layer_id, new_layer->id);
            return var_new_int(vm, 0);
        } else {
            printf("JS(Mario): ERROR - Failed to parse JSON string\n");
            return var_new_int(vm, -3);
        }
    }

    printf("JS(Mario): ERROR - g_layer_root is NULL\n");
    return var_new_int(vm, -4);
}

/* ====================== 初始化和清理 ====================== */
extern bool compile(bytecode_t *bc, const char* input);

static void out(const char* str) {
    write(1, str, strlen(str));
}
// 初始化 JS 引擎（使用 Mario）
int js_module_init(void)
{
    printf("JS(Mario): Initializing Mario JavaScript engine...\n");
    _malloc = malloc;
    _free = free;
    _out_func = out;
    // 创建 Mario 虚拟机
    g_vm = vm_new(compile);
    if (!g_vm) {
        fprintf(stderr, "JS(Mario): Failed to create VM\n");
        return -1;
    }

    // 初始化虚拟机（这会调用 on_init 回调）
    vm_init(g_vm, NULL, NULL);

    // 注册 API 函数
    js_module_register_api();

    printf("JS(Mario): Mario JavaScript engine initialized\n");
    return 0;
}

// 清理 JS 引擎
void js_module_cleanup(void)
{
    if (g_vm) {
        vm_close(g_vm);
        g_vm = NULL;
    }
}

void reg_native_yui(vm_t* vm, const char* decl, native_func_t native, void* data) {
	var_t* cls = vm_new_class(vm, "YUI");
    var_t* cls2 = vm_new_class(vm, "Yui");
	vm_reg_native(vm, cls, decl, native, data);
	vm_reg_native(vm, cls2, decl, native, data);
}

static inline var_t* vm_load_var(vm_t* vm, const char* name, bool create) {
	node_t* n = vm_load_node(vm, name, create);
	if(n != NULL)
		return n->var;
	return NULL;
}

static void vm_load_basic_classes(vm_t* vm) {
	vm->var_String = vm_load_var(vm, "String", false);
	vm->var_Array = vm_load_var(vm, "Array", false);
	vm->var_Number = vm_load_var(vm, "Number", false);
}

extern void reg_basic_natives(vm_t* vm);
// 注册 C API 到 JS
void js_module_register_api(void)
{
    if (!g_vm) return;

    reg_basic_natives(g_vm);
    vm_load_basic_classes(g_vm);

    // 注册 YUI 类的方法
    var_t* yui_cls = vm_new_class(g_vm, "YUI");
    vm_reg_native(g_vm, yui_cls, "setText(layerId, text)", mario_set_text, NULL);
    vm_reg_native(g_vm, yui_cls, "getText(layerId)", mario_get_text, NULL);
    vm_reg_native(g_vm, yui_cls, "setBgColor(layerId, color)", mario_set_bg_color, NULL);
    vm_reg_native(g_vm, yui_cls, "hide(layerId)", mario_hide, NULL);
    vm_reg_native(g_vm, yui_cls, "show(layerId)", mario_show, NULL);
    vm_reg_native(g_vm, yui_cls, "renderFromJson(layerId, json)", mario_render_from_json, NULL);
    vm_reg_native(g_vm, yui_cls, "log(...)", mario_log, NULL);

    // 也注册为全局函数（为了兼容性）
    vm_reg_static(g_vm, NULL, "setText(layerId, text)", mario_set_text, NULL);
    vm_reg_static(g_vm, NULL, "getText(layerId)", mario_get_text, NULL);
    vm_reg_static(g_vm, NULL, "setBgColor(layerId, color)", mario_set_bg_color, NULL);
    vm_reg_static(g_vm, NULL, "hide(layerId)", mario_hide, NULL);
    vm_reg_static(g_vm, NULL, "show(layerId)", mario_show, NULL);

    printf("JS(Mario): Registered native API functions\n");
}

// 加载并执行 JS 文件
int js_module_load_file(const char* filename)
{
    if (!g_vm) {
        fprintf(stderr, "JS(Mario): Engine not initialized\n");
        return -1;
    }

    printf("JS(Mario): Loading file %s...\n", filename);

    int len = 0;
    uint8_t* buf = load_file(filename, &len);
    if (!buf) {
        return -1;
    }

    // 使用 Mario 加载并运行代码
    bool success = vm_load_run(g_vm, (const char*)buf);
    free(buf);

    if (!success) {
        fprintf(stderr, "JS(Mario): Error executing %s\n", filename);
        return -1;
    }

    printf("JS(Mario): Successfully loaded %s\n", filename);
    return 0;
}

// 调用 JS 事件函数
int js_module_call_event(const char* event_name, Layer* layer)
{
    if (!g_vm || !event_name) return -1;

    // 移除 @ 前缀（如果有）
    const char* func_name = event_name;
    if (func_name[0] == '@') {
        func_name++;
    }

    // 查找函数
    node_t* func_node = vm_find(g_vm, func_name);
    if (!func_node || !func_node->var || !var_get_func(func_node->var)) {
        return -1;
    }

    // 创建参数数组
    var_t* args = var_new_array(g_vm);
    if (layer) {
        var_array_add(args, var_new_str(g_vm, layer->id));
    } else {
        var_array_add(args, var_new_null(g_vm));
    }

    // 调用函数
    var_t* result = call_m_func_by_name(g_vm, NULL, func_name, args);

    var_unref(args);

    if (!result) {
        fprintf(stderr, "JS(Mario): Error calling event %s\n", event_name);
        return -1;
    }

    var_unref(result);
    return 0;
}


