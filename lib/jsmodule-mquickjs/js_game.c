#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "js_module.h"
#include "mquickjs.h"
#include "mquickjs_build.h"
#include "../../lib/cjson/cJSON.h"
#include "../../src/game/game.h"

#if YUI_WITH_GAME

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
    JS_SetPropertyStr(ctx, obj, "grounded", JS_NewBool(e->grounded));
    JS_SetPropertyStr(ctx, obj, "solid", JS_NewBool(e->solid));
    JS_SetPropertyStr(ctx, obj, "trigger", JS_NewBool(e->trigger));
    JS_SetPropertyStr(ctx, obj, "__ptr", JS_NewInt64(ctx, (int64_t)(uintptr_t)e));
    return obj;
}

static GameEntity* game_entity_from_js(JSContext* ctx, JSValue val)
{
    JSValue ptr_val;
    double ptr = 0;
    if (!JS_IsObject(ctx, val)) {
        return NULL;
    }
    ptr_val = JS_GetPropertyStr(ctx, val, "__ptr");
    if (JS_IsException(ptr_val)) {
        return NULL;
    }
    if (JS_IsNull(ptr_val) || JS_IsUndefined(ptr_val)) {
        return NULL;
    }
    if (JS_ToNumber(ctx, &ptr, ptr_val) != 0) {
        return NULL;
    }
    return (GameEntity*)(uintptr_t)ptr;
}

static void game_entity_apply_js(JSContext* ctx, GameEntity* e, JSValue val)
{
    JSValue v;
    double d;
    if (!e || !JS_IsObject(ctx, val)) {
        return;
    }
    v = JS_GetPropertyStr(ctx, val, "x");
    if (JS_IsNumber(ctx, v) && JS_ToNumber(ctx, &d, v) == 0) e->x = (float)d;
    v = JS_GetPropertyStr(ctx, val, "y");
    if (JS_IsNumber(ctx, v) && JS_ToNumber(ctx, &d, v) == 0) e->y = (float)d;
    v = JS_GetPropertyStr(ctx, val, "z");
    if (JS_IsNumber(ctx, v) && JS_ToNumber(ctx, &d, v) == 0) e->z = (float)d;
    v = JS_GetPropertyStr(ctx, val, "vx");
    if (JS_IsNumber(ctx, v) && JS_ToNumber(ctx, &d, v) == 0) e->vx = (float)d;
    v = JS_GetPropertyStr(ctx, val, "vy");
    if (JS_IsNumber(ctx, v) && JS_ToNumber(ctx, &d, v) == 0) e->vy = (float)d;
    /* Do not write w/h: scripts rarely resize, and mid-reload stale apply
     * used to clobber platforms with the player's 32x48. */
}

static void js_game_script_update(GameEntity* entity, float dt)
{
    JSValue global;
    JSValue fn;
    JSValue ent;
    JSValue dt_val;
    JSValue ret;
    int scene_gen;
    if (!g_game_ctx || !entity || !entity->script[0]) {
        return;
    }
    scene_gen = game_scene_generation();
    global = JS_GetGlobalObject(g_game_ctx);
    fn = JS_GetPropertyStr(g_game_ctx, global, entity->script);
    if (!JS_IsFunction(g_game_ctx, fn)) {
        return;
    }
    ent = game_entity_to_js(g_game_ctx, entity);
    dt_val = JS_NewFloat64(g_game_ctx, dt);
    if (JS_StackCheck(g_game_ctx, 4)) {
        return;
    }
    JS_PushArg(g_game_ctx, dt_val);
    JS_PushArg(g_game_ctx, ent);
    JS_PushArg(g_game_ctx, fn);
    JS_PushArg(g_game_ctx, JS_NULL);
    ret = JS_Call(g_game_ctx, 2);
    if (JS_IsException(ret)) {
        JSValue exc = JS_GetException(g_game_ctx);
        printf("JS(Game): script %s error: ", entity->script);
        JS_PrintValueF(g_game_ctx, exc, JS_DUMP_LONG);
        printf("\n");
    } else if (game_scene_generation() == scene_gen && entity->alive) {
        /* Scene reload mid-script: do not apply stale JS state onto reused slots. */
        game_entity_apply_js(g_game_ctx, entity, ent);
    }
}

