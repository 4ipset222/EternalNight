#include "worldgen.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
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

void Chunk_Generate(Chunk* chunk, int seed)
{
    Perlin_Init(seed);

    float scale = 50.0f;
    float invScale = 1.0f / scale;

    float waterLevel = 0.4f;
    float beachLevel = 0.45f;
    float stoneLevel = 0.65f;

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

            float h = FractalPerlin2D(nx, ny, 3, 0.5f);
            float d = Perlin2D(nx * 2.0f, ny * 2.0f);
            float v = h * 0.85f + d * 0.15f;

            Tile* tile = &chunk->tiles[y * CHUNK_SIZE + x];

            if (v < waterLevel)
                tile->type = TILE_DEEP_WATER;
            else if (v < waterLevel + 0.02f)
                tile->type = TILE_WATER;
            else if (v < beachLevel)
                tile->type = TILE_SAND;
            else if (v < stoneLevel)
                tile->type = (d > 0.5f) ? TILE_GRASS : TILE_DIRT;
            else
                tile->type = TILE_STONE;
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
    if (!world->chunkMap)
    {
        free(world);
        return NULL;
    }
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
        default:              return (Vec4){1.0f, 0.0f, 1.0f, 1.0f};
    }
}
