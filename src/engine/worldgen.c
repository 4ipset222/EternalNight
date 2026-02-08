#include "worldgen.h"
#include "forgesystem.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <omp.h>

#define NUM_THREADS 4

static inline long long ChunkKey(int cx, int cy)
{
    return ((long long)(int)cx << 32) | (unsigned int)(int)cy;
}

static unsigned int ChunkHash(long long key, unsigned int cap)
{
    unsigned long long k = (unsigned long long)key;
    return (unsigned int)((k * 2654435761ULL) % (unsigned long long)cap);
}

static Chunk* World_GetChunk(ForgeWorld* world, int cx, int cy)
{
    long long key = ChunkKey(cx, cy);
    unsigned int idx = ChunkHash(key, (unsigned int)world->chunkCapacity);
    for (unsigned int n = 0; n < (unsigned int)world->chunkCapacity; n++)
    {
        unsigned int i = (idx + n) % (unsigned int)world->chunkCapacity;
        if (world->chunkMap[i].state == 0)
            return NULL;
        if (world->chunkMap[i].state == 1 && world->chunkMap[i].key == key)
            return world->chunkMap[i].chunk;
    }
    return NULL;
}

static int World_InsertChunk(ForgeWorld* world, int cx, int cy, Chunk* chunk)
{
    if (world->chunkCount >= world->chunkCapacity)
        return 0;
    long long key = ChunkKey(cx, cy);
    unsigned int idx = ChunkHash(key, (unsigned int)world->chunkCapacity);
    unsigned int tombstone = (unsigned int)(-1);
    for (unsigned int n = 0; n < (unsigned int)world->chunkCapacity; n++)
    {
        unsigned int i = (idx + n) % (unsigned int)world->chunkCapacity;
        if (world->chunkMap[i].state == 1 && world->chunkMap[i].key == key)
            return 0;

        if (world->chunkMap[i].state == 2 && tombstone == (unsigned int)(-1))
            tombstone = i;

        if (world->chunkMap[i].state == 0)
        {
            unsigned int dst = (tombstone != (unsigned int)(-1)) ? tombstone : i;
            world->chunkMap[dst].key = key;
            world->chunkMap[dst].chunk = chunk;
            world->chunkMap[dst].state = 1;
            world->chunkCount++;
            return 1;
        }
    }
    return 0;
}

static unsigned int World_Rand(ForgeWorld* world)
{
    world->rngState = world->rngState * 1664525u + 1013904223u;
    return world->rngState;
}

static float World_Rand01(ForgeWorld* world)
{
    return (float)(World_Rand(world) & 0x00FFFFFFu) / (float)0x01000000u;
}

int World_IsTileSolid(TileType type)
{
    return (type == TILE_STONE || type == TILE_DEEP_WATER) ? 1 : 0;
}

static int World_IsTileBlockedForSpawn(TileType type)
{
    return (type == TILE_STONE || type == TILE_WATER || type == TILE_DEEP_WATER) ? 1 : 0;
}

static int World_IsPointSolid(ForgeWorld* world, float tileSize, float x, float y)
{
    if (!world || tileSize <= 0.0f) return 0;
    int tx = (int)floorf(x / tileSize);
    int ty = (int)floorf(y / tileSize);
    TileType t = World_GetTile(world, tx, ty);
    return World_IsTileSolid(t);
}

static int World_IsCircleSolid(ForgeWorld* world, float tileSize, float x, float y, float radius)
{
    if (radius <= 0.0f) return World_IsPointSolid(world, tileSize, x, y);
    if (World_IsPointSolid(world, tileSize, x - radius, y - radius)) return 1;
    if (World_IsPointSolid(world, tileSize, x + radius, y - radius)) return 1;
    if (World_IsPointSolid(world, tileSize, x - radius, y + radius)) return 1;
    if (World_IsPointSolid(world, tileSize, x + radius, y + radius)) return 1;
    return 0;
}

void World_MoveWithCollision(ForgeWorld* world, float tileSize, float radius, float* ioX, float* ioY, float dx, float dy)
{
    if (!ioX || !ioY) return;
    float x = *ioX;
    float y = *ioY;
    float nx = x + dx;
    float ny = y + dy;

    if (!world || tileSize <= 0.0f)
    {
        *ioX = nx;
        *ioY = ny;
        return;
    }

    if (!World_IsCircleSolid(world, tileSize, nx, y, radius))
        x = nx;
    if (!World_IsCircleSolid(world, tileSize, x, ny, radius))
        y = ny;

    *ioX = x;
    *ioY = y;
}

