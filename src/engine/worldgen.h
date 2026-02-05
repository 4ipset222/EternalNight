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

typedef struct
{
    int seed;

    int loadRadiusChunks;
    int chunkCapacity;
    int chunkCount;
    ChunkSlot* chunkMap;
} ForgeWorld;

void Chunk_Generate(Chunk* chunk, int seed);

ForgeWorld* World_Create(int loadRadiusChunks, int seed);
void World_Destroy(ForgeWorld* world);

void World_UpdateChunks(ForgeWorld* world, int centerChunkX, int centerChunkY);

TileType World_GetTile(ForgeWorld* world, int x, int y);
Vec4 World_GetTileColor(TileType type);

#ifdef __cplusplus
}
#endif

#endif // __FORGE_WORLDGEN_H__
