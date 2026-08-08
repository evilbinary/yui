#ifndef JS_TIMER_H
#define JS_TIMER_H

#include "mquickjs.h"
#include "mquickjs_build.h"

extern const JSClassDef js_timer_class;
void js_module_register_timer_api(JSContext* ctx);

#endif
