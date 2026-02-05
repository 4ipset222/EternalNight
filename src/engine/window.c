#include "window.h"
#include "input.h"
#include "timer.h"
#include <GL/glew.h>

Window* Window_Create(int w, int h, const char* title)
{
    Window* win = malloc(sizeof(Window));

    win->handle = SDL_CreateWindow(
        title,
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        w, h,
        SDL_WINDOW_OPENGL
    );

    win->gl = SDL_GL_CreateContext(win->handle);

    glewExperimental = GL_TRUE;
    glewInit();

    win->width = w;
    win->height = h;
    win->should_close = 0;

    Input_Init();

    dbg_msg("Window", "Window created: %s (%dx%d)", title, w, h);

    return win;
}

void Window_Destroy(Window* w)
{
    SDL_GL_DeleteContext(w->gl);
    SDL_DestroyWindow(w->handle);

    dbg_msg("Window", "Window destroyed");

    free(w);
}

void Window_PollEvents(Window* w)
{
    Timer_Update();
    Input_Update();
    while (SDL_PollEvent(&w->event))
    {
        if (w->event.type == SDL_QUIT)
        {
            w->should_close = 1;
        }
        Input_ProcessEvent(&w->event);
    }
}

int Window_ShouldClose(Window* w)
{
    return w->should_close;
}

void Window_SetTitle(Window* w, const char* title)
{
    SDL_SetWindowTitle(w->handle, title);
}