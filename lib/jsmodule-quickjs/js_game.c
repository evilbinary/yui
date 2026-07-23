#include "js_game.h"
#include "../../src/game/game.h"

#if YUI_WITH_GAME

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

static JSContext* g_game_ctx;

static JSValue game_entity_to_js(JSContext* ctx, GameEntity* e)
{
    JSValue obj;
    if (!e) {
        return JS_NULL;
    }
    obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, obj, "id", JS_NewString(ctx, e->id));
    JS_SetPropertyStr(ctx, obj, "tag", JS_NewString(ctx, e->tag));
    JS_SetPropertyStr(ctx, obj, "script", JS_NewString(ctx, e->script));
    JS_SetPropertyStr(ctx, obj, "x", JS_NewFloat64(ctx, e->x));
    JS_SetPropertyStr(ctx, obj, "y", JS_NewFloat64(ctx, e->y));
    JS_SetPropertyStr(ctx, obj, "z", JS_NewFloat64(ctx, e->z));
    JS_SetPropertyStr(ctx, obj, "vx", JS_NewFloat64(ctx, e->vx));
    JS_SetPropertyStr(ctx, obj, "vy", JS_NewFloat64(ctx, e->vy));
    JS_SetPropertyStr(ctx, obj, "w", JS_NewFloat64(ctx, e->w));
    JS_SetPropertyStr(ctx, obj, "h", JS_NewFloat64(ctx, e->h));
    JS_SetPropertyStr(ctx, obj, "grounded", JS_NewBool(ctx, e->grounded));
    JS_SetPropertyStr(ctx, obj, "solid", JS_NewBool(ctx, e->solid));
    JS_SetPropertyStr(ctx, obj, "trigger", JS_NewBool(ctx, e->trigger));
    JS_SetPropertyStr(ctx, obj, "__ptr", JS_NewInt64(ctx, (int64_t)(uintptr_t)e));
    return obj;
}

static GameEntity* game_entity_from_js(JSContext* ctx, JSValueConst val)
{
    JSValue ptr_val;
    int64_t ptr = 0;
    if (!JS_IsObject(val)) {
        return NULL;
    }
    ptr_val = JS_GetPropertyStr(ctx, val, "__ptr");
    if (JS_ToInt64(ctx, &ptr, ptr_val) != 0) {
        JS_FreeValue(ctx, ptr_val);
        return NULL;
    }
    JS_FreeValue(ctx, ptr_val);
    return (GameEntity*)(uintptr_t)ptr;
}

static void game_entity_apply_js(JSContext* ctx, GameEntity* e, JSValueConst val)
{
    JSValue v;
    double d;
    if (!e || !JS_IsObject(val)) {
        return;
    }
    v = JS_GetPropertyStr(ctx, val, "x");
    if (JS_IsNumber(v) && JS_ToFloat64(ctx, &d, v) == 0) e->x = (float)d;
    JS_FreeValue(ctx, v);
    v = JS_GetPropertyStr(ctx, val, "y");
    if (JS_IsNumber(v) && JS_ToFloat64(ctx, &d, v) == 0) e->y = (float)d;
    JS_FreeValue(ctx, v);
    v = JS_GetPropertyStr(ctx, val, "z");
    if (JS_IsNumber(v) && JS_ToFloat64(ctx, &d, v) == 0) e->z = (float)d;
    JS_FreeValue(ctx, v);
    v = JS_GetPropertyStr(ctx, val, "vx");
    if (JS_IsNumber(v) && JS_ToFloat64(ctx, &d, v) == 0) e->vx = (float)d;
    JS_FreeValue(ctx, v);
    v = JS_GetPropertyStr(ctx, val, "vy");
    if (JS_IsNumber(v) && JS_ToFloat64(ctx, &d, v) == 0) e->vy = (float)d;
    JS_FreeValue(ctx, v);
    v = JS_GetPropertyStr(ctx, val, "w");
    if (JS_IsNumber(v) && JS_ToFloat64(ctx, &d, v) == 0) e->w = (float)d;
    JS_FreeValue(ctx, v);
    v = JS_GetPropertyStr(ctx, val, "h");
    if (JS_IsNumber(v) && JS_ToFloat64(ctx, &d, v) == 0) e->h = (float)d;
    JS_FreeValue(ctx, v);
}

