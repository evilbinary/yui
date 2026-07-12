#ifndef YUI_JS_TIMER_H
#define YUI_JS_TIMER_H

#include "../../lib/quickjs/quickjs.h"

#ifdef __cplusplus
extern "C" {
#endif

void js_timer_register_globals(JSContext* ctx);
void js_timer_run(JSContext* ctx);
void js_timer_clear_all(JSContext* ctx);

#ifdef __cplusplus
}
#endif

#endif
