#ifndef YUI_JS_SOCKET_H
#define YUI_JS_SOCKET_H

#include <quickjs.h>

#ifdef __cplusplus
extern "C" {
#endif

// 注册 Socket 相关的函数到 JS
void js_module_register_socket_api(JSContext* ctx);

#ifdef __cplusplus
}
#endif

#endif