static void js_game_script_update(GameEntity* entity, float dt)
{
    JSValue global;
    JSValue fn;
    JSValue ent;
    JSValue args[2];
    JSValue ret;
    if (!g_game_ctx || !entity || !entity->script[0]) {
        return;
    }
    global = JS_GetGlobalObject(g_game_ctx);
    fn = JS_GetPropertyStr(g_game_ctx, global, entity->script);
    if (!JS_IsFunction(g_game_ctx, fn)) {
        JS_FreeValue(g_game_ctx, fn);
        JS_FreeValue(g_game_ctx, global);
        return;
    }
    ent = game_entity_to_js(g_game_ctx, entity);
    args[0] = ent;
    args[1] = JS_NewFloat64(g_game_ctx, dt);
    ret = JS_Call(g_game_ctx, fn, JS_UNDEFINED, 2, args);
    if (JS_IsException(ret)) {
        JSValue exc = JS_GetException(g_game_ctx);
        JS_FreeValue(g_game_ctx, exc);
    } else {
        game_entity_apply_js(g_game_ctx, entity, ent);
    }
    JS_FreeValue(g_game_ctx, ret);
    JS_FreeValue(g_game_ctx, args[1]);
    JS_FreeValue(g_game_ctx, ent);
    JS_FreeValue(g_game_ctx, fn);
    JS_FreeValue(g_game_ctx, global);
}

static JSValue js_game_load_scene(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv)
{
    const char* path;
    int ok;
    (void)this_val;
    if (argc < 1) {
        return JS_ThrowTypeError(ctx, "Game.loadScene(path)");
    }
    path = JS_ToCString(ctx, argv[0]);
    if (!path) {
        return JS_EXCEPTION;
    }
    ok = game_load_scene_json(path);
    JS_FreeCString(ctx, path);
    return JS_NewBool(ctx, ok);
}

static JSValue js_game_clear_scene(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv)
{
    (void)ctx; (void)this_val; (void)argc; (void)argv;
    game_clear_scene();
    return JS_NewBool(ctx, 1);
}

static JSValue js_game_spawn(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv)
{
    GameEntity* e;
    cJSON* json = NULL;
    (void)this_val;
    if (argc < 1) {
        return JS_ThrowTypeError(ctx, "Game.spawn(desc)");
    }
    if (JS_IsString(argv[0])) {
        const char* id = JS_ToCString(ctx, argv[0]);
        e = game_spawn(id);
        if (id) JS_FreeCString(ctx, id);
    } else if (JS_IsObject(argv[0])) {
        JSValue global = JS_GetGlobalObject(ctx);
        JSValue json_obj = JS_GetPropertyStr(ctx, global, "JSON");
        JSValue stringify = JS_GetPropertyStr(ctx, json_obj, "stringify");
        JSValue str_val = JS_Call(ctx, stringify, json_obj, 1, &argv[0]);
        const char* s = JS_ToCString(ctx, str_val);
        if (s) {
            json = cJSON_Parse(s);
            JS_FreeCString(ctx, s);
        }
        JS_FreeValue(ctx, str_val);
        JS_FreeValue(ctx, stringify);
        JS_FreeValue(ctx, json_obj);
        JS_FreeValue(ctx, global);
        e = game_spawn_from_json(json);
        cJSON_Delete(json);
    } else {
        return JS_ThrowTypeError(ctx, "Game.spawn expects object or id");
    }
    return game_entity_to_js(ctx, e);
}

static JSValue js_game_destroy(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv)
{
    (void)this_val;
    if (argc < 1) {
        return JS_NewBool(ctx, 0);
    }
    if (JS_IsString(argv[0])) {
        const char* id = JS_ToCString(ctx, argv[0]);
        game_destroy_by_id(id);
        if (id) JS_FreeCString(ctx, id);
    } else {
        GameEntity* e = game_entity_from_js(ctx, argv[0]);
        game_destroy(e);
    }
    return JS_NewBool(ctx, 1);
}

static JSValue js_game_find(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv)
{
    const char* id;
    GameEntity* e;
    (void)this_val;
    if (argc < 1) {
        return JS_NULL;
    }
    id = JS_ToCString(ctx, argv[0]);
    e = game_find(id);
    if (id) JS_FreeCString(ctx, id);
    return game_entity_to_js(ctx, e);
}

