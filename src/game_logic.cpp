#include "game_logic.h"
#include "game_input.h"
#include <algorithm>
#include <cmath>

void UpdateSingleplayerGame(Player& player, World* world, const Camera2D& camera,
                           Inventory& inventory, Texture2D* weaponSprite, bool uiBlockInput, float dt)
{
    (void)camera;
    (void)inventory;
    NetInputState netInput = GetNetInput();
    player.Update(dt, Vec2{ netInput.moveX, netInput.moveY }, world->GetRaw(), world->GetTileSize());
    HandlePlayerAttack(player, camera, world, weaponSprite, uiBlockInput);
}

void UpdateCaveState(Player& player, World* world, bool& inCave,
                    Vec2& caveEntrance, bool multiplayerActive)
{
    const float caveDetectRadius = player.GetSize() * 1.5f;
    const float tileSize = world->GetTileSize();
    const auto carveSafeArea = [&](Vec2 center, int radiusTiles)
    {
        int tx = (int)std::floor(center.x / tileSize);
        int ty = (int)std::floor(center.y / tileSize);
        for (int dy = -radiusTiles; dy <= radiusTiles; ++dy)
        {
            for (int dx = -radiusTiles; dx <= radiusTiles; ++dx)
                World_SetTile(world->GetRaw(), tx + dx, ty + dy, TILE_DIRT);
        }
    };
    const auto carveCorridor = [&](Vec2 from, Vec2 to, int halfWidthTiles)
    {
        int x0 = (int)std::floor(from.x / tileSize);
        int y0 = (int)std::floor(from.y / tileSize);
        int x1 = (int)std::floor(to.x / tileSize);
        int y1 = (int)std::floor(to.y / tileSize);

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
        Vec2 playerPos = player.GetPosition();
        if (World_FindNearestCaveEntrance(world->GetRaw(), playerPos.x, playerPos.y, caveDetectRadius, &entranceX, &entranceY))
        {
            inCave = true;
            caveEntrance = { entranceX, entranceY };
            World_SaveCaveEntrance(world->GetRaw(), caveEntrance.x, caveEntrance.y);
            World_SetCaveMode(world->GetRaw(), 1);

            Vec2 caveSpawn = { caveEntrance.x, caveEntrance.y + tileSize * 4.0f };
            carveSafeArea(caveSpawn, 2);
            carveSafeArea(caveEntrance, 2);
            carveCorridor(caveSpawn, caveEntrance, 1);
            World_SetTile(world->GetRaw(), (int)std::floor(caveEntrance.x / tileSize), (int)std::floor(caveEntrance.y / tileSize), TILE_CAVE_ENTRANCE);

            player.SetPosition(caveSpawn);
        }
    }
    else if (inCave && !multiplayerActive)
    {
        Vec2 playerPos = player.GetPosition();
        if (World_CheckCaveEntrance(world->GetRaw(), playerPos.x, playerPos.y, caveDetectRadius))
        {
            inCave = false;
            World_SetCaveMode(world->GetRaw(), 0);

            Vec2 exitPos = { caveEntrance.x, caveEntrance.y - tileSize * 3.0f };
            carveSafeArea(caveEntrance, 2);
            carveSafeArea(exitPos, 2);
            carveCorridor(exitPos, caveEntrance, 1);
            World_SetTile(world->GetRaw(), (int)std::floor(caveEntrance.x / tileSize), (int)std::floor(caveEntrance.y / tileSize), TILE_CAVE_ENTRANCE);

            player.SetPosition(exitPos);
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
