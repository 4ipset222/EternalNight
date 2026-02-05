#ifndef __FORGE_WORLDGEN_H__
#define __FORGE_WORLDGEN_H__

#include "perlin.h"
#include "vmath.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define CHUNK_SIZE 32

typedef enum
{
    TILE_EMPTY,
    TILE_GRASS,
    TILE_DIRT,
    TILE_STONE,
    TILE_SAND,
    TILE_WATER,
    TILE_DEEP_WATER
} TileType;

typedef struct
{
    TileType type;
} Tile;

typedef struct
{
    int cx, cy;
    Tile tiles[CHUNK_SIZE * CHUNK_SIZE];
    int generated;
} Chunk;

#define WORLD_CHUNK_CAPACITY 512
#define WORLD_LOAD_RADIUS_CHUNKS 3
#define WORLD_MAX_CHUNKS_PER_UPDATE 2

typedef struct ChunkSlot
{
    long long key;
    Chunk* chunk;
    unsigned char state; /* 0=empty, 1=filled, 2=tombstone */
} ChunkSlot;

typedef struct MobArchetype MobArchetype;
typedef struct Mob Mob;

typedef struct
{
    int seed;

    int loadRadiusChunks;
    int chunkCapacity;
    int chunkCount;
    ChunkSlot* chunkMap;

    unsigned int rngState;

    MobArchetype* mobTypes;
    int mobTypeCount;

    Mob* mobs;
    int mobCount;
    int mobCapacity;
    float mobSpawnCooldown;
} ForgeWorld;

void Chunk_Generate(Chunk* chunk, int seed);

ForgeWorld* World_Create(int loadRadiusChunks, int seed);
void World_Destroy(ForgeWorld* world);

void World_UpdateChunks(ForgeWorld* world, int centerChunkX, int centerChunkY);

TileType World_GetTile(ForgeWorld* world, int x, int y);
Vec4 World_GetTileColor(TileType type);

#define WORLD_MAX_MOB_TYPES 16
#define WORLD_DEFAULT_MOB_CAPACITY 128

typedef struct MobArchetype
{
    char name[32];
    float size;
    float speed;
    Vec4 color;
    float aggroRange;
    float attackRange;
    float attackDamage;
    float attackCooldown;
    float maxHP;
} MobArchetype;

typedef struct Mob
{
    int type;
    float x, y;
    float vx, vy;
    float attackTimer;
    float hp;
} Mob;

int  World_RegisterMobType(ForgeWorld* world, const MobArchetype* archetype);
int  World_SpawnMob(ForgeWorld* world, int type, float x, float y);
void World_UpdateMobs(ForgeWorld* world, float dt, int isNight, float playerX, float playerY, float playerRadius, float* ioPlayerHP);
int  World_PlayerAttack(ForgeWorld* world, float originX, float originY, float dirX, float dirY, float range, float arcCos, float damage);

const Mob* World_GetMobs(const ForgeWorld* world, int* outCount);
const MobArchetype* World_GetMobArchetypes(const ForgeWorld* world, int* outCount);

#ifdef __cplusplus
}
#endif

#endif // __FORGE_WORLDGEN_H__
