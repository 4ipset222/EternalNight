#include "game_logic.h"
#include "game_input.h"
#include <algorithm>
#include <cmath>

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
    const float caveDetectRadius = player.GetSize() * 1.5f;
    const float tileSize = world->GetTileSize();
    const auto carveSafeArea = [&](float cx, float cy, int radiusTiles)
    {
        int tx = (int)std::floor(cx / tileSize);
        int ty = (int)std::floor(cy / tileSize);
        for (int dy = -radiusTiles; dy <= radiusTiles; ++dy)
        {
            for (int dx = -radiusTiles; dx <= radiusTiles; ++dx)
                World_SetTile(world->GetRaw(), tx + dx, ty + dy, TILE_DIRT);
        }
    };
    const auto carveCorridor = [&](float fromX, float fromY, float toX, float toY, int halfWidthTiles)
    {
        int x0 = (int)std::floor(fromX / tileSize);
        int y0 = (int)std::floor(fromY / tileSize);
        int x1 = (int)std::floor(toX / tileSize);
        int y1 = (int)std::floor(toY / tileSize);

        int x = x0;
        int y = y0;
        while (x != x1)
        {
            x += (x1 > x) ? 1 : -1;
            for (int oy = -halfWidthTiles; oy <= halfWidthTiles; ++oy)
                for (int ox = -halfWidthTiles; ox <= halfWidthTiles; ++ox)
                    World_SetTile(world->GetRaw(), x + ox, y + oy, TILE_DIRT);
        }
        while (y != y1)
        {
            y += (y1 > y) ? 1 : -1;
            for (int oy = -halfWidthTiles; oy <= halfWidthTiles; ++oy)
                for (int ox = -halfWidthTiles; ox <= halfWidthTiles; ++ox)
                    World_SetTile(world->GetRaw(), x + ox, y + oy, TILE_DIRT);
        }
    };

    if (!inCave && !multiplayerActive)
    {
        float entranceX = 0.0f;
        float entranceY = 0.0f;
        if (World_FindNearestCaveEntrance(world->GetRaw(), player.GetX(), player.GetY(), caveDetectRadius, &entranceX, &entranceY))
        {
            inCave = true;
            caveEntranceX = entranceX;
            caveEntranceY = entranceY;
            World_SaveCaveEntrance(world->GetRaw(), caveEntranceX, caveEntranceY);
            World_SetCaveMode(world->GetRaw(), 1);

            float caveSpawnX = caveEntranceX;
            float caveSpawnY = caveEntranceY + tileSize * 4.0f;
            carveSafeArea(caveSpawnX, caveSpawnY, 2);
            carveSafeArea(caveEntranceX, caveEntranceY, 2);
            carveCorridor(caveSpawnX, caveSpawnY, caveEntranceX, caveEntranceY, 1);
            World_SetTile(world->GetRaw(), (int)std::floor(caveEntranceX / tileSize), (int)std::floor(caveEntranceY / tileSize), TILE_CAVE_ENTRANCE);

            player.SetPosition(caveSpawnX, caveSpawnY);
        }
    }
    else if (inCave && !multiplayerActive)
    {
        if (World_CheckCaveEntrance(world->GetRaw(), player.GetX(), player.GetY(), caveDetectRadius))
        {
            inCave = false;
            World_SetCaveMode(world->GetRaw(), 0);

            float exitX = caveEntranceX;
            float exitY = caveEntranceY - tileSize * 3.0f;
            carveSafeArea(caveEntranceX, caveEntranceY, 2);
            carveSafeArea(exitX, exitY, 2);
            carveCorridor(exitX, exitY, caveEntranceX, caveEntranceY, 1);
            World_SetTile(world->GetRaw(), (int)std::floor(caveEntranceX / tileSize), (int)std::floor(caveEntranceY / tileSize), TILE_CAVE_ENTRANCE);

            player.SetPosition(exitX, exitY);
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