static JSValue js_game_load_scene(JSContext* ctx, JSValue* this_val, int argc, JSValue* argv)
{
    const char* path;
    int ok;
    JSCStringBuf cstr;
    (void)this_val;
    if (argc < 1) {
        return JS_ThrowTypeError(ctx, "Game.loadScene(path)");
    }
    path = JS_ToCString(ctx, argv[0], &cstr);
    if (!path) {
        return JS_EXCEPTION;
    }
    ok = game_load_scene_json(path);
    return JS_NewBool(ok);
}

static JSValue js_game_clear_scene(JSContext* ctx, JSValue* this_val, int argc, JSValue* argv)
{
    (void)ctx; (void)this_val; (void)argc; (void)argv;
    game_clear_scene();
    return JS_NewBool(1);
}

static JSValue js_game_spawn(JSContext* ctx, JSValue* this_val, int argc, JSValue* argv)
{
    GameEntity* e;
    cJSON* json = NULL;
    (void)this_val;
    if (argc < 1) {
        return JS_ThrowTypeError(ctx, "Game.spawn(desc)");
    }
    if (JS_IsString(ctx, argv[0])) {
        const char* id;
        JSCStringBuf cstr;
        id = JS_ToCString(ctx, argv[0], &cstr);
        e = game_spawn(id);
    } else if (JS_IsObject(ctx, argv[0])) {
        JSValue global = JS_GetGlobalObject(ctx);
        JSValue json_obj = JS_GetPropertyStr(ctx, global, "JSON");
        JSValue stringify = JS_GetPropertyStr(ctx, json_obj, "stringify");
        JSValue str_val;
        const char* s;
        JSCStringBuf cstr;
        if (JS_StackCheck(ctx, 3)) {
            return JS_NewBool(0);
        }
        JS_PushArg(ctx, argv[0]);
        JS_PushArg(ctx, stringify);
        JS_PushArg(ctx, json_obj);
        str_val = JS_Call(ctx, 1);
        s = JS_ToCString(ctx, str_val, &cstr);
        if (s) {
            json = cJSON_Parse(s);
        }
        e = game_spawn_from_json(json);
        cJSON_Delete(json);
    } else {
        return JS_ThrowTypeError(ctx, "Game.spawn expects object or id");
    }
    return game_entity_to_js(ctx, e);
}

static JSValue js_game_destroy(JSContext* ctx, JSValue* this_val, int argc, JSValue* argv)
{
    (void)this_val;
    if (argc < 1) {
        return JS_NewBool(0);
    }
    if (JS_IsString(ctx, argv[0])) {
        const char* id;
        JSCStringBuf cstr;
        id = JS_ToCString(ctx, argv[0], &cstr);
        game_destroy_by_id(id);
    } else {
        GameEntity* e = game_entity_from_js(ctx, argv[0]);
        game_destroy(e);
    }
    return JS_NewBool(1);
}

static JSValue js_game_find(JSContext* ctx, JSValue* this_val, int argc, JSValue* argv)
{
    const char* id;
    GameEntity* e;
    JSCStringBuf cstr;
    (void)this_val;
    if (argc < 1) {
        return JS_NULL;
    }
    id = JS_ToCString(ctx, argv[0], &cstr);
    e = game_find(id);
    return game_entity_to_js(ctx, e);
}

static JSValue js_game_find_by_tag(JSContext* ctx, JSValue* this_val, int argc, JSValue* argv)
{
    const char* tag;
    GameEntity* e;
    JSCStringBuf cstr;
    (void)this_val;
    if (argc < 1) {
        return JS_NULL;
    }
    tag = JS_ToCString(ctx, argv[0], &cstr);
    e = game_find_by_tag(tag);
    return game_entity_to_js(ctx, e);
}

