#include "platform_time.h"

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
