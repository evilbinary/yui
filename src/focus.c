/*
 * 焦点与空间导航。
 *
 * 方向键在可见、可聚焦的图层之间做二维空间移动；Enter/Space 激活焦点层。
 * 与 focused_layer 全局保持同步，供既有组件（Input/Text）复用。
 */
#include "focus.h"
#include "layer.h"
#include "layer_update.h"
#include "layer_lifecycle.h"
#include "layout.h"
#include "theme_manager.h"
#include "component_registry.h"

#include <stdlib.h>

#define FOCUS_SCOPE_MAX 16

static Layer* g_focus = NULL;
static Layer* g_scopes[FOCUS_SCOPE_MAX];
static Layer* g_scope_saved[FOCUS_SCOPE_MAX];
static int g_scope_count = 0;

#define FOCUS_MEMORY_MAX 16
static Layer* g_memory_page[FOCUS_MEMORY_MAX];
static Layer* g_memory_focus[FOCUS_MEMORY_MAX];
static int g_memory_count = 0;

typedef struct {
    Layer* cur;
    Layer* scope;
    FocusDirection dir;
    Layer* best;
    long best_score;
    long best_primary;
    long best_secondary;
    int cur_cx;
    int cur_cy;
} FocusSearch;

static long focus_abs_long(long v)
{
    return v < 0 ? -v : v;
}

static void focus_memory_set(Layer* page, Layer* focus)
{
    int i;
    if (!page) return;
    for (i = 0; i < g_memory_count; i++) {
        if (g_memory_page[i] == page) {
            g_memory_focus[i] = focus;
            return;
        }
    }
    if (g_memory_count < FOCUS_MEMORY_MAX) {
        g_memory_page[g_memory_count] = page;
        g_memory_focus[g_memory_count] = focus;
        g_memory_count++;
    }
}

static Layer* focus_memory_get(Layer* page)
{
    int i;
    if (!page) return NULL;
    for (i = 0; i < g_memory_count; i++) {
        if (g_memory_page[i] == page) {
            return g_memory_focus[i];
        }
    }
    return NULL;
}

static void focus_memory_clear_layer(Layer* layer)
{
    int i;
    for (i = 0; i < g_memory_count; i++) {
        if (g_memory_page[i] == layer || g_memory_focus[i] == layer) {
            g_memory_page[i] = g_memory_page[g_memory_count - 1];
            g_memory_focus[i] = g_memory_focus[g_memory_count - 1];
            g_memory_count--;
            return;
        }
    }
}

void focus_init(void)
{
    g_focus = NULL;
    g_scope_count = 0;
    g_memory_count = 0;
}

Layer* focus_get(void)
{
    return g_focus;
}

int focus_is_focusable(const Layer* layer)
{
    if (!layer) return 0;
    if (!layer->focusable) return 0;
    if (layer->state & LAYER_STATE_DISABLED) return 0;
    if (layer->visible == IN_VISIBLE) return 0;
    if (layer->rect.w <= 0 || layer->rect.h <= 0) return 0;
    if (!layer_is_effectively_visible(layer)) return 0;
    return 1;
}

int focus_is_within_scope(const Layer* layer)
{
    const Layer* p;
    if (!layer) return 0;
    if (g_scope_count <= 0) return 1;
    for (p = layer; p; p = p->parent) {
        if (p == g_scopes[g_scope_count - 1]) return 1;
    }
    return 0;
}

static int focus_is_within_subtree(const Layer* layer, const Layer* root)
{
    const Layer* p;
    if (!layer || !root) return 0;
    for (p = layer; p; p = p->parent) {
        if (p == root) return 1;
    }
    return 0;
}

static int focus_has_focusable_ancestor(const Layer* layer, const Layer* scope)
{
    const Layer* p;
    for (p = layer ? layer->parent : NULL; p && p != scope; p = p->parent) {
        /* focusChildren 容器不阻断子元素独立聚焦 */
        if (p->focusable && !p->focus_children) return 1;
    }
    return 0;
}

