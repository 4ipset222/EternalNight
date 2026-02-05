#include "forge.h"

bool Forge_Init()
{
    Timer_Init();

    SDL_Init(SDL_INIT_VIDEO);

    dbg_msg("Forge", "FORGE Engine initialized successfully");

    return true;
}

void Forge_Shutdown()
{
    SDL_Quit();

    dbg_msg("Forge", "FORGE Engine shutdown complete");
}
