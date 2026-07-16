#include "js_perf.h"
#include "../../lib/quickjs/quickjs.h"
#include "../../src/perf/perf.h"
#include <stdio.h>
#include <string.h>

static int js_perf_parse_bool(JSContext* ctx, JSValueConst val)
{
    return JS_ToBool(ctx, val);
}

static JSValue js_perf_enable(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv)
{
    perf_enable(1);
    printf("YUI Perf: enabled\n");
    return JS_NewBool(ctx, 1);
}

static JSValue js_perf_disable(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv)
{
    perf_enable(0);
    printf("YUI Perf: disabled\n");
    return JS_NewBool(ctx, 1);
}

static JSValue js_perf_reset(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv)
{
    perf_reset();
    return JS_NewBool(ctx, 1);
}

static JSValue js_perf_set_overlay(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv)
{
    if (argc < 1) {
        return JS_ThrowTypeError(ctx, "perf.setOverlay requires 1 argument");
    }
    perf_set_overlay(js_perf_parse_bool(ctx, argv[0]));
    return JS_NewBool(ctx, 1);
}

static JSValue js_perf_set_top_n(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv)
{
    int n;
    if (argc < 1 || JS_ToInt32(ctx, &n, argv[0]) != 0) {
        return JS_ThrowTypeError(ctx, "perf.setTopN requires an integer");
    }
    perf_set_top_n(n);
    return JS_NewBool(ctx, 1);
}

static JSValue js_perf_set_log_interval(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv)
{
    int frames;
    if (argc < 1 || JS_ToInt32(ctx, &frames, argv[0]) != 0) {
        return JS_ThrowTypeError(ctx, "perf.setLogInterval requires an integer");
    }
    perf_set_log_interval(frames);
    return JS_NewBool(ctx, 1);
}

static JSValue js_perf_watch(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv)
{
    if (argc < 1) {
        return JS_ThrowTypeError(ctx, "perf.watch requires layer id");
    }
    const char* layer_id = JS_ToCString(ctx, argv[0]);
    int ok = layer_id ? perf_watch(layer_id) : 0;
    if (layer_id) {
        JS_FreeCString(ctx, layer_id);
    }
    return JS_NewBool(ctx, ok);
}

static JSValue js_perf_unwatch(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv)
{
    if (argc < 1) {
        return JS_ThrowTypeError(ctx, "perf.unwatch requires layer id");
    }
    const char* layer_id = JS_ToCString(ctx, argv[0]);
    int ok = layer_id ? perf_unwatch(layer_id) : 0;
    if (layer_id) {
        JS_FreeCString(ctx, layer_id);
    }
    return JS_NewBool(ctx, ok);
}

static JSValue js_perf_clear_watch(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv)
{
    perf_clear_watch();
    return JS_NewBool(ctx, 1);
}

static JSValue js_perf_get_frame_stats(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv)
{
    const PerfFrameStats* stats = perf_get_frame_stats();
    JSValue obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, obj, "fps", JS_NewFloat64(ctx, stats ? stats->fps : 0.0));
    JS_SetPropertyStr(ctx, obj, "frameIndex", JS_NewInt64(ctx, stats ? (int64_t)stats->frame_index : 0));
    JS_SetPropertyStr(ctx, obj, "frameMs", JS_NewFloat64(ctx, stats ? (double)stats->frame_ns / 1000000.0 : 0.0));
    JS_SetPropertyStr(ctx, obj, "renderMs", JS_NewFloat64(ctx, stats ? (double)stats->render_tree_ns / 1000000.0 : 0.0));
    JS_SetPropertyStr(ctx, obj, "layerCount", JS_NewInt32(ctx, stats ? (int)stats->layers_rendered : 0));
    return obj;
}