static int focus_has_focusable_descendant(Layer* layer)
{
    int i;
    if (!layer) return 0;
    for (i = 0; i < layer->child_count; i++) {
        Layer* c = layer->children[i];
        if (!c || c->visible == IN_VISIBLE) continue;
        if (focus_is_focusable(c)) return 1;
        if (focus_has_focusable_descendant(c)) return 1;
    }
    if (layer->sub) {
        return focus_has_focusable_descendant(layer->sub);
    }
    return 0;
}

/* 可聚焦容器 + focusChildren + 有可聚焦子元素 => 分组，焦点下钻 */
static int focus_is_group(const Layer* layer)
{
    return layer && layer->focusable && layer->focus_children &&
           focus_has_focusable_descendant((Layer*)layer);
}

static void focus_apply_state_style(Layer* layer)
{
    if (!layer) return;
    theme_manager_apply_to_layer(layer, layer->id, yui_type_name(layer->type));
}

static void focus_invoke_event(Layer* layer, int is_focus)
{
    EventHandler handler;
    if (!layer || !layer->event) return;
    handler = is_focus ? layer->event->focus : layer->event->blur;
    if (handler) {
        EVENT_INVOKE(handler, layer);
    }
}

int focus_set(Layer* layer)
{
    Layer* old;
    if (layer && layer->state & LAYER_STATE_DISABLED) {
        return 0;
    }
    if (layer == g_focus) {
        return 0;
    }

    old = g_focus;
    if (old) {
        old->state &= ~LAYER_STATE_FOCUSED;
        focus_apply_state_style(old);
        mark_layer_dirty(old, DIRTY_STYLE | DIRTY_COLOR);
        focus_invoke_event(old, 0);
    }

    g_focus = layer;
    focused_layer = layer;

    if (layer) {
        layer->state |= LAYER_STATE_FOCUSED;
        focus_apply_state_style(layer);
        mark_layer_dirty(layer, DIRTY_STYLE | DIRTY_COLOR);
        focus_invoke_event(layer, 1);
        focus_scroll_into_view(layer);
    }
    return 1;
}

void focus_clear(void)
{
    focus_set(NULL);
}

static Layer* focus_find_first(Layer* layer, Layer* scope)
{
    int i;
    if (!layer || layer->visible == IN_VISIBLE) return NULL;
    if (layer != scope && focus_is_focusable(layer) &&
        !focus_has_focusable_ancestor(layer, scope) && !focus_is_group(layer)) {
        return layer;
    }
    for (i = 0; i < layer->child_count; i++) {
        Layer* found = focus_find_first(layer->children[i], scope);
        if (found) return found;
    }
    if (layer->sub) {
        return focus_find_first(layer->sub, scope);
    }
    return NULL;
}

static void focus_eval_candidate(FocusSearch* s, Layer* layer)
{
    Rect* cr = &s->cur->rect;
    Rect* lr = &layer->rect;
    int cx = lr->x + lr->w / 2;
    int cy = lr->y + lr->h / 2;
    long primary;
    long secondary;
    int overlap;
    long score;

    switch (s->dir) {
        case FOCUS_DIR_LEFT:
            if (cx >= s->cur_cx) return;
            primary = s->cur_cx - cx;
            secondary = focus_abs_long(cy - s->cur_cy);
            overlap = (lr->y < cr->y + cr->h) && (lr->y + lr->h > cr->y);
            break;
        case FOCUS_DIR_RIGHT:
            if (cx <= s->cur_cx) return;
            primary = cx - s->cur_cx;
            secondary = focus_abs_long(cy - s->cur_cy);
            overlap = (lr->y < cr->y + cr->h) && (lr->y + lr->h > cr->y);
            break;
        case FOCUS_DIR_UP:
            if (cy >= s->cur_cy) return;
            primary = s->cur_cy - cy;
            secondary = focus_abs_long(cx - s->cur_cx);
            overlap = (lr->x < cr->x + cr->w) && (lr->x + lr->w > cr->x);
            break;
        case FOCUS_DIR_DOWN:
            if (cy <= s->cur_cy) return;
            primary = cy - s->cur_cy;
            secondary = focus_abs_long(cx - s->cur_cx);
            overlap = (lr->x < cr->x + cr->w) && (lr->x + lr->w > cr->x);
            break;
        default:
            return;
    }

    /* 投影有重叠：只看主方向距离（列表/网格按行列走）；无重叠：叠加垂直偏移惩罚 */
    score = overlap ? primary : primary + secondary * 2 + 1000000L;
    if (s->best == NULL || score < s->best_score ||
        (score == s->best_score &&
         (primary < s->best_primary ||
          (primary == s->best_primary && secondary < s->best_secondary)))) {
        s->best = layer;
        s->best_score = score;
        s->best_primary = primary;
        s->best_secondary = secondary;
    }
}

