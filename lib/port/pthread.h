/*
 * Mock pthread.h for ya embedded (esp32/stm32) core build.
 * newlib does not ship pthread.h; the real implementation is provided
 * by the ESP-IDF project (esp_pthread) / RTOS at link time. Static
 * libs may keep these symbols undefined, so only declarations needed.
 */
#ifndef _PORT_PTHREAD_H
#define _PORT_PTHREAD_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t pthread_t;
typedef uint32_t pthread_mutex_t;

#define PTHREAD_MUTEX_INITIALIZER 0

int pthread_mutex_init(pthread_mutex_t *mutex, const void *attr);
int pthread_mutex_lock(pthread_mutex_t *mutex);
int pthread_mutex_unlock(pthread_mutex_t *mutex);
int pthread_mutex_destroy(pthread_mutex_t *mutex);

#ifdef __cplusplus
}
#endif

#endif /* _PORT_PTHREAD_H */