static JSValue js_game_overlaps(JSContext* ctx, JSValue* this_val, int argc, JSValue* argv)
{
    GameEntity* a;
    GameEntity* b;
    (void)this_val;
    if (argc < 2) {
        return JS_NewBool(0);
    }
    a = game_entity_from_js(ctx, argv[0]);
    b = game_entity_from_js(ctx, argv[1]);
    return JS_NewBool(game_entities_overlap(a, b));
}

static JSValue js_game_input_down(JSContext* ctx, JSValue* this_val, int argc, JSValue* argv)
{
    const char* name;
    int ok;
    JSCStringBuf cstr;
    (void)this_val;
    if (argc < 1) return JS_NewBool(0);
    name = JS_ToCString(ctx, argv[0], &cstr);
    ok = game_input_down(name);
    return JS_NewBool(ok);
}

static JSValue js_game_input_pressed(JSContext* ctx, JSValue* this_val, int argc, JSValue* argv)
{
    const char* name;
    int ok;
    JSCStringBuf cstr;
    (void)this_val;
    if (argc < 1) return JS_NewBool(0);
    name = JS_ToCString(ctx, argv[0], &cstr);
    ok = game_input_pressed(name);
    return JS_NewBool(ok);
}

static JSValue js_game_input_axis(JSContext* ctx, JSValue* this_val, int argc, JSValue* argv)
{
    const char* name;
    float v;
    JSCStringBuf cstr;
    (void)this_val;
    if (argc < 1) return JS_NewFloat64(ctx, 0);
    name = JS_ToCString(ctx, argv[0], &cstr);
    v = game_input_axis(name);
    return JS_NewFloat64(ctx, v);
}

static JSValue js_game_input_pointer(JSContext* ctx, JSValue* this_val, int argc, JSValue* argv)
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

static JSValue js_game_input_mouse_down(JSContext* ctx, JSValue* this_val, int argc, JSValue* argv)
{
    int button = 1;
    (void)this_val;
    if (argc >= 1) {
        JS_ToInt32(ctx, &button, argv[0]);
    }
    return JS_NewBool(game_input_mouse_down(button));
}

static JSValue js_game_input_mouse_pressed(JSContext* ctx, JSValue* this_val, int argc, JSValue* argv)
{
    int button = 1;
    (void)this_val;
    if (argc >= 1) {
        JS_ToInt32(ctx, &button, argv[0]);
    }
    return JS_NewBool(game_input_mouse_pressed(button));
}

static JSValue js_game_camera_follow(JSContext* ctx, JSValue* this_val, int argc, JSValue* argv)
{
    const char* id;
    JSCStringBuf cstr;
    (void)this_val;
    if (argc < 1) return JS_NewBool(0);
    id = JS_ToCString(ctx, argv[0], &cstr);
    game_camera_follow(id);
    return JS_NewBool(1);
}

static JSValue js_game_camera_set(JSContext* ctx, JSValue* this_val, int argc, JSValue* argv)
{
    double x = 0, y = 0;
    (void)this_val;
    if (argc >= 1) JS_ToNumber(ctx, &x, argv[0]);
    if (argc >= 2) JS_ToNumber(ctx, &y, argv[1]);
    game_camera_set((float)x, (float)y);
    return JS_NewBool(1);
}

static JSValue js_game_world_to_screen(JSContext* ctx, JSValue* this_val, int argc, JSValue* argv)
{
    double wx = 0, wy = 0;
    float sx = 0, sy = 0;
    JSValue obj;
    GameCamera* cam = game_camera();
    (void)this_val;
    if (argc >= 1) JS_ToNumber(ctx, &wx, argv[0]);
    if (argc >= 2) JS_ToNumber(ctx, &wy, argv[1]);
    if (cam) {
        sx = (float)wx - cam->x;
        sy = (float)wy - cam->y;
    }
    obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, obj, "x", JS_NewFloat64(ctx, sx));
    JS_SetPropertyStr(ctx, obj, "y", JS_NewFloat64(ctx, sy));
    return obj;
}

