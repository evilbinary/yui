#include "js_module.h"


#include "mquickjs.h"
#include "../../src/ytype.h"

#include "event.h"
#define CONFIG_CLASS_SOCKET
#include "js_socket.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Layer 结构的最小定义（只使用我们需要的字段）
#define MAX_TEXT 256
#ifndef YUI_MAX_PATH
#define YUI_MAX_PATH 1024
#endif

// 全局 JS 上下文
static JSContext* g_js_ctx = NULL;
static uint8_t* g_js_mem = NULL;
#ifndef JS_MEM_POOL_SIZE
#define JS_MEM_POOL_SIZE (200 * 1024)
#endif
static size_t g_js_mem_size = JS_MEM_POOL_SIZE;

// 全局 UI 根图层
extern struct Layer* g_layer_root;


static void check_timers(void);
void js_module_pump_timers(void);

extern void backend_register_update_callback(void (*callback)(void));


extern const JSSTDLibraryDef js_yuistdlib;
extern uint8_t* load_file(const char *filename, int *plen);
extern int hex_to_int(char c);
extern const char* js_module_get_root(void);

/* 拆分字节码只读区（flash XIP）基址：esp32 用 esp_partition_mmap 映射
   bcrom 分区；其它平台无此能力返回 NULL。 */
#if defined(YUI_ESP_PLATFORM)
extern const void* backend_esp32_bc_rom_base(size_t* psize);
#else
__attribute__((weak)) const void* backend_esp32_bc_rom_base(size_t* psize) {
    if (psize) *psize = 0;
    return NULL;
}
#endif


// 初始化 JS 引擎
int js_module_init(void)
{
    printf("JS: Initializing JavaScript engine...\n");

#if defined(YUI_ESP_PLATFORM)
    extern size_t heap_caps_get_free_size(int caps);
    printf("JS: heap free=%u\n", (unsigned)heap_caps_get_free_size(4));
#endif

    g_js_mem = malloc(g_js_mem_size);
    if (!g_js_mem) {
        fprintf(stderr, "JS: Failed to allocate memory (%u bytes)\n", (unsigned)g_js_mem_size);
        return -1;
    }
    printf("JS: JS_MEM_POOL_SIZE=%u bytes\n", (unsigned)g_js_mem_size);

    g_js_ctx = JS_NewContext(g_js_mem, g_js_mem_size, &js_yuistdlib);
    if (!g_js_ctx) {
        fprintf(stderr, "JS: Failed to create context\n");
        free(g_js_mem);
        return -1;
    }

    js_module_register_api();
    js_module_init_layer_lifecycle();

    /* 驱动 setTimeout/clearTimeout：注册到 backend 每帧 update 回调 */
    backend_register_update_callback(js_module_pump_timers);

    printf("JS: JavaScript engine initialized\n");
    return 0;
}

// 清理 JS 引擎
void js_module_cleanup(void)
{
    if (g_layer_root) {
        js_module_shutdown();
    }
    g_layer_root = NULL;

    if (g_js_ctx) {
        JS_FreeContext(g_js_ctx);
        g_js_ctx = NULL;
    }
    if (g_js_mem) {
        free(g_js_mem);
        g_js_mem = NULL;
    }
}

// 统一的 Socket API 注册函数
#ifdef CONFIG_CLASS_SOCKET
extern void js_module_register_socket_api(JSContext* ctx);
#endif
extern void js_module_register_yui_api(JSContext* ctx);
extern void js_module_register_game_api(JSContext* ctx);
#ifdef YUI_WITH_GAME
extern void js_module_register_timer_api(JSContext* ctx);
extern void js_module_register_perf_api(JSContext* ctx);
#endif

