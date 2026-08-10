#include "platform_thread.h"
#include <stdint.h>
#include <stdlib.h>

#ifdef _WIN32

static DWORD WINAPI ecs_platform_thread_start(void *argument) {
    struct ecs_platform_thread_start_s {
        ecs_platform_thread_func_t function;
        void *argument;
    } *start = argument;
    ecs_platform_thread_func_t function = start->function;
    void *function_argument = start->argument;
    free(start);
    return function(function_argument);
}

bool ecs_platform_thread_create(
    ecs_platform_thread_t *thread,
    ecs_platform_thread_func_t function,
    void *argument
) {
    struct ecs_platform_thread_start_s {
        ecs_platform_thread_func_t function;
        void *argument;
    } *start = malloc(sizeof(*start));
    if (!start) {
        return false;
    }
    start->function = function;
    start->argument = argument;
    thread->handle = CreateThread(NULL, 0, ecs_platform_thread_start, start, 0, NULL);
    if (!thread->handle) {
        free(start);
        return false;
    }
    return true;
}

void ecs_platform_thread_join(ecs_platform_thread_t *thread) {
    WaitForSingleObject(thread->handle, INFINITE);
    CloseHandle(thread->handle);
    thread->handle = NULL;
}

void ecs_platform_mutex_init(ecs_platform_mutex_t *mutex) { InitializeCriticalSection(mutex); }
void ecs_platform_mutex_fini(ecs_platform_mutex_t *mutex) { DeleteCriticalSection(mutex); }
void ecs_platform_mutex_lock(ecs_platform_mutex_t *mutex) { EnterCriticalSection(mutex); }
void ecs_platform_mutex_unlock(ecs_platform_mutex_t *mutex) { LeaveCriticalSection(mutex); }
void ecs_platform_condition_init(ecs_platform_condition_t *condition) {
    InitializeConditionVariable(condition);
}
void ecs_platform_condition_fini(ecs_platform_condition_t *condition) { (void)condition; }
void ecs_platform_condition_wait(
    ecs_platform_condition_t *condition,
    ecs_platform_mutex_t *mutex
) {
    SleepConditionVariableCS(condition, mutex, INFINITE);
}
void ecs_platform_condition_signal(ecs_platform_condition_t *condition) {
    WakeConditionVariable(condition);
}
void ecs_platform_condition_broadcast(ecs_platform_condition_t *condition) {
    WakeAllConditionVariable(condition);
}

uint32_t ecs_platform_hardware_thread_count(void) {
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    return info.dwNumberOfProcessors ? info.dwNumberOfProcessors : 1;
}

#else

#include <stdlib.h>
#include <unistd.h>

bool ecs_platform_thread_create(
    ecs_platform_thread_t *thread,
    ecs_platform_thread_func_t function,
    void *argument
) {
    return pthread_create(&thread->handle, NULL, function, argument) == 0;
}

void ecs_platform_thread_join(ecs_platform_thread_t *thread) {
    pthread_join(thread->handle, NULL);
}

void ecs_platform_mutex_init(ecs_platform_mutex_t *mutex) { pthread_mutex_init(mutex, NULL); }
void ecs_platform_mutex_fini(ecs_platform_mutex_t *mutex) { pthread_mutex_destroy(mutex); }
void ecs_platform_mutex_lock(ecs_platform_mutex_t *mutex) { pthread_mutex_lock(mutex); }
void ecs_platform_mutex_unlock(ecs_platform_mutex_t *mutex) { pthread_mutex_unlock(mutex); }
void ecs_platform_condition_init(ecs_platform_condition_t *condition) {
    pthread_cond_init(condition, NULL);
}
void ecs_platform_condition_fini(ecs_platform_condition_t *condition) {
    pthread_cond_destroy(condition);
}
void ecs_platform_condition_wait(
    ecs_platform_condition_t *condition,
    ecs_platform_mutex_t *mutex
) {
    pthread_cond_wait(condition, mutex);
}
void ecs_platform_condition_signal(ecs_platform_condition_t *condition) {
    pthread_cond_signal(condition);
}
void ecs_platform_condition_broadcast(ecs_platform_condition_t *condition) {
    pthread_cond_broadcast(condition);
}

uint32_t ecs_platform_hardware_thread_count(void) {
    long count = sysconf(_SC_NPROCESSORS_ONLN);
    return count > 0 ? (uint32_t)count : 1;
}

#endif