static void js_game_on_trigger(GameEntity* a, GameEntity* b, GameTriggerPhase phase)
{
    JSValue global;
    JSValue fn;
    JSValue a_val, b_val, phase_val;
    JSValue ret;
    const char* phase_name = "stay";
    if (!g_game_ctx) {
        return;
    }
    if (phase == GAME_TRIGGER_ENTER) phase_name = "enter";
    else if (phase == GAME_TRIGGER_EXIT) phase_name = "exit";
    global = JS_GetGlobalObject(g_game_ctx);
    fn = JS_GetPropertyStr(g_game_ctx, global, "onTrigger");
    if (!JS_IsFunction(g_game_ctx, fn)) {
        JSValue game = JS_GetPropertyStr(g_game_ctx, global, "Game");
        fn = JS_GetPropertyStr(g_game_ctx, game, "onTrigger");
    }
    if (!JS_IsFunction(g_game_ctx, fn)) {
        return;
    }
    a_val = game_entity_to_js(g_game_ctx, a);
    b_val = game_entity_to_js(g_game_ctx, b);
    phase_val = JS_NewString(g_game_ctx, phase_name);
    if (JS_StackCheck(g_game_ctx, 5)) {
        return;
    }
    JS_PushArg(g_game_ctx, phase_val);
    JS_PushArg(g_game_ctx, b_val);
    JS_PushArg(g_game_ctx, a_val);
    JS_PushArg(g_game_ctx, fn);
    JS_PushArg(g_game_ctx, JS_NULL);
    ret = JS_Call(g_game_ctx, 3);
    if (JS_IsException(ret)) {
        JSValue exc = JS_GetException(g_game_ctx);
        printf("JS(Game): onTrigger error: ");
        JS_PrintValueF(g_game_ctx, exc, JS_DUMP_LONG);
        printf("\n");
    }
}

static JSValue js_game_find_all_by_tag(JSContext* ctx, JSValue* this_val, int argc, JSValue* argv)
{
    const char* tag;
    GameEntity* list[GAME_MAX_ENTITIES];
    int n;
    int i;
    JSValue arr;
    JSCStringBuf cstr;
    (void)this_val;
    if (argc < 1) {
        return JS_NewArray(ctx, 0);
    }
    tag = JS_ToCString(ctx, argv[0], &cstr);
    n = game_find_all_by_tag(tag, list, GAME_MAX_ENTITIES);
    arr = JS_NewArray(ctx, 0);
    for (i = 0; i < n; i++) {
        JS_SetPropertyUint32(ctx, arr, (uint32_t)i, game_entity_to_js(ctx, list[i]));
    }
    return arr;
}

static JSValue js_game_pool_acquire(JSContext* ctx, JSValue* this_val, int argc, JSValue* argv)
{
    const char* prefab = "bullet";
    GameEntity* e;
    JSCStringBuf cstr;
    (void)this_val;
    if (argc >= 1) {
        prefab = JS_ToCString(ctx, argv[0], &cstr);
    }
    e = game_pool_acquire(prefab);
    return game_entity_to_js(ctx, e);
}

static JSValue js_game_pool_release(JSContext* ctx, JSValue* this_val, int argc, JSValue* argv)
{
    (void)this_val;
    if (argc < 1) {
        return JS_NewBool(0);
    }
    game_pool_release(game_entity_from_js(ctx, argv[0]));
    return JS_NewBool(1);
}

static JSValue js_game_play_anim(JSContext* ctx, JSValue* this_val, int argc, JSValue* argv)
{
    GameEntity* e;
    const char* clip;
    int ok;
    JSCStringBuf cstr;
    (void)this_val;
    if (argc < 2) {
        return JS_NewBool(0);
    }
    e = game_entity_from_js(ctx, argv[0]);
    clip = JS_ToCString(ctx, argv[1], &cstr);
    ok = game_play_anim(e, clip);
    return JS_NewBool(ok);
}