// 注册 C API 到 JS
void js_module_register_api(void)
{
    if (!g_js_ctx) return;
    
    // YUI API 
    js_module_register_yui_api(g_js_ctx);
    
#ifdef CONFIG_CLASS_SOCKET
    // 调用统一的 Socket API 注册函数
    js_module_register_socket_api(g_js_ctx);
#endif
    // Game API
    js_module_register_game_api(g_js_ctx);
#ifdef YUI_WITH_GAME
    // Timer API
    js_module_register_timer_api(g_js_ctx);
    // Perf API
    js_module_register_perf_api(g_js_ctx);
#endif
    
    printf("JS(Socket): Registered API function\n");
}


/* 二进制安全读取文件（相对路径回退 g_js_root）。返回 malloc 缓冲区，
   长度写入 *plen（含二进制 \0 不截断）。失败返回 NULL。 */
static uint8_t* read_file_binary(const char* filename, int* plen)
{
    char path_buf[YUI_MAX_PATH];
    FILE* f = NULL;
    long size;
    uint8_t* buf;

    f = fopen(filename, "rb");
    if (!f) {
        const char* root = js_module_get_root();
        if (root && root[0] && filename[0] != '/') {
            snprintf(path_buf, sizeof(path_buf), "%s/%s", root, filename);
            f = fopen(path_buf, "rb");
        }
    }
    if (!f) {
        return NULL;
    }
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    size = ftell(f);
    if (size < 0 || fseek(f, 0, SEEK_SET) != 0) { fclose(f); return NULL; }
    buf = (uint8_t*)malloc((size_t)size + 1);
    if (!buf) { fclose(f); return NULL; }
    if (fread(buf, 1, (size_t)size, f) != (size_t)size) {
        free(buf); fclose(f); return NULL;
    }
    buf[size] = '\0';
    fclose(f);
    if (plen) *plen = (int)size;
    return buf;
}

/* 构造同目录字节码路径：xxx.js -> xxx.bc */
static void js_bc_path(const char* filename, char* out, size_t out_sz)
{
    size_t n = strlen(filename);
    if (n > 3 && strcmp(filename + n - 3, ".js") == 0) {
        snprintf(out, out_sz, "%.*s.bc", (int)(n - 3), filename);
    } else {
        snprintf(out, out_sz, "%s.bc", filename);
    }
}

/* build_bcrom.py 生成的新格式：28B 头 + ram 区 + 重定位表 */
#define BCRAM_MAGIC 0x53435242u  /* "BCRS" */

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t flags;
    uint32_t ram_len;      /* 字节数（含 16B JSBytecodeHeader 副本） */
    uint32_t rom_off;      /* 本文件只读区在 bcrom 分区内的偏移 */
    uint32_t rom_len;
    uint32_t fixup_count;
} BcRamHeader;

/* 轻量判断 bc_path 是否为 BCRS 拆分格式（只读 24B 头，不整文件读入 RAM） */
static int js_bc_is_split(const char* bc_path)
{
    FILE* f;
    BcRamHeader hdr;
    long fsize;
    int ret = 0;

    f = fopen(bc_path, "rb");
    if (!f) {
        const char* root = js_module_get_root();
        char path_buf[YUI_MAX_PATH];
        if (root && root[0] && bc_path[0] != '/') {
            snprintf(path_buf, sizeof(path_buf), "%s/%s", root, bc_path);
            f = fopen(path_buf, "rb");
        }
    }
    if (!f)
        return 0;

    if (fread(&hdr, 1, sizeof(hdr), f) == sizeof(hdr) &&
        hdr.magic == BCRAM_MAGIC && hdr.ram_len != 0 &&
        fseek(f, 0, SEEK_END) == 0 && (fsize = ftell(f)) >= 0 &&
        (long)sizeof(hdr) + (long)hdr.ram_len + (long)hdr.fixup_count * 8 <= fsize)
        ret = 1;

    fclose(f);
    return ret;
}

