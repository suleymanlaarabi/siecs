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

uint32_t ecs_platform_hardware_thread_count(void) {
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    return info.dwNumberOfProcessors ? info.dwNumberOfProcessors : 1;
}

#else

#include <stdlib.h>
#include <unistd.h>

uint32_t ecs_platform_hardware_thread_count(void) {
    long count = sysconf(_SC_NPROCESSORS_ONLN);
    return count > 0 ? (uint32_t)count : 1;
}

#endif
