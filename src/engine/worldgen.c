#include "worldgen.h"
#include "forgesystem.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <omp.h>

#define NUM_THREADS 4

static inline long long ChunkKey(int cx, int cy, int mode)
{
    unsigned long long x = (unsigned int)(int)cx;
    unsigned long long y = (unsigned int)(int)cy;
    unsigned long long m = (unsigned int)(mode ? 1 : 0);
    return (long long)((x << 33) ^ (y << 1) ^ m);
}

static unsigned int ChunkHash(long long key, unsigned int cap)
{
    unsigned long long k = (unsigned long long)key;
    return (unsigned int)((k * 2654435761ULL) % (unsigned long long)cap);
}

static unsigned int World_Hash3(int a, int b, int c)
{
    unsigned int x = (unsigned int)a * 0x9E3779B9u;
    unsigned int y = (unsigned int)b * 0x85EBCA6Bu;
    unsigned int z = (unsigned int)c * 0xC2B2AE35u;
    unsigned int h = x ^ y ^ z;
    h ^= h >> 16;
    h *= 0x7FEB352Du;
    h ^= h >> 15;
    h *= 0x846CA68Bu;
    h ^= h >> 16;
    return h;
}

static Chunk* World_GetChunk(ForgeWorld* world, int cx, int cy, int mode)
{
    long long key = ChunkKey(cx, cy, mode);
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

static int World_InsertChunk(ForgeWorld* world, int cx, int cy, int mode, Chunk* chunk)
{
    if (world->chunkCount >= world->chunkCapacity)
        return 0;
    long long key = ChunkKey(cx, cy, mode);
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

static void GetBiomeData(int seed, float wx, float wy, float* out_height, float* out_temp, float* out_moisture)
{
    Perlin_Init(seed);

    float scale = 70.0f;
    float invScale = 1.0f / scale;

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

    if (out_height) *out_height = height;
    if (out_temp) *out_temp = temp;
    if (out_moisture) *out_moisture = moisture;
}

const char* World_GetBiomeName(ForgeWorld* world, int x, int y)
{
    if (!world) return "Unknown";

    float height, temp, moisture;
    GetBiomeData(world->seed, (float)x, (float)y, &height, &temp, &moisture);

    float oceanLevel = 0.38f;
    float shallowWaterLevel = oceanLevel + 0.03f;
    float beachLevel = oceanLevel + 0.06f;
    float hillLevel = 0.62f;
    float mountainLevel = 0.72f;
    float coldTemp = 0.35f;
    float hotTemp = 0.68f;

    if (height < oceanLevel)
        return "Deep Ocean";
    else if (height < shallowWaterLevel)
        return (temp < coldTemp) ? "Frozen Ocean" : "Shallow Ocean";
    else if (height < beachLevel)
        return (temp < coldTemp) ? "Snowy Beach" : "Sandy Beach";
    else if (height > mountainLevel)
        return (temp < 0.45f) ? "Snow Mountain" : "Stone Mountain";
    else if (height > hillLevel)
        return (temp < coldTemp) ? "Snowy Hills" : "Rocky Hills";
    else
    {
        if (temp < coldTemp)
            return "Frozen Tundra";
        else if (temp > hotTemp && moisture < 0.45f)
            return "Desert";
        else
            return (moisture > 0.6f) ? "Forest" : "Grassland";
    }
}

void Chunk_Generate(Chunk* chunk, int seed, bool isCave, float waterAmount, float stoneAmount, float caveAmount)
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
    
    float waterModifier = waterAmount * 0.12f;
    float adjustedOceanLevel = oceanLevel - 0.06f + waterModifier;
    float adjustedShallowLevel = adjustedOceanLevel + 0.03f;
    float adjustedBeachLevel = adjustedOceanLevel + 0.06f;

    int baseX = chunk->cx * CHUNK_SIZE;
    int baseY = chunk->cy * CHUNK_SIZE;
    int entranceCandidates[CHUNK_SIZE * CHUNK_SIZE];
    int entranceCandidateCount = 0;

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

            if (isCave)
            {
                float tunnel = FractalPerlin2D(nx * 0.26f + 100.0f, ny * 0.26f - 73.0f, 3, 0.55f);
                float chamber = FractalPerlin2D(nx * 0.06f - 41.0f, ny * 0.06f + 59.0f, 2, 0.5f);
                float openValue = tunnel * 0.85f + chamber * 0.15f;
                float openThreshold = 0.67f - caveAmount * 0.07f;

                if (openValue > openThreshold)
                {
                    float puddle = FractalPerlin2D(nx * 0.33f + 17.0f, ny * 0.33f - 12.0f, 1, 0.5f);
                    tile->type = (puddle < 0.10f) ? TILE_WATER : TILE_DIRT;
                }
                else
                {
                    tile->type = TILE_STONE;
                }
                continue;
            }

            if (height < adjustedOceanLevel)
            {
                tile->type = TILE_DEEP_WATER;
            }
            else if (height < adjustedShallowLevel)
            {
                tile->type = (temp < coldTemp) ? TILE_ICE : TILE_WATER;
            }
            else if (height < adjustedBeachLevel)
            {
                tile->type = (temp < coldTemp) ? TILE_SNOW : TILE_SAND;
            }
            else if (height > mountainLevel)
            {
                float stoneProb = FractalPerlin2D(nx * 0.06f, ny * 0.06f, 1, 0.5f);
                float stoneThreshold = 0.3f + stoneAmount * 0.3f;
                if (stoneProb > stoneThreshold)
                {
                    tile->type = (temp < 0.45f) ? TILE_SNOW : TILE_STONE;
                }
                else
                {
                    tile->type = (temp < 0.45f) ? TILE_SNOW : TILE_DIRT;
                }
            }
            else if (height > hillLevel)
            {
                float stoneProb = FractalPerlin2D(nx * 0.06f, ny * 0.06f, 1, 0.5f);
                float stoneThreshold = 0.5f + stoneAmount * 0.2f;
                if (stoneProb > stoneThreshold)
                {
                    tile->type = (temp < coldTemp) ? TILE_SNOW : TILE_STONE;
                }
                else
                {
                    tile->type = (temp < coldTemp) ? TILE_SNOW : TILE_DIRT;
                }
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
                    tile->type = (moisture > 0.35f) ? TILE_GRASS : TILE_DIRT;
                }
            }

            if (tile->type == TILE_STONE && moisture > 0.22f && height > hillLevel)
                entranceCandidates[entranceCandidateCount++] = y * CHUNK_SIZE + x;
        }
    }

    if (!isCave)
    {
        float chunkEntranceChance = 0.06f + caveAmount * 0.28f;
        if (abs(chunk->cx) <= 1 && abs(chunk->cy) <= 1)
            chunkEntranceChance = 0.75f;

        unsigned int rollHash = World_Hash3(seed ^ 0x1F123BB5, chunk->cx, chunk->cy);
        float roll = (float)(rollHash & 0x00FFFFFFu) / (float)0x01000000u;
        if (roll < chunkEntranceChance)
        {
            if (entranceCandidateCount > 0)
            {
                unsigned int pickHash = World_Hash3(seed ^ 0x57A9D21F, chunk->cx, chunk->cy);
                int pick = (int)(pickHash % (unsigned int)entranceCandidateCount);
                int idx = entranceCandidates[pick];
                chunk->tiles[idx].type = TILE_CAVE_ENTRANCE;
            }
            else if (abs(chunk->cx) <= 1 && abs(chunk->cy) <= 1)
            {
                int mid = (CHUNK_SIZE / 2) * CHUNK_SIZE + (CHUNK_SIZE / 2);
                TileType t = chunk->tiles[mid].type;
                if (t != TILE_WATER && t != TILE_DEEP_WATER)
                    chunk->tiles[mid].type = TILE_CAVE_ENTRANCE;
            }
        }
    }

    chunk->generated = 1;
}

