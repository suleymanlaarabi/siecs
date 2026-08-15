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
#define ecs_platform_mutex_init(m) InitializeCriticalSection(m)
#define ecs_platform_mutex_fini(m) DeleteCriticalSection(m)
#define ecs_platform_mutex_lock(m) EnterCriticalSection(m)
#define ecs_platform_mutex_unlock(m) LeaveCriticalSection(m)
#define ecs_platform_condition_init(c) InitializeConditionVariable(c)
#define ecs_platform_condition_fini(c) ((void)(c))
#define ecs_platform_condition_wait(c, m) SleepConditionVariableCS(c, m, INFINITE)
#define ecs_platform_condition_signal(c) WakeConditionVariable(c)
#define ecs_platform_condition_broadcast(c) WakeAllConditionVariable(c)
#else
#include <pthread.h>
typedef struct {
    pthread_t handle;
} ecs_platform_thread_t;
typedef pthread_mutex_t ecs_platform_mutex_t;
typedef pthread_cond_t ecs_platform_condition_t;
typedef void *(*ecs_platform_thread_func_t)(void *);
#define ECS_PLATFORM_THREAD_CALL
#define ecs_platform_thread_create(t, f, a) (pthread_create(&(t)->handle, NULL, f, a) == 0)
#define ecs_platform_thread_join(t) pthread_join((t)->handle, NULL)
#define ecs_platform_mutex_init(m) pthread_mutex_init(m, NULL)
#define ecs_platform_mutex_fini(m) pthread_mutex_destroy(m)
#define ecs_platform_mutex_lock(m) pthread_mutex_lock(m)
#define ecs_platform_mutex_unlock(m) pthread_mutex_unlock(m)
#define ecs_platform_condition_init(c) pthread_cond_init(c, NULL)
#define ecs_platform_condition_fini(c) pthread_cond_destroy(c)
#define ecs_platform_condition_wait(c, m) pthread_cond_wait(c, m)
#define ecs_platform_condition_signal(c) pthread_cond_signal(c)
#define ecs_platform_condition_broadcast(c) pthread_cond_broadcast(c)
#endif

#ifdef _WIN32
bool ecs_platform_thread_create(
    ecs_platform_thread_t *thread,
    ecs_platform_thread_func_t function,
    void *argument
);
void ecs_platform_thread_join(ecs_platform_thread_t *thread);
#endif

uint32_t ecs_platform_hardware_thread_count(void);

#endif
