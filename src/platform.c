#include "platform.h"
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

#ifdef _WIN32
#include <windows.h>
#elif defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#else
#include <time.h>
#endif

double ecs_platform_time_now_sec(void) {
#ifdef _WIN32
    LARGE_INTEGER frequency;
    LARGE_INTEGER counter;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart / (double)frequency.QuadPart;
#elif defined(__EMSCRIPTEN__)
    return emscripten_get_now() / 1000.0;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
#endif
}

void ecs_platform_time_sleep_sec(double seconds) {
    if (seconds <= 0.0) {
        return;
    }

#ifdef _WIN32
    Sleep((DWORD)(seconds * 1000.0));
#elif defined(__EMSCRIPTEN__)
    /* A web frame must never block the browser's main thread. */
    (void)seconds;
#else
    struct timespec ts;
    ts.tv_sec = (time_t)seconds;
    ts.tv_nsec = (long)((seconds - (double)ts.tv_sec) * 1000000000.0);
    nanosleep(&ts, NULL);
#endif
}