/* 加载拆分格式字节码：直接按需读文件（头 + RAM 骨架 + 流式 fixup），不再
   把整个 .bc 文件读入 RAM。ROM 只读区（string/byte_array/float64）始终走
   bcrom flash XIP（backend_esp32_bc_rom_base mmap），不进 RAM。
   返回需长期持有的 RAM 骨架（context 生命周期内有效）。 */
static uint8_t* js_load_bytecode_split(JSContext* ctx, const char* bc_path, uint32_t* p_ram_len)
{
    BcRamHeader hdr;
    FILE* f;
    uint8_t* ram;
    const void* rom_base;
    size_t rom_part_size = 0;
    uint32_t i;

    f = fopen(bc_path, "rb");
    if (!f) {
        const char* root = js_module_get_root();
        char path_buf[YUI_MAX_PATH];
        if (root && root[0] && bc_path[0] != '/') {
            snprintf(path_buf, sizeof(path_buf), "%s/%s", root, bc_path);
            f = fopen(path_buf, "rb");
        }
        if (!f) {
            fprintf(stderr, "JS: cannot open %s\n", bc_path);
            return NULL;
        }
    }

    if (fread(&hdr, 1, sizeof(hdr), f) != sizeof(hdr)) {
        fprintf(stderr, "JS: split bytecode too short\n");
        fclose(f);
        return NULL;
    }
    if (hdr.magic != BCRAM_MAGIC || hdr.ram_len == 0) {
        fprintf(stderr, "JS: invalid split bytecode header\n");
        fclose(f);
        return NULL;
    }

    rom_base = backend_esp32_bc_rom_base(&rom_part_size);
    if (!rom_base) {
        fprintf(stderr, "JS: bcrom partition not mapped (flash bytecode needs XIP)\n");
        fclose(f);
        return NULL;
    }
    if ((size_t)hdr.rom_off + hdr.rom_len > rom_part_size) {
        fprintf(stderr, "JS: bcrom range out of partition (%u+%u > %u)\n",
                (unsigned)hdr.rom_off, (unsigned)hdr.rom_len, (unsigned)rom_part_size);
        fclose(f);
        return NULL;
    }

    ram = (uint8_t*)malloc(hdr.ram_len);
    if (!ram) {
        fprintf(stderr, "JS: out of memory for bytecode skeleton (ram_len=%u)\n",
                (unsigned)hdr.ram_len);
#if defined(YUI_ESP_PLATFORM)
        extern size_t heap_caps_get_free_size(int caps);
        extern size_t heap_caps_get_largest_free_block(int caps);
        fprintf(stderr, "JS: sys free=%u largest=%u\n",
                (unsigned)heap_caps_get_free_size(4),
                (unsigned)heap_caps_get_largest_free_block(4));
#endif
        fclose(f);
        return NULL;
    }
    if (p_ram_len)
        *p_ram_len = hdr.ram_len;
    /* 跳过 24B 头，直接读 RAM 区到骨架 */
    if (fread(ram, 1, hdr.ram_len, f) != hdr.ram_len) {
        fprintf(stderr, "JS: short read of split RAM region\n");
        free(ram);
        fclose(f);
        return NULL;
    }

    /* 流式读重定位表并应用：每个条目 = (site, toff)，toff bit31=1 表示目标在
       ROM 区（bcrom XIP），否则在 RAM 骨架内 */
    for (i = 0; i < hdr.fixup_count; i++) {
        uint32_t site, toff;
        uintptr_t addr;
        if (fread(&site, 4, 1, f) != 1 || fread(&toff, 4, 1, f) != 1) {
            fprintf(stderr, "JS: short read of fixup table\n");
            free(ram);
            fclose(f);
            return NULL;
        }
        addr = (toff & 0x80000000u)
                   ? ((uintptr_t)rom_base + hdr.rom_off + (toff & 0x7fffffffu))
                   : ((uintptr_t)ram + (toff & 0x7fffffffu));
        /* JSValue 指针编码：值 = 地址 + 1 */
        *(uint32_t*)(ram + site) = (uint32_t)(addr + 1);
    }
    fclose(f);

    /* 规范化字符串 atom：把字节码里的标识符/属性名字符串绑定到 ROM 标准库
       表的规范字符串（JS_MakeUniqueString 语义，指针身份等值才匹配）。
       先用真实的 data base 修正 base_addr，再以 offset=0 跑 update_atoms 遍，
       只做去重、不改指针。若缺失，全局标识符（如 YUI）会因字符串对象不同而
       解析失败 -> "variable 'YUI' is not defined"。 */
    {
        JSBytecodeHeader* ram_hdr = (JSBytecodeHeader*)ram;
        ram_hdr->base_addr = (uintptr_t)(ram + sizeof(JSBytecodeHeader));
        if (JS_RelocateBytecode2(ctx, ram_hdr, ram + sizeof(JSBytecodeHeader),
                                 hdr.ram_len - sizeof(JSBytecodeHeader),
                                 (uintptr_t)(ram + sizeof(JSBytecodeHeader)),
                                 1)) {
            fprintf(stderr, "JS: could not canonicalize split bytecode atoms\n");
            free(ram);
            return NULL;
        }
    }

    return ram;
}


