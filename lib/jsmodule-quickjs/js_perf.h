#ifndef YUI_JS_PERF_H
#define YUI_JS_PERF_H

#include <quickjs.h>

#ifdef __cplusplus
extern "C" {
#endif

void js_module_register_perf_api(JSContext* ctx, JSValueConst yui_obj);

#ifdef __cplusplus
}
#endif

#endif
