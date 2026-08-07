/* bc_compile.c: PC 端字节码编译工具入口。
 *
 * 用 YUI 的 ROM 标准库表（js_yuistdlib，含 YUI/Socket atom）编译 JS 脚本为
 * mquickjs 32 位字节码，保证编译时解析的全局标识符（YUI 等）与 ESP32 运行时
 * 的 atom 表一致，字节码运行时才能解析到这些全局对象。
 *
 * 用法: bc_compile -o out.bc [-b base_addr] script.js
 */
#define MQJS_STDLIB_DEF js_yuistdlib

#include "yui_stdlib_stubs.c"
#include "yui_stdlib_link.c"
#include "../mquickjs/mqjs.c"
