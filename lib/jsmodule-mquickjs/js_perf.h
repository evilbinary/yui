#ifndef JS_PERF_H
#define JS_PERF_H

#include "mquickjs.h"
#include "mquickjs_build.h"

extern const JSClassDef js_perf_class;
void js_module_register_perf_api(JSContext* ctx);

#endif
