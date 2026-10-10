#ifndef YUI_FOCUS_H
#define YUI_FOCUS_H

#include "ytype.h"

typedef enum {
    FOCUS_DIR_NONE = 0,
    FOCUS_DIR_LEFT,
    FOCUS_DIR_RIGHT,
    FOCUS_DIR_UP,
    FOCUS_DIR_DOWN
} FocusDirection;

void focus_init(void);

Layer* focus_get(void);

/* 设置焦点层；返回 1 表示焦点发生变化。layer 可为 NULL（清空焦点）。 */
int focus_set(Layer* layer);
void focus_clear(void);

/* 在 root（或当前作用域）内按方向做二维空间导航。返回 1 表示焦点移动。 */
int focus_move(Layer* root, FocusDirection dir);

/* 激活当前焦点层（触发 onClick）。返回 1 表示已激活。 */
int focus_activate(void);

/* 按键处理入口：方向键移动焦点，Enter/Space 激活。返回 1 表示已消费。 */
int focus_handle_key(Layer* root, KeyEvent* event);

/* 焦点作用域：Dialog 等弹出层打开时把焦点限制在子树内。 */
void focus_push_scope(Layer* scope);
void focus_pop_scope(Layer* scope);

/* 图层销毁/显示/隐藏时维护焦点有效性。 */
void focus_on_layer_destroy(Layer* layer);
void focus_on_layer_show(Layer* layer);
void focus_on_layer_hide(Layer* layer);

int focus_is_focusable(const Layer* layer);
int focus_is_within_scope(const Layer* layer);

/* 焦点移入可滚动容器时把该层滚动到可见区域。 */
void focus_scroll_into_view(Layer* layer);

#endif
