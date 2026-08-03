/* Mock dlfcn.h for embedded platforms without dynamic linking
 * (ESP32/STM32). quickjs-libc.c includes <dlfcn.h>; provide empty
 * implementations so it compiles. Shared-library JS modules are
 * not supported on these platforms. */
#ifndef YUI_MOCK_DLFCN_H
#define YUI_MOCK_DLFCN_H

#define RTLD_LAZY   1
#define RTLD_NOW    2
#define RTLD_LOCAL  0
#define RTLD_GLOBAL 0x100

static inline void* dlopen(const char* path, int flags) {
    (void)path;
    (void)flags;
    return NULL;
}

static inline void* dlsym(void* handle, const char* symbol) {
    (void)handle;
    (void)symbol;
    return NULL;
}

static inline int dlclose(void* handle) {
    (void)handle;
    return 0;
}

static inline const char* dlerror(void) {
    return "dynamic linking not supported on this platform";
}

#endif /* YUI_MOCK_DLFCN_H */
