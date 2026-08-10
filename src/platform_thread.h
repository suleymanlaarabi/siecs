#ifndef SIECS_PLATFORM_THREAD_H
#define SIECS_PLATFORM_THREAD_H

#include <stdbool.h>
#include <stdint.h>

#ifdef _WIN32
#include <windows.h>
typedef struct {
    HANDLE handle;
} ecs_platform_thread_t;
typedef CRITICAL_SECTION ecs_platform_mutex_t;
typedef CONDITION_VARIABLE ecs_platform_condition_t;
#define ECS_PLATFORM_THREAD_CALL WINAPI
typedef DWORD (ECS_PLATFORM_THREAD_CALL *ecs_platform_thread_func_t)(void *);
#else
#include <pthread.h>
typedef struct {
    pthread_t handle;
} ecs_platform_thread_t;
typedef pthread_mutex_t ecs_platform_mutex_t;
typedef pthread_cond_t ecs_platform_condition_t;
typedef void *(*ecs_platform_thread_func_t)(void *);
#define ECS_PLATFORM_THREAD_CALL
#endif

bool ecs_platform_thread_create(
    ecs_platform_thread_t *thread,
    ecs_platform_thread_func_t function,
    void *argument
);
void ecs_platform_thread_join(ecs_platform_thread_t *thread);

void ecs_platform_mutex_init(ecs_platform_mutex_t *mutex);
void ecs_platform_mutex_fini(ecs_platform_mutex_t *mutex);
void ecs_platform_mutex_lock(ecs_platform_mutex_t *mutex);
void ecs_platform_mutex_unlock(ecs_platform_mutex_t *mutex);

void ecs_platform_condition_init(ecs_platform_condition_t *condition);
void ecs_platform_condition_fini(ecs_platform_condition_t *condition);
void ecs_platform_condition_wait(
    ecs_platform_condition_t *condition,
    ecs_platform_mutex_t *mutex
);
void ecs_platform_condition_signal(ecs_platform_condition_t *condition);
void ecs_platform_condition_broadcast(ecs_platform_condition_t *condition);

uint32_t ecs_platform_hardware_thread_count(void);

#endif