int World_RegisterMobType(ForgeWorld* world, const MobArchetype* archetype)
{
    if (!world || !archetype || !world->mobTypes)
        return -1;
    if (world->mobTypeCount >= WORLD_MAX_MOB_TYPES)
        return -1;

    int id = world->mobTypeCount++;
    world->mobTypes[id] = *archetype;
    world->mobTypes[id].name[sizeof(world->mobTypes[id].name) - 1] = '\0';
    return id;
}

int World_SpawnMob(ForgeWorld* world, int type, float x, float y)
{
    if (!world || !world->mobs)
        return 0;
    if (type < 0 || type >= world->mobTypeCount)
        return 0;
    if (world->mobCount >= world->mobCapacity)
        return 0;

    Mob* mob = &world->mobs[world->mobCount++];
    mob->type = type;
    mob->x = x;
    mob->y = y;
    mob->vx = 0.0f;
    mob->vy = 0.0f;
    mob->attackTimer = 0.0f;
    mob->hp = world->mobTypes[type].maxHP;
    return 1;
}

void World_UpdateMobs(ForgeWorld* world, float dt, int isNight, float playerX, float playerY, float playerRadius, float* ioPlayerHP)
{
    float px = playerX;
    float py = playerY;
    World_UpdateMobsMulti(world, dt, isNight, &px, &py, 1, playerRadius, ioPlayerHP);
}

void World_UpdateMobsMulti(ForgeWorld* world, float dt, int isNight, const float* playerX, const float* playerY, int playerCount, float playerRadius, float* ioPlayerHP)
{
    if (!world || !world->mobs || world->mobTypeCount <= 0 || playerCount <= 0)
        return;

    const float tileSize = 16.0f;
    const int desiredCount = 12;
    const float spawnCooldown = 4.0f;
    const float spawnMinRadius = 220.0f;
    const float spawnMaxRadius = 420.0f;
    const float despawnRadius = 700.0f;

    if (world->mobSpawnCooldown > 0.0f)
        world->mobSpawnCooldown -= dt;

    if (!isNight)
    {
        world->mobSpawnCooldown = spawnCooldown;
    }

    if (isNight && world->mobCount < desiredCount && world->mobSpawnCooldown <= 0.0f)
    {
        int pick = (int)(World_Rand01(world) * (float)playerCount);
        if (pick < 0) pick = 0;
        if (pick >= playerCount) pick = playerCount - 1;

        int type = (int)(World_Rand01(world) * (float)world->mobTypeCount);
        if (type < 0) type = 0;
        if (type >= world->mobTypeCount) type = world->mobTypeCount - 1;

        bool spawned = false;
        for (int attempt = 0; attempt < 12; ++attempt)
        {
            float angle = World_Rand01(world) * 6.2831853f;
            float radius = spawnMinRadius + World_Rand01(world) * (spawnMaxRadius - spawnMinRadius);
            float sx = playerX[pick] + cosf(angle) * radius;
            float sy = playerY[pick] + sinf(angle) * radius;

            int tx = (int)floorf(sx / tileSize);
            int ty = (int)floorf(sy / tileSize);
            TileType t = World_GetTile(world, tx, ty);
            if (World_IsTileBlockedForSpawn(t))
                continue;

            World_SpawnMob(world, type, sx, sy);
            world->mobSpawnCooldown = spawnCooldown;
            spawned = true;
            break;
        }
        if (!spawned)
            world->mobSpawnCooldown = spawnCooldown;
    }

    for (int i = 0; i < world->mobCount; i++)
    {
        Mob* mob = &world->mobs[i];
        const MobArchetype* arch = &world->mobTypes[mob->type];

        if (mob->attackTimer > 0.0f)
            mob->attackTimer -= dt;

        float bestDistSq = 1e30f;
        int bestIdx = 0;
        for (int p = 0; p < playerCount; ++p)
        {
            float dxp = playerX[p] - mob->x;
            float dyp = playerY[p] - mob->y;
            float d = dxp * dxp + dyp * dyp;
            if (d < bestDistSq)
            {
                bestDistSq = d;
                bestIdx = p;
            }
        }

        float targetX = playerX[bestIdx];
        float targetY = playerY[bestIdx];

        if (bestDistSq > despawnRadius * despawnRadius)
        {
            world->mobs[i] = world->mobs[world->mobCount - 1];
            world->mobCount--;
            i--;
            continue;
        }

        float aggro = arch->aggroRange;
        if (bestDistSq <= aggro * aggro)
        {
            float dist = sqrtf(bestDistSq);
            if (dist > 0.001f)
            {
                float nx = (targetX - mob->x) / dist;
                float ny = (targetY - mob->y) / dist;
                float stopDist = (arch->size * 0.5f) + playerRadius;
                if (dist > stopDist)
                {
                    float dx = nx * arch->speed * dt;
                    float dy = ny * arch->speed * dt;
                    float radius = arch->size * 0.5f;
                    World_MoveWithCollision(world, tileSize, radius, &mob->x, &mob->y, dx, dy);
                }
                else
                {
                    float desiredX = targetX - nx * stopDist;
                    float desiredY = targetY - ny * stopDist;
                    float radius = arch->size * 0.5f;
                    float dx = desiredX - mob->x;
                    float dy = desiredY - mob->y;
                    World_MoveWithCollision(world, tileSize, radius, &mob->x, &mob->y, dx, dy);
                }
            }
        }

        float attackRange = arch->attackRange + playerRadius;
        if (bestDistSq <= attackRange * attackRange)
        {
            if (mob->attackTimer <= 0.0f && ioPlayerHP)
            {
                float hp = ioPlayerHP[bestIdx] - arch->attackDamage;
                ioPlayerHP[bestIdx] = hp < 0.0f ? 0.0f : hp;
                mob->attackTimer = arch->attackCooldown;
            }
        }
    }
}