static void focus_search_visit(FocusSearch* s, Layer* layer)
{
    int i;
    if (!layer || layer->visible == IN_VISIBLE) return;

    if (layer == s->cur) {
        /* 当前焦点若是原子容器，其子孙已被排除；分组容器继续下钻 */
        if (focus_is_focusable(layer) && !focus_is_group(layer)) return;
    } else if (focus_is_focusable(layer) &&
               !focus_has_focusable_ancestor(layer, s->scope) &&
               !focus_is_group(layer)) {
        focus_eval_candidate(s, layer);
        return;
    }

    for (i = 0; i < layer->child_count; i++) {
        focus_search_visit(s, layer->children[i]);
    }
    if (layer->sub) {
        focus_search_visit(s, layer->sub);
    }
}

int focus_move(Layer* root, FocusDirection dir)
{
    Layer* scope = g_scope_count > 0 ? g_scopes[g_scope_count - 1] : root;
    Layer* cur = g_focus;
    FocusSearch s;

    if (dir == FOCUS_DIR_NONE || !scope) return 0;

    if (!cur || !focus_is_focusable(cur) || !focus_is_within_scope(cur)) {
        Layer* first = focus_find_first(scope, scope);
        if (first) {
            focus_set(first);
            return 1;
        }
        return 0;
    }

    s.cur = cur;
    s.scope = scope;
    s.dir = dir;
    s.best = NULL;
    s.best_score = 0;
    s.best_primary = 0;
    s.best_secondary = 0;
    s.cur_cx = cur->rect.x + cur->rect.w / 2;
    s.cur_cy = cur->rect.y + cur->rect.h / 2;

    focus_search_visit(&s, scope);

    if (s.best) {
        focus_set(s.best);
        return 1;
    }
    return 0;
}

int focus_activate(void)
{
    if (!g_focus) return 0;
    if (g_focus->state & LAYER_STATE_DISABLED) return 0;
    if (g_focus->event && g_focus->event->click) {
        EVENT_INVOKE(g_focus->event->click, g_focus);
        return 1;
    }
    return 0;
}

int focus_handle_key(Layer* root, KeyEvent* event)
{
    if (!event || event->type != KEY_EVENT_DOWN) return 0;

    switch (event->data.key.key_code) {
        case SDLK_LEFT:
            return focus_move(root, FOCUS_DIR_LEFT);
        case SDLK_RIGHT:
            return focus_move(root, FOCUS_DIR_RIGHT);
        case SDLK_UP:
            return focus_move(root, FOCUS_DIR_UP);
        case SDLK_DOWN:
            return focus_move(root, FOCUS_DIR_DOWN);
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
        case SDLK_SPACE:
            /* Button/Input/List 等自带 handle_key_event，激活由组件在 UP 时触发，
             * 这里只处理纯 View 等无按键处理器的焦点层，避免双触发。 */
            if (g_focus && g_focus->handle_key_event) {
                return 0;
            }
            return focus_activate();
        default:
            return 0;
    }
}