static PerfSortBy js_perf_parse_sort(JSContext* ctx, JSValueConst val)
{
    if (!JS_IsString(val)) {
        return PERF_SORT_TIME;
    }
    const char* sort = JS_ToCString(ctx, val);
    if (!sort) {
        return PERF_SORT_TIME;
    }
    PerfSortBy result = PERF_SORT_TIME;
    if (strcmp(sort, "count") == 0) {
        result = PERF_SORT_COUNT;
    } else if (strcmp(sort, "name") == 0) {
        result = PERF_SORT_NAME;
    }
    JS_FreeCString(ctx, sort);
    return result;
}

static JSValue js_perf_get_layer_stats(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv)
{
    PerfSortBy sort_by = PERF_SORT_TIME;
    if (argc >= 1) {
        sort_by = js_perf_parse_sort(ctx, argv[0]);
    }

    PerfLayerStats stats[32];
    int count = perf_get_layer_stats(stats, 32, sort_by);
    JSValue arr = JS_NewArray(ctx);

    for (int i = 0; i < count; i++) {
        JSValue item = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, item, "id", JS_NewString(ctx, stats[i].id ? stats[i].id : ""));
        JS_SetPropertyStr(ctx, item, "type", JS_NewInt32(ctx, stats[i].type));
        JS_SetPropertyStr(ctx, item, "renderCount", JS_NewInt32(ctx, (int)stats[i].render_count));
        JS_SetPropertyStr(ctx, item, "renderMs", JS_NewFloat64(ctx, (double)stats[i].render_ns / 1000000.0));
        JS_SetPropertyStr(ctx, item, "totalRenderMs", JS_NewFloat64(ctx, (double)stats[i].total_render_ns / 1000000.0));
        JS_SetPropertyStr(ctx, item, "maxRenderCount", JS_NewInt32(ctx, (int)stats[i].max_render_count));
        JS_SetPropertyStr(ctx, item, "framesSeen", JS_NewInt64(ctx, (int64_t)stats[i].total_frames_seen));
        JS_SetPropertyUint32(ctx, arr, (uint32_t)i, item);
    }

    return arr;
}

void js_module_register_perf_api(JSContext* ctx, JSValueConst yui_obj)
{
    JSValue perf_obj;

    if (!ctx || JS_IsUndefined(yui_obj) || JS_IsNull(yui_obj)) {
        return;
    }

    perf_obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, perf_obj, "enable", JS_NewCFunction(ctx, js_perf_enable, "enable", 0));
    JS_SetPropertyStr(ctx, perf_obj, "disable", JS_NewCFunction(ctx, js_perf_disable, "disable", 0));
    JS_SetPropertyStr(ctx, perf_obj, "reset", JS_NewCFunction(ctx, js_perf_reset, "reset", 0));
    JS_SetPropertyStr(ctx, perf_obj, "setOverlay", JS_NewCFunction(ctx, js_perf_set_overlay, "setOverlay", 1));
    JS_SetPropertyStr(ctx, perf_obj, "setTopN", JS_NewCFunction(ctx, js_perf_set_top_n, "setTopN", 1));
    JS_SetPropertyStr(ctx, perf_obj, "setLogInterval", JS_NewCFunction(ctx, js_perf_set_log_interval, "setLogInterval", 1));
    JS_SetPropertyStr(ctx, perf_obj, "watch", JS_NewCFunction(ctx, js_perf_watch, "watch", 1));
    JS_SetPropertyStr(ctx, perf_obj, "unwatch", JS_NewCFunction(ctx, js_perf_unwatch, "unwatch", 1));
    JS_SetPropertyStr(ctx, perf_obj, "clearWatch", JS_NewCFunction(ctx, js_perf_clear_watch, "clearWatch", 0));
    JS_SetPropertyStr(ctx, perf_obj, "getFrameStats", JS_NewCFunction(ctx, js_perf_get_frame_stats, "getFrameStats", 0));
    JS_SetPropertyStr(ctx, perf_obj, "getLayerStats", JS_NewCFunction(ctx, js_perf_get_layer_stats, "getLayerStats", 1));
    JS_SetPropertyStr(ctx, yui_obj, "perf", perf_obj);
    printf("JS(QuickJS): Registered YUI.perf API\n");
}