const Mob* World_GetMobs(const ForgeWorld* world, int* outCount)
{
    if (outCount)
        *outCount = world ? world->mobCount : 0;
    return world ? world->mobs : NULL;
}

const MobArchetype* World_GetMobArchetypes(const ForgeWorld* world, int* outCount)
{
    if (outCount)
        *outCount = world ? world->mobTypeCount : 0;
    return world ? world->mobTypes : NULL;
}

int World_PlayerAttack(ForgeWorld* world, float originX, float originY, float dirX, float dirY, float range, float arcCos, float damage)
{
    if (!world || !world->mobs || world->mobCount <= 0)
        return 0;

    float dirLenSq = dirX * dirX + dirY * dirY;
    if (dirLenSq < 0.0001f)
        return 0;

    float invLen = 1.0f / sqrtf(dirLenSq);
    dirX *= invLen;
    dirY *= invLen;

    int hits = 0;
    for (int i = 0; i < world->mobCount; i++)
    {
        Mob* mob = &world->mobs[i];
        const MobArchetype* arch = &world->mobTypes[mob->type];

        float dx = mob->x - originX;
        float dy = mob->y - originY;
        float distSq = dx * dx + dy * dy;
        float radius = (arch->size * 0.5f);
        float maxDist = range + radius;
        if (distSq > maxDist * maxDist)
            continue;

        float dist = sqrtf(distSq);
        if (dist > 0.001f)
        {
            float ndx = dx / dist;
            float ndy = dy / dist;
            float dot = ndx * dirX + ndy * dirY;
            if (dot < arcCos)
                continue;
        }

        mob->hp -= damage;
        hits++;
        if (mob->hp <= 0.0f)
        {
            world->mobs[i] = world->mobs[world->mobCount - 1];
            world->mobCount--;
            i--;
        }
    }

    return hits;
}

