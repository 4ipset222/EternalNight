#include "World.h"
#include <vector>
#include <math.h>

World::World(int loadRadiusChunks, int seed)
{
    forgeWorld = World_Create(loadRadiusChunks, seed);
    tileSize = 16.0f;
}

World::~World()
{
    World_Destroy(forgeWorld);
}

void World::Update(float dt, int isNight, float focusX, float focusY, float playerX, float playerY, float playerRadius, float* ioPlayerHP, bool updateMobs)
{
    int camTileX = (int)floorf(focusX / tileSize);
    int camTileY = (int)floorf(focusY / tileSize);

    int centerChunkX = camTileX >= 0
        ? camTileX / CHUNK_SIZE
        : (camTileX - CHUNK_SIZE + 1) / CHUNK_SIZE;

    int centerChunkY = camTileY >= 0
        ? camTileY / CHUNK_SIZE
        : (camTileY - CHUNK_SIZE + 1) / CHUNK_SIZE;

    World_UpdateChunks(forgeWorld, centerChunkX, centerChunkY);
    if (updateMobs)
        World_UpdateMobs(forgeWorld, dt, isNight, playerX, playerY, playerRadius, ioPlayerHP);
}

void World::Draw(const Camera2D& camera, int screenW, int screenH, bool drawMobs) const
{
    int tilesXStart = (int)floorf(camera.x / tileSize);
    int tilesYStart = (int)floorf(camera.y / tileSize);

    int tilesXEnd = tilesXStart +
        (int)ceilf(screenW / tileSize / camera.zoom) + 2;

    int tilesYEnd = tilesYStart +
        (int)ceilf(screenH / tileSize / camera.zoom) + 2;

    const int tilesW = tilesXEnd - tilesXStart;
    const int tilesH = tilesYEnd - tilesYStart;
    const int maxTiles = (tilesW > 0 && tilesH > 0)
        ? tilesW * tilesH
        : 0;

    std::vector<float> triPos;
    std::vector<float> triCol;
    triPos.reserve((size_t)maxTiles * 6 * 2);
    triCol.reserve((size_t)maxTiles * 6 * 4);

    for (int y = tilesYStart; y < tilesYEnd; y++)
    {
        for (int x = tilesXStart; x < tilesXEnd; x++)
        {
            TileType tileType = World_GetTile(forgeWorld, x, y);
            if (tileType == TILE_EMPTY)
                continue;

            Vec4 col = World_GetTileColor(tileType);

            float x0 = x * tileSize;
            float y0 = y * tileSize;
            float x1 = x0 + tileSize;
            float y1 = y0 + tileSize;

            float pos[] =
            {
                x0, y0,
                x1, y0,
                x1, y1,
                x1, y1,
                x0, y1,
                x0, y0
            };

            for (int i = 0; i < 12; i++)
                triPos.push_back(pos[i]);

            for (int v = 0; v < 6; v++)
            {
                triCol.push_back(col.x);
                triCol.push_back(col.y);
                triCol.push_back(col.z);
                triCol.push_back(col.w);
            }
        }
    }

    Renderer_DrawTrianglesColored(
        triPos.data(),
        triCol.data(),
        (int)(triPos.size() / 2)
    );

    if (drawMobs)
    {
        int mobCount = 0;
        const Mob* mobs = World_GetMobs(forgeWorld, &mobCount);
        int typeCount = 0;
        const MobArchetype* types = World_GetMobArchetypes(forgeWorld, &typeCount);
        if (mobs && types)
        {
            for (int i = 0; i < mobCount; i++)
            {
                const Mob& mob = mobs[i];
                if (mob.type < 0 || mob.type >= typeCount)
                    continue;
                const MobArchetype& arch = types[mob.type];

                Renderer_DrawRectangle(
                    Rect{ mob.x - arch.size * 0.5f, mob.y - arch.size * 0.5f, arch.size, arch.size },
                    Color{ arch.color.x, arch.color.y, arch.color.z, arch.color.w }
                );
            }
        }
    }
}
