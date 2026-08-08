#include <stdio.h>
#include "mquickjs.h"
#include "mquickjs_build.h"
#include "../../src/game/game.h"

static JSValue js_timer_dt(JSContext* ctx, JSValue* this_val, int argc, JSValue* argv)
{
    (void)this_val; (void)argc; (void)argv;
    return JS_NewFloat64(ctx, game_time_dt());
}

static const JSPropDef js_timer[] = {
    JS_CFUNC_DEF("dt", 0, js_timer_dt),
    JS_CFUNC_DEF("getDt", 0, js_timer_dt),
    JS_PROP_END,
};

const JSClassDef js_timer_class =
    JS_OBJECT_DEF("Timer", js_timer);

void js_module_register_timer_api(JSContext* ctx)
{
    (void)ctx;
    printf("JS(mquickjs): Registered Timer API\n");
}