void Chunk_Generate(Chunk* chunk, int seed)
{
    Perlin_Init(seed);

    float scale = 70.0f;
    float invScale = 1.0f / scale;

    float oceanLevel = 0.38f;
    float shallowWaterLevel = oceanLevel + 0.03f;
    float beachLevel = oceanLevel + 0.06f;
    float hillLevel = 0.62f;
    float mountainLevel = 0.72f;
    float coldTemp = 0.35f;
    float hotTemp = 0.68f;

    int baseX = chunk->cx * CHUNK_SIZE;
    int baseY = chunk->cy * CHUNK_SIZE;

    for (int y = 0; y < CHUNK_SIZE; y++)
    {
        for (int x = 0; x < CHUNK_SIZE; x++)
        {
            int wx = baseX + x;
            int wy = baseY + y;

            float nx = wx * invScale;
            float ny = wy * invScale;

            float h = FractalPerlin2D(nx, ny, 4, 0.5f);
            float d = Perlin2D(nx * 2.2f, ny * 2.2f);
            float v = h * 0.8f + d * 0.2f;

            float continent = FractalPerlin2D(nx * 0.15f, ny * 0.15f, 2, 0.5f);
            float oceanBias = (0.5f - continent) * 0.25f;
            float height = v + oceanBias;

            float temp = FractalPerlin2D(nx * 0.08f, ny * 0.08f, 2, 0.5f);
            temp -= (height - 0.5f) * 0.6f;
            if (temp < 0.0f) temp = 0.0f;
            if (temp > 1.0f) temp = 1.0f;

            float moisture = FractalPerlin2D(nx * 0.12f, ny * 0.12f, 3, 0.5f);

            Tile* tile = &chunk->tiles[y * CHUNK_SIZE + x];

            if (height < oceanLevel)
            {
                tile->type = TILE_DEEP_WATER;
            }
            else if (height < shallowWaterLevel)
            {
                tile->type = (temp < coldTemp) ? TILE_ICE : TILE_WATER;
            }
            else if (height < beachLevel)
            {
                tile->type = (temp < coldTemp) ? TILE_SNOW : TILE_SAND;
            }
            else if (height > mountainLevel)
            {
                tile->type = (temp < 0.45f) ? TILE_SNOW : TILE_STONE;
            }
            else if (height > hillLevel)
            {
                tile->type = (temp < coldTemp) ? TILE_SNOW : TILE_STONE;
            }
            else
            {
                if (temp < coldTemp)
                {
                    tile->type = TILE_SNOW;
                }
                else if (temp > hotTemp && moisture < 0.45f)
                {
                    tile->type = TILE_SAND;
                }
                else
                {
                    tile->type = (moisture > 0.6f) ? TILE_GRASS : TILE_DIRT;
                }
            }
        }
    }

    chunk->generated = 1;
}

ForgeWorld* World_Create(int loadRadiusChunks, int seed)
{
    ForgeWorld* world = malloc(sizeof(ForgeWorld));
    world->seed = seed;
    world->loadRadiusChunks = loadRadiusChunks > 0 ? loadRadiusChunks : WORLD_LOAD_RADIUS_CHUNKS;
    world->chunkCapacity = WORLD_CHUNK_CAPACITY;
    world->chunkCount = 0;
    world->chunkMap = calloc(world->chunkCapacity, sizeof(*world->chunkMap));
    world->rngState = (unsigned int)(seed * 747796405u + 2891336453u);
    world->mobTypes = calloc(WORLD_MAX_MOB_TYPES, sizeof(*world->mobTypes));
    world->mobTypeCount = 0;
    world->mobs = calloc(WORLD_DEFAULT_MOB_CAPACITY, sizeof(*world->mobs));
    world->mobCount = 0;
    world->mobCapacity = WORLD_DEFAULT_MOB_CAPACITY;
    world->mobSpawnCooldown = 0.0f;
    if (!world->chunkMap || !world->mobTypes || !world->mobs)
    {
        free(world->mobs);
        free(world->mobTypes);
        free(world->chunkMap);
        free(world);
        return NULL;
    }

    MobArchetype orange = {0};
    strncpy(orange.name, "orange_rect", sizeof(orange.name) - 1);
    orange.size = 18.0f;
    orange.speed = 70.0f;
    orange.color = (Vec4){1.0f, 0.5f, 0.0f, 1.0f};
    orange.aggroRange = 220.0f;
    orange.attackRange = 12.0f;
    orange.attackDamage = 5.0f;
    orange.attackCooldown = 0.8f;
    orange.maxHP = 20.0f;
    World_RegisterMobType(world, &orange);

    dbg_msg("World", "Created world with seed %d", seed);

    return world;
}

void World_Destroy(ForgeWorld* world)
{
    if (world)
    {
        if (world->chunkMap)
        {
            for (int i = 0; i < world->chunkCapacity; i++)
            {
                if (world->chunkMap[i].state == 1 && world->chunkMap[i].chunk)
                    free(world->chunkMap[i].chunk);
            }
            free(world->chunkMap);
        }
        free(world->mobs);
        free(world->mobTypes);
        free(world);
    }
}

