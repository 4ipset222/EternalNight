#include "game_render.h"
#include "world.h"
#include <cstdio>
#include <cmath>

void DrawDebugInfo(const Player& player, World* world, const Renderer* renderer,
                  const Camera2D& camera, bool inCave, bool isNight, float cycleTimer,
                  float DAY_DURATION, float NIGHT_DURATION, float fogStrength)
{
    float px = player.GetX();
    float py = player.GetY();
    float tileSize = 16.0f;

    int tileX = (int)floorf(px / tileSize);
    int tileY = (int)floorf(py / tileSize);

    int chunkX = tileX >= 0
        ? tileX / CHUNK_SIZE
        : (tileX - CHUNK_SIZE + 1) / CHUNK_SIZE;

    int chunkY = tileY >= 0
        ? tileY / CHUNK_SIZE
        : (tileY - CHUNK_SIZE + 1) / CHUNK_SIZE;

    int tileId = World_GetTile(world->GetRaw(), tileX, tileY);
    const char* biomeName = World_GetBiomeName(world->GetRaw(), tileX, tileY);
    int caveNearby = World_CheckCaveEntrance(world->GetRaw(), px, py, 100.0f);

    char dbg[512];
    snprintf(dbg, sizeof(dbg),
        "FPS: %.1f\n"
        "Player: %.1f, %.1f\n"
        "Tile: %d, %d (id %d)\n"
        "Chunk: %d, %d\n"
        "Biome: %s\n"
        "In Cave: %s\n"
        "Cave Near: %s\n"
        "Zoom: %.2f\n"
        "Time: %s\n"
        "Cycle: %.2f / %.2f\n"
        "Night Fog: %.2f",
        GetFPS(),
        px, py,
        tileX, tileY, tileId,
        chunkX, chunkY,
        biomeName,
        inCave ? "YES" : "NO",
        caveNearby ? "YES" : "NO",
        camera.zoom,
        isNight ? "Night" : "Day",
        cycleTimer, isNight ? NIGHT_DURATION : DAY_DURATION,
        fogStrength
    );

    Renderer_DrawTextEx(dbg, 10, 10, 16, Color{1, 1, 0, 1}, TEXT_STYLE_OUTLINE_SHADOW);
    Renderer_DrawTextEx(" WASD - move\n LMB - attack(sword)\n 1-5 - select hotbar\n Tab - toggle inventory\n Esc - pause menu\n Q/E - zoom\n F3 - debug info",
                       (float)renderer->width - 140, 10, 14, Color{1, 1, 1, 1}, TEXT_STYLE_OUTLINE_SHADOW);
}

void DrawChunkDebugLines(const Camera2D& camera, const Renderer* renderer)
{
    const float tileSize = 16.0f;
    const float chunkWorldSize = CHUNK_SIZE * tileSize;

    auto floorDiv = [](int v, int d) -> int
    {
        return v >= 0 ? v / d : (v - d + 1) / d;
    };

    int tilesXStart = (int)floorf(camera.x / tileSize);
    int tilesYStart = (int)floorf(camera.y / tileSize);
    int tilesXEnd = tilesXStart + (int)ceilf(800 / tileSize / camera.zoom) + 2;
    int tilesYEnd = tilesYStart + (int)ceilf(600 / tileSize / camera.zoom) + 2;

    int chunkXStart = floorDiv(tilesXStart, CHUNK_SIZE);
    int chunkYStart = floorDiv(tilesYStart, CHUNK_SIZE);
    int chunkXEnd = floorDiv(tilesXEnd - 1, CHUNK_SIZE);
    int chunkYEnd = floorDiv(tilesYEnd - 1, CHUNK_SIZE);

    Color chunkColor = {1.0f, 0.2f, 0.2f, 0.6f};
    for (int cy = chunkYStart; cy <= chunkYEnd; cy++)
    {
        for (int cx = chunkXStart; cx <= chunkXEnd; cx++)
        {
            float x0 = cx * chunkWorldSize;
            float y0 = cy * chunkWorldSize;
            Renderer_DrawRectangleLines(Rect{x0, y0, chunkWorldSize, chunkWorldSize}, 1, chunkColor);
        }
    }
}