static JSValue js_game_find_by_tag(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv)
{
    const char* tag;
    GameEntity* e;
    (void)this_val;
    if (argc < 1) {
        return JS_NULL;
    }
    tag = JS_ToCString(ctx, argv[0]);
    e = game_find_by_tag(tag);
    if (tag) JS_FreeCString(ctx, tag);
    return game_entity_to_js(ctx, e);
}

static JSValue js_game_overlaps(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv)
{
    GameEntity* a;
    GameEntity* b;
    (void)this_val;
    if (argc < 2) {
        return JS_NewBool(ctx, 0);
    }
    a = game_entity_from_js(ctx, argv[0]);
    b = game_entity_from_js(ctx, argv[1]);
    return JS_NewBool(ctx, game_entities_overlap(a, b));
}

static JSValue js_game_input_down(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv)
{
    const char* name;
    int ok;
    (void)this_val;
    if (argc < 1) return JS_NewBool(ctx, 0);
    name = JS_ToCString(ctx, argv[0]);
    ok = game_input_down(name);
    if (name) JS_FreeCString(ctx, name);
    return JS_NewBool(ctx, ok);
}

static JSValue js_game_input_pressed(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv)
{
    const char* name;
    int ok;
    (void)this_val;
    if (argc < 1) return JS_NewBool(ctx, 0);
    name = JS_ToCString(ctx, argv[0]);
    ok = game_input_pressed(name);
    if (name) JS_FreeCString(ctx, name);
    return JS_NewBool(ctx, ok);
}

static JSValue js_game_input_axis(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv)
{
    const char* name;
    float v;
    (void)this_val;
    if (argc < 1) return JS_NewFloat64(ctx, 0);
    name = JS_ToCString(ctx, argv[0]);
    v = game_input_axis(name);
    if (name) JS_FreeCString(ctx, name);
    return JS_NewFloat64(ctx, v);
}

static JSValue js_game_input_pointer(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv)
{
    int x = 0, y = 0;
    JSValue obj;
    (void)this_val; (void)argc; (void)argv;
    game_input_pointer(&x, &y);
    obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, obj, "x", JS_NewInt32(ctx, x));
    JS_SetPropertyStr(ctx, obj, "y", JS_NewInt32(ctx, y));
    return obj;
}

static JSValue js_game_input_mouse_down(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv)
{
    int button = 1;
    (void)this_val;
    if (argc >= 1) {
        JS_ToInt32(ctx, &button, argv[0]);
    }
    return JS_NewBool(ctx, game_input_mouse_down(button));
}

static JSValue js_game_input_mouse_pressed(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv)
{
    int button = 1;
    (void)this_val;
    if (argc >= 1) {
        JS_ToInt32(ctx, &button, argv[0]);
    }
    return JS_NewBool(ctx, game_input_mouse_pressed(button));
}

static JSValue js_game_get_dt(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv)
{
    (void)this_val; (void)argc; (void)argv;
    return JS_NewFloat64(ctx, game_time_dt());
}

static JSValue js_game_camera_follow(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv)
{
    const char* id;
    (void)this_val;
    if (argc < 1) return JS_NewBool(ctx, 0);
    id = JS_ToCString(ctx, argv[0]);
    game_camera_follow(id);
    if (id) JS_FreeCString(ctx, id);
    return JS_NewBool(ctx, 1);
}

static JSValue js_game_camera_set(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv)
{
    double x = 0, y = 0;
    (void)this_val;
    if (argc >= 1) JS_ToFloat64(ctx, &x, argv[0]);
    if (argc >= 2) JS_ToFloat64(ctx, &y, argv[1]);
    game_camera_set((float)x, (float)y);
    return JS_NewBool(ctx, 1);
}

static JSValue js_game_world_to_screen(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv)
{
    double wx = 0, wy = 0;
    float sx = 0, sy = 0;
    JSValue obj;
    GameCamera* cam = game_camera();
    (void)this_val;
    if (argc >= 1) JS_ToFloat64(ctx, &wx, argv[0]);
    if (argc >= 2) JS_ToFloat64(ctx, &wy, argv[1]);
    if (cam) {
        sx = (float)wx - cam->x;
        sy = (float)wy - cam->y;
    }
    obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, obj, "x", JS_NewFloat64(ctx, sx));
    JS_SetPropertyStr(ctx, obj, "y", JS_NewFloat64(ctx, sy));
    return obj;
}

