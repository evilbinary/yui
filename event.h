#ifndef YUI_EVENT_H
#define YUI_EVENT_H

#define MAX_EVENT 512

// 定义事件处理函数类型
typedef void (*EventHandler)(void* data);


typedef struct {
    char name[50];          // 事件名称
    EventHandler handler;   // 事件处理函数
} EventEntry;


int register_event_handler(const char* name, EventHandler handler);

EventHandler find_event_by_name(const char* name);


#endif