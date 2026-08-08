#include <stdio.h>
#include "mquickjs.h"
#include "mquickjs_build.h"
#include "../../src/game/game.h"

static JSValue js_perf_get_stats(JSContext* ctx, JSValue* this_val, int argc, JSValue* argv)
{
    const GamePerfStats* st = game_perf_get_stats();
    JSValue obj = JS_NewObject(ctx);
    (void)this_val; (void)argc; (void)argv;
    if (!st) {
        return obj;
    }
    JS_SetPropertyStr(ctx, obj, "entities", JS_NewInt32(ctx, st->entities));
    JS_SetPropertyStr(ctx, obj, "draws", JS_NewInt32(ctx, st->draws));
    JS_SetPropertyStr(ctx, obj, "particles", JS_NewInt32(ctx, st->particles));
    JS_SetPropertyStr(ctx, obj, "fps", JS_NewFloat64(ctx, st->fps));
    JS_SetPropertyStr(ctx, obj, "updateMs", JS_NewFloat64(ctx, st->update_ms));
    JS_SetPropertyStr(ctx, obj, "renderMs", JS_NewFloat64(ctx, st->render_ms));
    return obj;
}

static const JSPropDef js_perf[] = {
    JS_CFUNC_DEF("getStats", 0, js_perf_get_stats),
    JS_PROP_END,
};

const JSClassDef js_perf_class =
    JS_OBJECT_DEF("Perf", js_perf);

void js_module_register_perf_api(JSContext* ctx)
{
    (void)ctx;
    printf("JS(mquickjs): Registered Perf API\n");
}