static JSValue js_game_audio_play(JSContext* ctx, JSValue* this_val, int argc, JSValue* argv)
{
    const char* path;
    int ok;
    JSCStringBuf cstr;
    (void)this_val;
    if (argc < 1) return JS_NewBool(0);
    path = JS_ToCString(ctx, argv[0], &cstr);
    ok = game_audio_play_sfx(path);
    return JS_NewBool(ok);
}

static JSValue js_game_audio_play_bgm(JSContext* ctx, JSValue* this_val, int argc, JSValue* argv)
{
    const char* path;
    int loop = 1;
    int ok;
    JSCStringBuf cstr;
    (void)this_val;
    if (argc < 1) return JS_NewBool(0);
    path = JS_ToCString(ctx, argv[0], &cstr);
    if (argc >= 2) {
        loop = JS_ToBool(ctx, argv[1]);
    }
    ok = game_audio_play_bgm(path, loop);
    return JS_NewBool(ok);
}

static JSValue js_game_audio_stop_bgm(JSContext* ctx, JSValue* this_val, int argc, JSValue* argv)
{
    (void)ctx; (void)this_val; (void)argc; (void)argv;
    game_audio_stop_bgm();
    return JS_UNDEFINED;
}

static JSValue js_game_spawn_particles(JSContext* ctx, JSValue* this_val, int argc, JSValue* argv)
{
    double x = 0, y = 0, speed = 120, life = 0.45;
    int count = 12;
    Color color = {255, 200, 80, 255};
    JSValue v;
    (void)this_val;
    if (argc < 1 || !JS_IsObject(ctx, argv[0])) {
        return JS_NewInt32(ctx, 0);
    }
    v = JS_GetPropertyStr(ctx, argv[0], "x");
    if (JS_IsNumber(ctx, v)) JS_ToNumber(ctx, &x, v);
    v = JS_GetPropertyStr(ctx, argv[0], "y");
    if (JS_IsNumber(ctx, v)) JS_ToNumber(ctx, &y, v);
    v = JS_GetPropertyStr(ctx, argv[0], "count");
    if (JS_IsNumber(ctx, v)) {
        int32_t c = 12;
        JS_ToInt32(ctx, &c, v);
        count = c;
    }
    v = JS_GetPropertyStr(ctx, argv[0], "speed");
    if (JS_IsNumber(ctx, v)) JS_ToNumber(ctx, &speed, v);
    v = JS_GetPropertyStr(ctx, argv[0], "life");
    if (JS_IsNumber(ctx, v)) JS_ToNumber(ctx, &life, v);
    v = JS_GetPropertyStr(ctx, argv[0], "color");
    if (JS_IsString(ctx, v)) {
        const char* s;
        JSCStringBuf cstr;
        unsigned int rgb = 0;
        s = JS_ToCString(ctx, v, &cstr);
        if (s && s[0] == '#' && strlen(s) >= 7) {
            sscanf(s + 1, "%06x", &rgb);
            color.r = (unsigned char)((rgb >> 16) & 0xff);
            color.g = (unsigned char)((rgb >> 8) & 0xff);
            color.b = (unsigned char)(rgb & 0xff);
        }
    }
    return JS_NewInt32(ctx, game_spawn_particles((float)x, (float)y, count, color, (float)speed, (float)life));
}

static JSValue js_game_debug_set_boxes(JSContext* ctx, JSValue* this_val, int argc, JSValue* argv)
{
    int on = 1;
    (void)this_val;
    if (argc >= 1) {
        on = JS_ToBool(ctx, argv[0]);
    }
    game_debug_set_boxes(on);
    return JS_NewBool(game_debug_boxes_enabled());
}

static JSValue js_game_debug_boxes_enabled(JSContext* ctx, JSValue* this_val, int argc, JSValue* argv)
{
    (void)ctx; (void)this_val; (void)argc; (void)argv;
    return JS_NewBool(game_debug_boxes_enabled());
}

/* ====================== Game 相关的 JS 函数表 ====================== */

