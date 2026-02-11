#include "game_logic.h"
#include "game_input.h"
#include <algorithm>

void UpdateSingleplayerGame(Player& player, World* world, const Camera2D& camera,
                           Inventory& inventory, Texture2D* weaponSprite, bool uiBlockInput, float dt)
{
    NetInputState netInput = GetNetInput();
    player.Update(dt, netInput.moveX, netInput.moveY, world->GetRaw(), world->GetTileSize());
    HandleBlockPlacement(player, camera, nullptr, world, inventory, uiBlockInput);
    HandlePlayerAttack(player, camera, world, weaponSprite, uiBlockInput);
}

void UpdateCaveState(Player& player, World* world, bool& inCave,
                    float& caveEntranceX, float& caveEntranceY, bool multiplayerActive)
{
    if (!inCave && !multiplayerActive)
    {
        if (World_CheckCaveEntrance(world->GetRaw(), player.GetX(), player.GetY(), player.GetSize()))
        {
            inCave = true;
            caveEntranceX = player.GetX();
            caveEntranceY = player.GetY();
            World_SaveCaveEntrance(world->GetRaw(), caveEntranceX, caveEntranceY);
            World_SetCaveMode(world->GetRaw(), 1);
            player.SetPosition(caveEntranceX, caveEntranceY + 100.0f);
        }
    }
    else if (inCave && !multiplayerActive)
    {
        if (World_CheckCaveEntrance(world->GetRaw(), player.GetX(), player.GetY(), player.GetSize()))
        {
            inCave = false;
            World_SetCaveMode(world->GetRaw(), 0);
            player.SetPosition(caveEntranceX, caveEntranceY - 50.0f);
        }
    }
}

void UpdateDayNightCycle(bool& isNight, float& cycleTimer, float DAY_DURATION,
                        float NIGHT_DURATION, float NIGHT_FADE, float& fogStrength, float dt)
{
    cycleTimer += dt;
    if (!isNight && cycleTimer >= DAY_DURATION)
    {
        isNight = true;
        cycleTimer = 0.0f;
    }
    else if (isNight && cycleTimer >= NIGHT_DURATION)
    {
        isNight = false;
        cycleTimer = 0.0f;
    }

    const float maxFog = 0.8f;
    if (isNight)
    {
        float t = cycleTimer;
        if (t < NIGHT_FADE)
            fogStrength = std::clamp(t / NIGHT_FADE * maxFog, 0.0f, maxFog);
        else if (t > NIGHT_DURATION - NIGHT_FADE)
            fogStrength = std::clamp((NIGHT_DURATION - t) / NIGHT_FADE * maxFog, 0.0f, maxFog);
        else
            fogStrength = maxFog;
    }
    else
    {
        fogStrength = 0.0f;
    }
}
