#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"
#include "event.h"
#include "mysql_fun.h"
#include <mysql/mysql.h>

static MYSQL* g_mysql_conn = NULL;
static int g_mysql_connected = 0;

static const char* json_get_string(cJSON* json, const char* key) {
    cJSON* item = cJSON_GetObjectItem(json, key);
    return (item && cJSON_IsString(item)) ? item->valuestring : NULL;
}

static int json_get_int(cJSON* json, const char* key, int def) {
    cJSON* item = cJSON_GetObjectItem(json, key);
    return (item && cJSON_IsNumber(item)) ? item->valueint : def;
}

static char* result_to_json(MYSQL_RES* result) {
    if (!result) return strdup("[]");

    unsigned int num = mysql_num_fields(result);
    MYSQL_FIELD* fields = mysql_fetch_fields(result);

    cJSON* arr = cJSON_CreateArray();
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result))) {
        unsigned long* lengths = mysql_fetch_lengths(result);
        cJSON* obj = cJSON_CreateObject();
        for (unsigned int j = 0; j < num; j++) {
            if (row[j]) {
                cJSON_AddStringToObject(obj, fields[j].name, row[j]);
            } else {
                cJSON_AddNullToObject(obj, fields[j].name);
            }
        }
        cJSON_AddItemToArray(arr, obj);
    }
    mysql_free_result(result);

    char* json_str = cJSON_PrintUnformatted(arr);
    cJSON_Delete(arr);
    return json_str;
}

// ====================== 事件处理器 ======================

static void* handle_mysql_connect(void* data) {
    const char* json_str = (const char*)data;
    if (!json_str) return strdup("{\"success\":false,\"error\":\"No args\"}");

    cJSON* json = cJSON_Parse(json_str);
    if (!json) return strdup("{\"success\":false,\"error\":\"Invalid JSON\"}");

    const char* host = json_get_string(json, "host");
    const char* user = json_get_string(json, "user");
    const char* pass = json_get_string(json, "password");
    const char* db   = json_get_string(json, "database");
    int port = json_get_int(json, "port", 3306);

    if (!host || !user || !pass || !db) {
        cJSON_Delete(json);
        return strdup("{\"success\":false,\"error\":\"Missing required fields\"}");
    }

    if (g_mysql_conn) { mysql_close(g_mysql_conn); g_mysql_conn = NULL; g_mysql_connected = 0; }

    g_mysql_conn = mysql_init(NULL);
    if (!g_mysql_conn) {
        cJSON_Delete(json);
        return strdup("{\"success\":false,\"error\":\"mysql_init failed\"}");
    }

    MYSQL* ret = mysql_real_connect(g_mysql_conn, host, user, pass, db, port, NULL, 0);
    cJSON_Delete(json);

    if (ret) {
        g_mysql_connected = 1;
        mysql_set_character_set(g_mysql_conn, "utf8mb4");
        printf("MySQL: Connected to %s:%d db=%s\n", host, port, db);
        return strdup("{\"success\":true}");
    } else {
        printf("MySQL: Connection failed: %s\n", mysql_error(g_mysql_conn));
        char err_buf[512];
        snprintf(err_buf, sizeof(err_buf), "{\"success\":false,\"error\":\"%s\"}", mysql_error(g_mysql_conn));
        mysql_close(g_mysql_conn);
        g_mysql_conn = NULL;
        return strdup(err_buf);
    }
}

static void* handle_mysql_close(void* data) {
    if (g_mysql_conn) {
        mysql_close(g_mysql_conn);
        g_mysql_conn = NULL;
        g_mysql_connected = 0;
        printf("MySQL: Connection closed\n");
    }
    return strdup("{\"success\":true}");
}

static void* handle_mysql_query(void* data) {
    const char* json_str = (const char*)data;
    if (!json_str) return strdup("[]");
    if (!g_mysql_conn || !g_mysql_connected) return strdup("{\"error\":\"Not connected\"}");

    cJSON* json = cJSON_Parse(json_str);
    if (!json) return strdup("{\"error\":\"Invalid JSON\"}");

    const char* sql = json_get_string(json, "sql");
    if (!sql) { cJSON_Delete(json); return strdup("{\"error\":\"Missing sql\"}"); }
    cJSON_Delete(json);

    if (mysql_query(g_mysql_conn, sql) != 0) {
        char err_buf[1024];
        snprintf(err_buf, sizeof(err_buf), "{\"error\":\"%s\"}", mysql_error(g_mysql_conn));
        return strdup(err_buf);
    }

    MYSQL_RES* result = mysql_store_result(g_mysql_conn);
    if (!result) return strdup("[]");

    char* json_out = result_to_json(result);
    // result_to_json allocates with strdup or cJSON_Print, return directly
    return json_out;
}