void World_UpdateChunks(ForgeWorld* world, int centerChunkX, int centerChunkY)
{
    int r = world->loadRadiusChunks;
    int minCx = centerChunkX - r;
    int maxCx = centerChunkX + r;
    int minCy = centerChunkY - r;
    int maxCy = centerChunkY + r;

    for (int i = 0; i < world->chunkCapacity; i++)
    {
        if (world->chunkMap[i].state != 1 || world->chunkMap[i].chunk == NULL)
            continue;
        Chunk* c = world->chunkMap[i].chunk;
        int cx = c->cx, cy = c->cy;
        if (cx < minCx || cx > maxCx || cy < minCy || cy > maxCy)
        {
            world->chunkMap[i].key = 0;
            world->chunkMap[i].chunk = NULL;
            world->chunkMap[i].state = 2; /* tombstone */
            world->chunkCount--;
            free(c);
        }
    }

    int loadedThisUpdate = 0;
    for (int cy = minCy; cy <= maxCy; cy++)
    {
        for (int cx = minCx; cx <= maxCx; cx++)
        {
            if (loadedThisUpdate >= WORLD_MAX_CHUNKS_PER_UPDATE)
                return;
            if (World_GetChunk(world, cx, cy))
                continue;
            if (world->chunkCount >= world->chunkCapacity)
                break;
            Chunk* chunk = malloc(sizeof(Chunk));
            if (!chunk)
                continue;
            chunk->cx = cx;
            chunk->cy = cy;
            chunk->generated = 0;
            Chunk_Generate(chunk, world->seed);
            if (!World_InsertChunk(world, cx, cy, chunk))
                free(chunk);
            else
                loadedThisUpdate++;
        }
    }
}

TileType World_GetTile(ForgeWorld* world, int x, int y)
{
    int cx = (int)(x < 0 ? (x - CHUNK_SIZE + 1) / CHUNK_SIZE : x / CHUNK_SIZE);
    int cy = (int)(y < 0 ? (y - CHUNK_SIZE + 1) / CHUNK_SIZE : y / CHUNK_SIZE);

    Chunk* chunk = World_GetChunk(world, cx, cy);
    if (!chunk)
    {
        if (world->chunkCount >= world->chunkCapacity)
            return TILE_EMPTY;

        Chunk* newChunk = malloc(sizeof(Chunk));
        if (!newChunk)
            return TILE_EMPTY;

        newChunk->cx = cx;
        newChunk->cy = cy;
        newChunk->generated = 0;
        Chunk_Generate(newChunk, world->seed);

        if (!World_InsertChunk(world, cx, cy, newChunk))
        {
            free(newChunk);
            return TILE_EMPTY;
        }

        chunk = newChunk;
    }

    if (!chunk->generated)
        Chunk_Generate(chunk, world->seed);

    int lx = x - cx * CHUNK_SIZE;
    int ly = y - cy * CHUNK_SIZE;
    if (lx < 0) lx += CHUNK_SIZE;
    if (ly < 0) ly += CHUNK_SIZE;

    return chunk->tiles[ly * CHUNK_SIZE + lx].type;
}

Vec4 World_GetTileColor(TileType type)
{
    switch (type)
    {
        case TILE_EMPTY:      return (Vec4){0.1f, 0.1f, 0.1f, 1.0f};
        case TILE_GRASS:      return (Vec4){0.2f, 0.8f, 0.2f, 1.0f};
        case TILE_DIRT:       return (Vec4){0.6f, 0.4f, 0.2f, 1.0f};
        case TILE_STONE:      return (Vec4){0.5f, 0.5f, 0.5f, 1.0f};
        case TILE_SAND:       return (Vec4){0.9f, 0.8f, 0.4f, 1.0f};
        case TILE_WATER:      return (Vec4){0.2f, 0.6f, 1.0f, 1.0f};
        case TILE_DEEP_WATER: return (Vec4){0.1f, 0.4f, 0.8f, 1.0f};
        case TILE_SNOW:       return (Vec4){0.92f, 0.95f, 0.98f, 1.0f};
        case TILE_ICE:        return (Vec4){0.7f, 0.9f, 1.0f, 1.0f};
        default:              return (Vec4){1.0f, 0.0f, 1.0f, 1.0f};
    }
}