static const JSPropDef js_game_input[] = {
    JS_CFUNC_DEF("down", 1, js_game_input_down),
    JS_CFUNC_DEF("pressed", 1, js_game_input_pressed),
    JS_CFUNC_DEF("axis", 1, js_game_input_axis),
    JS_CFUNC_DEF("pointer", 0, js_game_input_pointer),
    JS_CFUNC_DEF("mouseDown", 1, js_game_input_mouse_down),
    JS_CFUNC_DEF("mousePressed", 1, js_game_input_mouse_pressed),
    JS_PROP_END,
};

static const JSClassDef js_game_input_obj =
    JS_OBJECT_DEF("GameInput", js_game_input);

static const JSPropDef js_game_camera[] = {
    JS_CFUNC_DEF("follow", 1, js_game_camera_follow),
    JS_CFUNC_DEF("set", 2, js_game_camera_set),
    JS_CFUNC_DEF("worldToScreen", 2, js_game_world_to_screen),
    JS_PROP_END,
};

static const JSClassDef js_game_camera_obj =
    JS_OBJECT_DEF("GameCamera", js_game_camera);

static const JSPropDef js_game_audio[] = {
    JS_CFUNC_DEF("play", 1, js_game_audio_play),
    JS_CFUNC_DEF("playBgm", 2, js_game_audio_play_bgm),
    JS_CFUNC_DEF("stopBgm", 0, js_game_audio_stop_bgm),
    JS_PROP_END,
};

static const JSClassDef js_game_audio_obj =
    JS_OBJECT_DEF("GameAudio", js_game_audio);

static const JSPropDef js_game_pool[] = {
    JS_CFUNC_DEF("acquire", 1, js_game_pool_acquire),
    JS_CFUNC_DEF("release", 1, js_game_pool_release),
    JS_PROP_END,
};

static const JSClassDef js_game_pool_obj =
    JS_OBJECT_DEF("GamePool", js_game_pool);

static const JSPropDef js_game_debug[] = {
    JS_CFUNC_DEF("setBoxes", 1, js_game_debug_set_boxes),
    JS_CFUNC_DEF("boxes", 0, js_game_debug_boxes_enabled),
    JS_PROP_END,
};

static const JSClassDef js_game_debug_obj =
    JS_OBJECT_DEF("GameDebug", js_game_debug);

static const JSPropDef js_game[] = {
    JS_CFUNC_DEF("loadScene", 1, js_game_load_scene),
    JS_CFUNC_DEF("clearScene", 0, js_game_clear_scene),
    JS_CFUNC_DEF("spawn", 1, js_game_spawn),
    JS_CFUNC_DEF("destroy", 1, js_game_destroy),
    JS_CFUNC_DEF("find", 1, js_game_find),
    JS_CFUNC_DEF("findByTag", 1, js_game_find_by_tag),
    JS_CFUNC_DEF("findAllByTag", 1, js_game_find_all_by_tag),
    JS_CFUNC_DEF("overlaps", 2, js_game_overlaps),
    JS_CFUNC_DEF("playAnim", 2, js_game_play_anim),
    JS_CFUNC_DEF("spawnParticles", 1, js_game_spawn_particles),
    JS_PROP_CLASS_DEF("input", &js_game_input_obj),
    JS_PROP_CLASS_DEF("camera", &js_game_camera_obj),
    JS_PROP_CLASS_DEF("audio", &js_game_audio_obj),
    JS_PROP_CLASS_DEF("pool", &js_game_pool_obj),
    JS_PROP_CLASS_DEF("debug", &js_game_debug_obj),
    JS_PROP_END,
};

static const JSClassDef js_game_class =
    JS_OBJECT_DEF("Game", js_game);

// 注册 Game API 到 JS（导出函数，不使用 static）
void js_module_register_game_api(JSContext* ctx) {
    if (!ctx) return;

    g_game_ctx = ctx;
    game_init();
    game_set_script_update_fn(js_game_script_update);
    game_set_trigger_fn(js_game_on_trigger);

    printf("JS(mquickjs): Registered Game API\n");
}

#else /* !YUI_WITH_GAME */

void js_module_register_game_api(JSContext* ctx) {
    (void)ctx;
}

#endif
