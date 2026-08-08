// This file is used to ensure js_yuistdlib symbol is linked
// It only includes yui_stdlib.h to get the symbol definition


// #ifdef CONFIG_CLASS_YUI
#include "js_socket.c"
#include "js_game.c"
#include "js_timer.c"
#include "js_perf.c"

#include "yui_stdlib.c"
/* 32-bit targets (JS_PTR64 undefined, e.g. esp32/stm32) use the
   32-bit ROM table (yui_stdlib_32.h) since RV32 GCC cannot build the
   64-bit self-referencing table. */
#ifdef JS_PTR64
#include "yui_stdlib.h"
#else
#include "yui_stdlib_32.h"
#endif
// #endif
