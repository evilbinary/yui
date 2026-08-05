// mqjs_shim.c
// mquickjs 与 QuickJS 的 API 差异补齐（见 lib/mquickjs/README.md）：
// mquickjs 使用内存池 + 追踪 GC，无引用计数，JSValue 无需显式释放。

#include <stddef.h>

#include "mquickjs.h"

// JS_FreeValue 在标准 QuickJS 中用于引用计数释放；mquickjs 使用
// 追踪 + 压缩 GC 且不依赖 C 库 malloc/free，值由 GC 统一回收，
// 无需（也无法）显式释放。此空实现仅为 API 兼容存根，见 lib/mquickjs/README.md。
void JS_FreeValue(JSContext *ctx, JSValue val)
{
    (void)ctx;
    (void)val;
}

int JS_ToBool(JSContext *ctx, JSValue val)
{
    (void)ctx;
    if (JS_IsBool(val)) {
        return JS_VALUE_GET_SPECIAL_VALUE(val) != 0;
    }
    if (JS_IsInt(val)) {
        return JS_VALUE_GET_INT(val) != 0;
    }
    return !JS_IsNull(val) && !JS_IsUndefined(val);
}

void JS_FreeCString(JSContext *ctx, JSCStringBuf *buf)
{
    (void)ctx;
    (void)buf;
}

JSValue JS_NewNumber(JSContext *ctx, double d)
{
    return JS_NewFloat64(ctx, d);
}

int JS_IsObject(JSContext *ctx, JSValue v)
{
    return JS_GetClassID(ctx, v) >= 0;
}