static void* handle_mysql_exec(void* data) {
    const char* json_str = (const char*)data;
    if (!json_str) return strdup("{\"affected\":0}");
    if (!g_mysql_conn || !g_mysql_connected) return strdup("{\"error\":\"Not connected\"}");

    cJSON* json = cJSON_Parse(json_str);
    if (!json) return strdup("{\"error\":\"Invalid JSON\"}");

    const char* sql = json_get_string(json, "sql");
    if (!sql) { cJSON_Delete(json); return strdup("{\"error\":\"Missing sql\"}"); }
    cJSON_Delete(json);

    if (mysql_query(g_mysql_conn, sql) != 0) {
        char err_buf[1024];
        snprintf(err_buf, sizeof(err_buf), "{\"error\":\"%s\"}", mysql_error(g_mysql_conn));
        return strdup(err_buf);
    }

    my_ulonglong affected = mysql_affected_rows(g_mysql_conn);
    MYSQL_RES* result = mysql_store_result(g_mysql_conn);
    if (result) mysql_free_result(result);

    char buf[64];
    snprintf(buf, sizeof(buf), "{\"affected\":%llu}", (unsigned long long)affected);
    return strdup(buf);
}

static void* handle_mysql_tables(void* data) {
    if (!g_mysql_conn || !g_mysql_connected) return strdup("[]");

    if (mysql_query(g_mysql_conn, "SHOW TABLES") != 0) return strdup("[]");

    MYSQL_RES* result = mysql_store_result(g_mysql_conn);
    if (!result) return strdup("[]");

    cJSON* arr = cJSON_CreateArray();
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result))) {
        cJSON_AddItemToArray(arr, cJSON_CreateString(row[0]));
    }
    mysql_free_result(result);

    char* json_out = cJSON_PrintUnformatted(arr);
    cJSON_Delete(arr);
    return json_out;
}

static void* handle_mysql_databases(void* data) {
    if (!g_mysql_conn || !g_mysql_connected) return strdup("[]");

    if (mysql_query(g_mysql_conn, "SHOW DATABASES") != 0) return strdup("[]");

    MYSQL_RES* result = mysql_store_result(g_mysql_conn);
    if (!result) return strdup("[]");

    cJSON* arr = cJSON_CreateArray();
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result))) {
        cJSON_AddItemToArray(arr, cJSON_CreateString(row[0]));
    }
    mysql_free_result(result);

    char* json_out = cJSON_PrintUnformatted(arr);
    cJSON_Delete(arr);
    return json_out;
}

static void* handle_mysql_db_tables(void* data) {
    const char* json_str = (const char*)data;
    if (!json_str || !g_mysql_conn || !g_mysql_connected) return strdup("[]");

    cJSON* json = cJSON_Parse(json_str);
    if (!json) return strdup("[]");

    const char* dbname = json_get_string(json, "database");
    if (!dbname) { cJSON_Delete(json); return strdup("[]"); }
    cJSON_Delete(json);

    char sql[512];
    snprintf(sql, sizeof(sql), "SHOW TABLES FROM `%s`", dbname);

    if (mysql_query(g_mysql_conn, sql) != 0) return strdup("[]");

    MYSQL_RES* result = mysql_store_result(g_mysql_conn);
    if (!result) return strdup("[]");

    cJSON* arr = cJSON_CreateArray();
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result))) {
        cJSON_AddItemToArray(arr, cJSON_CreateString(row[0]));
    }
    mysql_free_result(result);

    char* json_out = cJSON_PrintUnformatted(arr);
    cJSON_Delete(arr);
    return json_out;
}

static void* handle_mysql_error(void* data) {
    if (g_mysql_conn) {
        char buf[512];
        snprintf(buf, sizeof(buf), "{\"error\":\"%s\"}", mysql_error(g_mysql_conn));
        return strdup(buf);
    } else {
        return strdup("{\"error\":\"No connection\"}");
    }
}

static void* handle_mysql_columns(void* data) {
    const char* json_str = (const char*)data;
    if (!json_str || !g_mysql_conn || !g_mysql_connected) return strdup("[]");

    cJSON* json = cJSON_Parse(json_str);
    if (!json) return strdup("[]");

    const char* table = json_get_string(json, "table");
    if (!table) { cJSON_Delete(json); return strdup("[]"); }
    cJSON_Delete(json);

    char sql[512];
    snprintf(sql, sizeof(sql), "SHOW COLUMNS FROM `%s`", table);

    if (mysql_query(g_mysql_conn, sql) != 0) return strdup("[]");

    MYSQL_RES* result = mysql_store_result(g_mysql_conn);
    if (!result) return strdup("[]");

    char* json_out = result_to_json(result);
    return json_out;
}

// ====================== 注册 ======================

void register_mysql_handlers(void) {
    register_event_handler("mysql_connect",    handle_mysql_connect);
    register_event_handler("mysql_close",      handle_mysql_close);
    register_event_handler("mysql_query",      handle_mysql_query);
    register_event_handler("mysql_exec",       handle_mysql_exec);
    register_event_handler("mysql_tables",     handle_mysql_tables);
    register_event_handler("mysql_databases",  handle_mysql_databases);
    register_event_handler("mysql_error",      handle_mysql_error);
    register_event_handler("mysql_columns",    handle_mysql_columns);
    register_event_handler("mysql_db_tables",  handle_mysql_db_tables);
    printf("MySQL: Registered %d event handlers\n", 9);
}
