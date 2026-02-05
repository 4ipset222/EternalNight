#ifndef __FORGE_TIMER_H__
#define __FORGE_TIMER_H__

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct Timer
{
    double current_time;
    double last_time;
    double delta_time;
    double frame_count;
    double fps;
} Timer;

extern Timer g_timer;

void Timer_Init(void);
void Timer_Update(void);

double GetDeltaTime(void);
double GetFPS(void);
double GetTime(void);

#ifdef __cplusplus
}
#endif

#endif // __FORGE_TIMER_H__