ForgeWorld* World_Create(int loadRadiusChunks, int seed)
{
    ForgeWorld* world = malloc(sizeof(ForgeWorld));
    world->seed = seed;
    world->isCave = 0;
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

    world->caveEntranceX = 0.0f;
    world->caveEntranceY = 0.0f;
    world->waterAmount = 0.5f;
    world->stoneAmount = 0.5f;
    world->caveAmount = 0.5f;

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

int World_CheckCaveEntrance(ForgeWorld* world, float playerX, float playerY, float radius)
{
    if (!world) return 0;

    float tileSize = 16.0f;
    int px = (int)floorf(playerX / tileSize);
    int py = (int)floorf(playerY / tileSize);
    
    int checkRadius = (int)ceilf(radius / tileSize) + 1;
    for (int dy = -checkRadius; dy <= checkRadius; dy++)
    {
        for (int dx = -checkRadius; dx <= checkRadius; dx++)
        {
            int tx = px + dx;
            int ty = py + dy;
            TileType tile = World_GetTile(world, tx, ty);
            if (tile == TILE_CAVE_ENTRANCE)
            {
                float dx_f = (float)(tx * (int)tileSize + (int)(tileSize * 0.5f)) - playerX;
                float dy_f = (float)(ty * (int)tileSize + (int)(tileSize * 0.5f)) - playerY;
                float distSq = dx_f * dx_f + dy_f * dy_f;
                if (distSq <= radius * radius)
                    return 1;
            }
        }
    }
    return 0;
}

int World_FindNearestCaveEntrance(ForgeWorld* world, float playerX, float playerY, float radius, float* outX, float* outY)
{
    if (!world || !outX || !outY) return 0;

    const float tileSize = 16.0f;
    int px = (int)floorf(playerX / tileSize);
    int py = (int)floorf(playerY / tileSize);

    int checkRadius = (int)ceilf(radius / tileSize) + 1;
    float bestDistSq = radius * radius;
    int found = 0;

    for (int dy = -checkRadius; dy <= checkRadius; dy++)
    {
        for (int dx = -checkRadius; dx <= checkRadius; dx++)
        {
            int tx = px + dx;
            int ty = py + dy;
            TileType tile = World_GetTile(world, tx, ty);
            if (tile != TILE_CAVE_ENTRANCE)
                continue;

            float cx = (float)(tx * (int)tileSize + (int)(tileSize * 0.5f));
            float cy = (float)(ty * (int)tileSize + (int)(tileSize * 0.5f));
            float ddx = cx - playerX;
            float ddy = cy - playerY;
            float distSq = ddx * ddx + ddy * ddy;
            if (distSq <= bestDistSq)
            {
                bestDistSq = distSq;
                *outX = cx;
                *outY = cy;
                found = 1;
            }
        }
    }

    return found;
}

void World_SaveCaveEntrance(ForgeWorld* world, float x, float y)
{
    if (!world) return;
    world->caveEntranceX = x;
    world->caveEntranceY = y;
}

void World_RestoreCaveExit(ForgeWorld* world, float* outX, float* outY)
{
    if (!world || !outX || !outY) return;
    *outX = world->caveEntranceX;
    *outY = world->caveEntranceY;
}

void World_SetCaveMode(ForgeWorld* world, int isCave)
{
    if (!world) return;

    if (world->isCave == isCave)
        return;

    world->isCave = isCave;
}

void World_ReloadChunks(ForgeWorld* world)
{
    if (!world) return;

    for (int i = 0; i < world->chunkCapacity; i++)
    {
        if (world->chunkMap[i].state == 1 && world->chunkMap[i].chunk)
            free(world->chunkMap[i].chunk);

        world->chunkMap[i].state = 0;
        world->chunkMap[i].chunk = NULL;
        world->chunkMap[i].key = 0;
    }

    world->chunkCount = 0;
}

void World_SetWaterAmount(ForgeWorld* world, float amount)
{
    if (!world) return;
    world->waterAmount = amount < 0.0f ? 0.0f : (amount > 1.0f ? 1.0f : amount);
}

void World_SetStoneAmount(ForgeWorld* world, float amount)
{
    if (!world) return;
    world->stoneAmount = amount < 0.0f ? 0.0f : (amount > 1.0f ? 1.0f : amount);
}

void World_SetCaveAmount(ForgeWorld* world, float amount)
{
    if (!world) return;
    world->caveAmount = amount < 0.0f ? 0.0f : (amount > 1.0f ? 1.0f : amount);
}

void World_UpdateChunks(ForgeWorld* world, int centerChunkX, int centerChunkY)
{
    int r = world->loadRadiusChunks;
    int minCx = centerChunkX - r;
    int maxCx = centerChunkX + r;
    int minCy = centerChunkY - r;
    int maxCy = centerChunkY + r;

    int loadedThisUpdate = 0;
    for (int cy = minCy; cy <= maxCy; cy++)
    {
        for (int cx = minCx; cx <= maxCx; cx++)
        {
            if (loadedThisUpdate >= WORLD_MAX_CHUNKS_PER_UPDATE)
                return;
            if (World_GetChunk(world, cx, cy, world->isCave))
                continue;
            if (world->chunkCount >= world->chunkCapacity)
                break;
            Chunk* chunk = malloc(sizeof(Chunk));
            if (!chunk)
                continue;
            chunk->cx = cx;
            chunk->cy = cy;
            chunk->generated = 0;
            Chunk_Generate(chunk, world->seed, world->isCave, world->waterAmount, world->stoneAmount, world->caveAmount);
            if (!World_InsertChunk(world, cx, cy, world->isCave, chunk))
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

    Chunk* chunk = World_GetChunk(world, cx, cy, world->isCave);
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
        Chunk_Generate(newChunk, world->seed, world->isCave, world->waterAmount, world->stoneAmount, world->caveAmount);

        if (!World_InsertChunk(world, cx, cy, world->isCave, newChunk))
        {
            free(newChunk);
            return TILE_EMPTY;
        }

        chunk = newChunk;
    }

    if (!chunk->generated)
        Chunk_Generate(chunk, world->seed, world->isCave, world->waterAmount, world->stoneAmount, world->caveAmount);

    int lx = x - cx * CHUNK_SIZE;
    int ly = y - cy * CHUNK_SIZE;
    if (lx < 0) lx += CHUNK_SIZE;
    if (ly < 0) ly += CHUNK_SIZE;

    return chunk->tiles[ly * CHUNK_SIZE + lx].type;
}

int World_SetTile(ForgeWorld* world, int x, int y, TileType type)
{
    if (!world) return 0;
    
    int cx = (int)(x < 0 ? (x - CHUNK_SIZE + 1) / CHUNK_SIZE : x / CHUNK_SIZE);
    int cy = (int)(y < 0 ? (y - CHUNK_SIZE + 1) / CHUNK_SIZE : y / CHUNK_SIZE);

    Chunk* chunk = World_GetChunk(world, cx, cy, world->isCave);
    if (!chunk)
    {
        if (world->chunkCount >= world->chunkCapacity)
            return 0;

        Chunk* newChunk = malloc(sizeof(Chunk));
        if (!newChunk)
            return 0;

        newChunk->cx = cx;
        newChunk->cy = cy;
        newChunk->generated = 0;
        Chunk_Generate(newChunk, world->seed, world->isCave, world->waterAmount, world->stoneAmount, world->caveAmount);

        if (!World_InsertChunk(world, cx, cy, world->isCave, newChunk))
        {
            free(newChunk);
            return 0;
        }

        chunk = newChunk;
    }

    int lx = x - cx * CHUNK_SIZE;
    int ly = y - cy * CHUNK_SIZE;
    if (lx < 0) lx += CHUNK_SIZE;
    if (ly < 0) ly += CHUNK_SIZE;

    if (lx >= 0 && lx < CHUNK_SIZE && ly >= 0 && ly < CHUNK_SIZE)
    {
        chunk->tiles[ly * CHUNK_SIZE + lx].type = type;
        return 1;
    }
    return 0;
}

Vec4 World_GetTileColor(TileType type)
{
    switch (type)
    {
        case TILE_EMPTY:           return (Vec4){0.1f, 0.1f, 0.1f, 1.0f};
        case TILE_GRASS:           return (Vec4){0.2f, 0.8f, 0.2f, 1.0f};
        case TILE_DIRT:            return (Vec4){0.6f, 0.4f, 0.2f, 1.0f};
        case TILE_STONE:           return (Vec4){0.5f, 0.5f, 0.5f, 1.0f};
        case TILE_SAND:            return (Vec4){0.9f, 0.8f, 0.4f, 1.0f};
        case TILE_WATER:           return (Vec4){0.2f, 0.6f, 1.0f, 1.0f};
        case TILE_DEEP_WATER:      return (Vec4){0.1f, 0.4f, 0.8f, 1.0f};
        case TILE_SNOW:            return (Vec4){0.92f, 0.95f, 0.98f, 1.0f};
        case TILE_ICE:             return (Vec4){0.7f, 0.9f, 1.0f, 1.0f};
        case TILE_CAVE_ENTRANCE:   return (Vec4){0.2f, 0.2f, 0.15f, 1.0f};
        default:                   return (Vec4){1.0f, 0.0f, 1.0f, 1.0f};
    }
}
