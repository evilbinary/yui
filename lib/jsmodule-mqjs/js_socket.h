#ifndef JS_SOCKET_H
#define JS_SOCKET_H

#include "mquickjs.h"


#define JS_CLASS_SOCKET (JS_CLASS_USER + 1)

void js_module_register_socket_api(JSContext* ctx);

#endif