void js_game_register_api(JSContext* ctx)
{
    JSValue global;
    JSValue game_obj;
    JSValue input_obj;
    JSValue time_obj;
    JSValue camera_obj;

    if (!ctx) {
        return;
    }
    g_game_ctx = ctx;
    game_init();
    game_set_script_update_fn(js_game_script_update);

    global = JS_GetGlobalObject(ctx);
    game_obj = JS_NewObject(ctx);

    JS_SetPropertyStr(ctx, game_obj, "loadScene", JS_NewCFunction(ctx, js_game_load_scene, "loadScene", 1));
    JS_SetPropertyStr(ctx, game_obj, "clearScene", JS_NewCFunction(ctx, js_game_clear_scene, "clearScene", 0));
    JS_SetPropertyStr(ctx, game_obj, "spawn", JS_NewCFunction(ctx, js_game_spawn, "spawn", 1));
    JS_SetPropertyStr(ctx, game_obj, "destroy", JS_NewCFunction(ctx, js_game_destroy, "destroy", 1));
    JS_SetPropertyStr(ctx, game_obj, "find", JS_NewCFunction(ctx, js_game_find, "find", 1));
    JS_SetPropertyStr(ctx, game_obj, "findByTag", JS_NewCFunction(ctx, js_game_find_by_tag, "findByTag", 1));
    JS_SetPropertyStr(ctx, game_obj, "overlaps", JS_NewCFunction(ctx, js_game_overlaps, "overlaps", 2));

    input_obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, input_obj, "down", JS_NewCFunction(ctx, js_game_input_down, "down", 1));
    JS_SetPropertyStr(ctx, input_obj, "pressed", JS_NewCFunction(ctx, js_game_input_pressed, "pressed", 1));
    JS_SetPropertyStr(ctx, input_obj, "axis", JS_NewCFunction(ctx, js_game_input_axis, "axis", 1));
    JS_SetPropertyStr(ctx, input_obj, "pointer", JS_NewCFunction(ctx, js_game_input_pointer, "pointer", 0));
    JS_SetPropertyStr(ctx, input_obj, "mouseDown", JS_NewCFunction(ctx, js_game_input_mouse_down, "mouseDown", 1));
    JS_SetPropertyStr(ctx, input_obj, "mousePressed", JS_NewCFunction(ctx, js_game_input_mouse_pressed, "mousePressed", 1));
    JS_SetPropertyStr(ctx, game_obj, "input", input_obj);

    time_obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, time_obj, "dt", JS_NewCFunction(ctx, js_game_get_dt, "dt", 0));
    /* Also allow Game.time.dt as property via getter-less method call Game.time.dt() — add alias getDt */
    JS_SetPropertyStr(ctx, time_obj, "getDt", JS_NewCFunction(ctx, js_game_get_dt, "getDt", 0));
    JS_SetPropertyStr(ctx, game_obj, "time", time_obj);

    camera_obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, camera_obj, "follow", JS_NewCFunction(ctx, js_game_camera_follow, "follow", 1));
    JS_SetPropertyStr(ctx, camera_obj, "set", JS_NewCFunction(ctx, js_game_camera_set, "set", 2));
    JS_SetPropertyStr(ctx, camera_obj, "worldToScreen", JS_NewCFunction(ctx, js_game_world_to_screen, "worldToScreen", 2));
    JS_SetPropertyStr(ctx, game_obj, "camera", camera_obj);

    JS_SetPropertyStr(ctx, global, "Game", game_obj);
    JS_FreeValue(ctx, global);
    printf("JS(QuickJS): Registered Game API\n");
}

void js_game_set_context(JSContext* ctx)
{
    g_game_ctx = ctx;
}

void js_game_shutdown(void)
{
    if (!g_game_ctx) {
        game_shutdown();
        return;
    }
    JSValue global = JS_GetGlobalObject(g_game_ctx);
    JS_SetPropertyStr(g_game_ctx, global, "Game", JS_UNDEFINED);
    JS_FreeValue(g_game_ctx, global);
    g_game_ctx = NULL;
    game_shutdown();
}

#else /* !YUI_WITH_GAME */

void js_game_register_api(JSContext* ctx)
{
    (void)ctx;
}

void js_game_set_context(JSContext* ctx)
{
    (void)ctx;
}

void js_game_shutdown(void)
{
}

#endif