// 加载并执行 JS 文件（优先同目录 xxx.bc 预编译字节码，回退源码 eval）
int js_module_load_file(const char* filename)
{
    if (!g_js_ctx) {
        fprintf(stderr, "JS: Engine not initialized\n");
        return -1;
    }

    printf("JS: Loading file %s...\n", filename);

    int len = 0;
    uint8_t* buf = NULL;
    char bc_path[YUI_MAX_PATH];
    JSValue val;
    int is_bc = 0;

    js_bc_path(filename, bc_path, sizeof(bc_path));
    if (js_bc_is_split(bc_path)) {
        /* 拆分格式：直接按需读文件，只保留 RAM 骨架；ROM 区走 bcrom XIP */
        is_bc = 1;
        {
            uint32_t ram_len = 0;
            uint8_t* ram = js_load_bytecode_split(g_js_ctx, bc_path, &ram_len);
            if (!ram) {
                return -1;
            }
            len = (int)ram_len;
            {
                JSBytecodeHeader* hdr = (JSBytecodeHeader*)ram;
                printf("JS: Loading split bytecode %s\n", bc_path);
                fflush(stdout);
                val = JS_LoadBytecode2(g_js_ctx, hdr);
                if (JS_IsException(val)) {
                    JSValue exc = JS_GetException(g_js_ctx);
                    fprintf(stderr, "JS: Error loading split bytecode %s:\n", bc_path);
                    JS_PrintValueF(g_js_ctx, exc, JS_DUMP_LONG);
                    printf("\n");
                    return -1;
                }
                printf("JS: split bytecode ready\n");
                fflush(stdout);
                /* ram 骨架需在 context 生命周期内保持，不 free */
                val = JS_Run(g_js_ctx, val);
            }
        }
    } else {
        buf = read_file_binary(bc_path, &len);
        if (buf && JS_IsBytecode(buf, (size_t)len)) {
        is_bc = 1;
        printf("JS: Loading bytecode %s (len=%d)\n", bc_path, len);
        fflush(stdout);
        {
            /* 手动 relocate（update_atoms=TRUE）：搬移指针并把字节码字符串
               规范化到 ROM 标准库表（与 split 路径一致）。 */
            JSBytecodeHeader* hdr = (JSBytecodeHeader*)buf;
            uint8_t* data_ptr = buf + sizeof(JSBytecodeHeader);
            if (JS_RelocateBytecode2(g_js_ctx, hdr, data_ptr,
                                     (uint32_t)(len - sizeof(JSBytecodeHeader)),
                                     (uintptr_t)data_ptr, 1)) {
                fprintf(stderr, "JS: Could not relocate bytecode %s\n", bc_path);
                free(buf);
                return -1;
            }
        }
        val = JS_LoadBytecode(g_js_ctx, buf);
        if (JS_IsException(val)) {
            JSValue exc = JS_GetException(g_js_ctx);
            fprintf(stderr, "JS: Error loading bytecode %s:\n", bc_path);
            JS_PrintValueF(g_js_ctx, exc, JS_DUMP_LONG);
            printf("\n");
            free(buf);
            return -1;
        }
        /* main_func 与 hdr->unique_strings 都引用 buf 内的字节码数据，且
           JS_LoadBytecode 把 unique_strings 表指针存进 ctx->rom_atom_tables，
           必须在 JSContext 生命周期内保持 buf 有效（README: "buf must be
           allocated as long as the JSContext exists"）。因此不 free。 */
        val = JS_Run(g_js_ctx, val);
    } else {
        if (buf) free(buf);
        buf = load_file(filename, &len);
        if (!buf) {
            fprintf(stderr, "JS: Failed to load JS file %s\n", filename);
            return -1;
        }
        val = JS_Eval(g_js_ctx, (const char*)buf, (size_t)len, filename, 0);
        free(buf);
    }
    }

    if (JS_IsException(val)) {
        JSValue exc = JS_GetException(g_js_ctx);
        fprintf(stderr, "JS: Error executing %s:\n", filename);
        JS_PrintValueF(g_js_ctx, exc, JS_DUMP_LONG);
        printf("\n");
        return -1;
    }

    printf("JS: Successfully loaded %s, len=%d (%s)\n", filename, len,
           is_bc ? "bytecode" : "source");
    return 0;
}
// 调用 JS 事件函数
// onTouch: (layerId, event)  event = { type, deltaX, deltaY, pointerId, fingerCount, x, y }
// 其它:    (layerId)
static int event_name_is_layer_touch(const Layer* layer, const char* event_name)
{
    const char* tn;
    const char* en;
    if (!layer || !layer->event || !event_name || !event_name[0]) {
        return 0;
    }
    tn = layer->event->touch_name;
    if (!tn[0]) {
        return 0;
    }
    if (strcmp(tn, event_name) == 0) {
        return 1;
    }
    en = event_name[0] == '@' ? event_name + 1 : event_name;
    tn = tn[0] == '@' ? tn + 1 : tn;
    return strcmp(tn, en) == 0;
}

