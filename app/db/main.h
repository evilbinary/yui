#ifndef YUI_APP_DB_MAIN_H
#define YUI_APP_DB_MAIN_H

#include <quickjs.h>

#ifdef __cplusplus
extern "C" {
#endif

void register_mysql_api(JSContext* ctx);

#ifdef __cplusplus
}
#endif

#endif
