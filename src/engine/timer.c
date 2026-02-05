#include "timer.h"
#include <time.h>

#ifdef _WIN32
    #include <windows.h>
    static LARGE_INTEGER perf_freq;
    static LARGE_INTEGER perf_counter_start;
#else
    #include <sys/time.h>
#endif

Timer g_timer = {0};

void Timer_Init(void)
{
#ifdef _WIN32
    QueryPerformanceFrequency(&perf_freq);
    QueryPerformanceCounter(&perf_counter_start);
#endif
    g_timer.current_time = 0.0;
    g_timer.last_time = 0.0;
    g_timer.delta_time = 0.0;
    g_timer.frame_count = 0.0;
    g_timer.fps = 0.0;
}

double GetTime(void)
{
#ifdef _WIN32
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return (double)(counter.QuadPart - perf_counter_start.QuadPart) / perf_freq.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
#endif
}

void Timer_Update(void)
{
    g_timer.last_time = g_timer.current_time;
    g_timer.current_time = GetTime();
    g_timer.delta_time = g_timer.current_time - g_timer.last_time;
    
    if (g_timer.delta_time > 0.0)
    {
        g_timer.fps = 1.0 / g_timer.delta_time;
    }
    
    g_timer.frame_count += 1.0;
}

double GetDeltaTime(void)
{
    return g_timer.delta_time;
}

double GetFPS(void)
{
    return g_timer.fps;
}