static JSValue js_make_pointer_event_object(JSContext* ctx, const PointerEvent* pe)
{
    JSValue ev = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, ev, "type",
                      JS_NewString(ctx, pointer_phase_to_string(pe->phase)));
    JS_SetPropertyStr(ctx, ev, "deltaX", JS_NewInt32(ctx, pe->delta_x));
    JS_SetPropertyStr(ctx, ev, "deltaY", JS_NewInt32(ctx, pe->delta_y));
    JS_SetPropertyStr(ctx, ev, "pointerId", JS_NewInt32(ctx, pe->pointer_id));
    JS_SetPropertyStr(ctx, ev, "fingerCount",
                      JS_NewInt32(ctx, pe->device == POINTER_DEVICE_TOUCH
                                          ? pe->finger_count
                                          : 1));
    JS_SetPropertyStr(ctx, ev, "x", JS_NewInt32(ctx, pe->x));
    JS_SetPropertyStr(ctx, ev, "y", JS_NewInt32(ctx, pe->y));
    return ev;
}

int js_module_call_event(const char* event_name, Layer* layer)
{
    if (!g_js_ctx || !event_name) return -1;

    /* 拷到栈上：event_name / layer->id 常指向 Layer 内嵌字段；
     * GetProperty/Call 可能触发 mquickjs GC，踩坏相邻堆（曾见名变成 "ormal"）。 */
    char func_name_buf[128];
    char layer_id_buf[64];
    const char* src = event_name[0] == '@' ? event_name + 1 : event_name;
    strncpy(func_name_buf, src, sizeof(func_name_buf) - 1);
    func_name_buf[sizeof(func_name_buf) - 1] = '\0';

    layer_id_buf[0] = '\0';
    if (layer && layer->id[0]) {
        strncpy(layer_id_buf, layer->id, sizeof(layer_id_buf) - 1);
        layer_id_buf[sizeof(layer_id_buf) - 1] = '\0';
    }

    const PointerEvent* pe = get_current_pointer_event();
    int is_gesture = event_name_is_layer_touch(layer, event_name) && pe != NULL;
    int argc = is_gesture ? 2 : 1;

    JSValue global_obj = JS_GetGlobalObject(g_js_ctx);
    JSValue func = JS_GetPropertyStr(g_js_ctx, global_obj, func_name_buf);
    printf("JS: call_event '%s' getprop is_func=%d\n", func_name_buf,
           JS_IsFunction(g_js_ctx, func));

    if (JS_IsUndefined(func) || !JS_IsFunction(g_js_ctx, func)) {
        printf("JS: call_event '%s' not on global, try map/eval\n", func_name_buf);
        JS_FreeValue(g_js_ctx, global_obj);
        JS_FreeValue(g_js_ctx, func);

        if (js_module_trigger_event(func_name_buf, layer) == 0) {
            return 0;
        }

        /* mquickjs：部分 function 声明对 GetProperty 不可见，用 eval 按名调用 */
        char expr[256];
        if (layer_id_buf[0]) {
            snprintf(expr, sizeof(expr),
                     "(function(){if(typeof %s!=='function')throw new Error('no %s');%s('%s');})()",
                     func_name_buf, func_name_buf, func_name_buf, layer_id_buf);
        } else {
            snprintf(expr, sizeof(expr),
                     "(function(){if(typeof %s!=='function')throw new Error('no %s');%s();})()",
                     func_name_buf, func_name_buf, func_name_buf);
        }
        JSValue v = JS_Eval(g_js_ctx, expr, strlen(expr), "<call_event>", 0);
        if (JS_IsException(v)) {
            JSValue exc = JS_GetException(g_js_ctx);
            printf("JS: Eval call %s failed: ", func_name_buf);
            JS_PrintValueF(g_js_ctx, exc, JS_DUMP_LONG);
            printf("\n");
            return -1;
        }
        return 0;
    }

    if (JS_StackCheck(g_js_ctx, (uint32_t)(argc + 2))) {
        printf("JS: call_event '%s' stack/oom\n", func_name_buf);
        JS_FreeValue(g_js_ctx, global_obj);
        JS_FreeValue(g_js_ctx, func);
        return -1;
    }

    JSValue layer_id_val = layer_id_buf[0] ? JS_NewString(g_js_ctx, layer_id_buf) : JS_NULL;

    if (is_gesture) {
        JS_PushArg(g_js_ctx, js_make_pointer_event_object(g_js_ctx, pe));
    }
    JS_PushArg(g_js_ctx, layer_id_val);
    JS_PushArg(g_js_ctx, func);
    JS_PushArg(g_js_ctx, JS_NULL);
    JSValue result = JS_Call(g_js_ctx, argc);

    JS_FreeValue(g_js_ctx, global_obj);

    if (JS_IsException(result)) {
        JSValue exc = JS_GetException(g_js_ctx);
        printf("JS: Error calling event %s:\n", func_name_buf);
        JS_PrintValueF(g_js_ctx, exc, JS_DUMP_LONG);
        printf("\n");
        return -1;
    }

    return 0;
}


// 检查并触发定时器（内部静态函数）
static void check_timers(void)
{
    if (!g_js_ctx) return;
    js_run_timers(g_js_ctx);
}

// 供 UI 主循环周期性调用，驱动 setTimeout/setInterval
void js_module_pump_timers(void)
{
    check_timers();
}

void* js_module_get_context(void) {
    return g_js_ctx;
}
