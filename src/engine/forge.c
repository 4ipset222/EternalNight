#include "forge.h"
#ifdef _WIN32
#include <objbase.h>
#endif

bool Forge_Init()
{
    Timer_Init();

    SDL_Init(SDL_INIT_VIDEO);
#ifdef _WIN32
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
#endif

    dbg_msg("Forge", "FORGE Engine initialized successfully");

    return true;
}

void Forge_Shutdown()
{
#ifdef _WIN32
    CoUninitialize();
#endif
    SDL_Quit();

    dbg_msg("Forge", "FORGE Engine shutdown complete");
}
