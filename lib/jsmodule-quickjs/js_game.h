#ifndef YUI_JS_GAME_H
#define YUI_JS_GAME_H

#include "../../lib/quickjs/quickjs.h"

#ifndef YUI_WITH_GAME
#define YUI_WITH_GAME 1
#endif

void js_game_register_api(JSContext* ctx);

void js_game_set_context(JSContext* ctx);

void js_game_shutdown(void);

#endif