void focus_push_scope(Layer* scope)
{
    if (!scope) return;
    if (g_scope_count >= FOCUS_SCOPE_MAX) return;
    g_scope_saved[g_scope_count] = g_focus;
    g_scopes[g_scope_count++] = scope;
    if (!focus_is_within_scope(g_focus)) {
        Layer* first = focus_find_first(scope, scope);
        if (first) {
            focus_set(first);
        } else {
            focus_set(NULL);
        }
    }
}

void focus_pop_scope(Layer* scope)
{
    int i;
    Layer* saved = NULL;
    if (g_scope_count <= 0) return;
    if (scope) {
        for (i = g_scope_count - 1; i >= 0; i--) {
            if (g_scopes[i] == scope) {
                saved = g_scope_saved[i];
                g_scope_count = i;
                break;
            }
        }
    } else {
        g_scope_count--;
        saved = g_scope_saved[g_scope_count];
    }
    if (saved && focus_is_focusable(saved) && focus_is_within_scope(saved)) {
        focus_set(saved);
    } else if (!focus_is_within_scope(g_focus)) {
        focus_set(NULL);
    }
}

void focus_scroll_into_view(Layer* layer)
{
    Layer* p;
    if (!layer) return;
    if (getenv("YUI_NO_FOCUS_SCROLL")) return;
    for (p = layer->parent; p; p = p->parent) {
        int top;
        int bottom;
        if (p->scrollable != 1 && p->scrollable != 3) continue;
        if (p->content_height <= p->rect.h) continue;

        top = p->rect.y;
        bottom = p->rect.y + p->rect.h;
        /* 正数内容下移，负数内容上移（与 scroll_offset 反向） */
        if (layer->rect.y < top) {
            layout_scroll_vertical(p, top - layer->rect.y);
        } else if (layer->rect.y + layer->rect.h > bottom) {
            layout_scroll_vertical(p, -((layer->rect.y + layer->rect.h) - bottom));
        }
    }
}

void focus_on_layer_destroy(Layer* layer)
{
    int i;
    if (!layer) return;
    if (g_focus == layer) {
        /* 图层正在销毁，直接断开引用，不做主题回滚 */
        g_focus = NULL;
        focused_layer = NULL;
    }
    for (i = 0; i < g_scope_count; i++) {
        if (g_scopes[i] == layer) {
            g_scope_count = i;
            break;
        }
    }
    focus_memory_clear_layer(layer);
}

void focus_on_layer_show(Layer* layer)
{
    Layer* remembered;
    int is_page;
    if (!layer) return;

    /* 只有路由页（声明了 onShow）显示时才初始化焦点；
     * 普通元素显示不改焦点（动态网格/列表逐个 show 时布局尚未稳定，
     * 此时聚焦会因临时尺寸误触发滚动）。 */
    is_page = (layer->lifecycle_flags & LIFECYCLE_ON_SHOW) != 0;
    if (!is_page) {
        return;
    }
    if (g_focus && focus_is_focusable(g_focus) && focus_is_within_scope(g_focus)) {
        if (focus_is_within_subtree(g_focus, layer)) {
            return;
        }
    }
    /* 优先恢复该页上次焦点 */
    remembered = focus_memory_get(layer);
    if (remembered && focus_is_focusable(remembered) &&
        focus_is_within_subtree(remembered, layer)) {
        focus_set(remembered);
        return;
    }
    {
        Layer* first = focus_find_first(layer, layer);
        if (first) {
            focus_set(first);
        }
    }
}

void focus_on_layer_hide(Layer* layer)
{
    if (!layer) return;
    if (g_focus) {
        Layer* p;
        for (p = g_focus; p; p = p->parent) {
            if (p == layer) {
                focus_memory_set(layer, g_focus);
                break;
            }
        }
    }
}
