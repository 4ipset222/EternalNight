#ifndef __FORGE_WINDOW_H__
#define __FORGE_WINDOW_H__

#include <SDL2/SDL.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct Window
{
    SDL_Window* handle;
    SDL_Event event;
    SDL_GLContext gl;
    int width;
    int height;
    int should_close;
} Window;

Window* Window_Create(int w, int h, const char* title);
void    Window_Destroy(Window* w);

void    Window_PollEvents(Window* w);
int     Window_ShouldClose(Window* w);

void    Window_SetTitle(Window* w, const char* title);

#ifdef __cplusplus
}
#endif

#endif // __FORGE_WINDOW_H__