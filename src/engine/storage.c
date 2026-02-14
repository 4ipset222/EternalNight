#include "storage.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <SDL2/SDL.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

#define SAVE_PATH "saves/world.bin"
#define SAVE_MAGIC 0x56534E45u /* ENSV */
#define SAVE_VERSION 1u

typedef struct SaveHeader
{
    unsigned int magic;
    unsigned int version;
    int seed;
    int loadRadiusChunks;
    int isCave;
    float waterAmount;
    float stoneAmount;
    float caveAmount;
    float caveEntranceX;
    float caveEntranceY;
    int chunkCount;
} SaveHeader;

typedef struct SaveChunkEntry
{
    int mode;
    Chunk chunk;
} SaveChunkEntry;

static unsigned int Storage_HashKey(long long key, unsigned int cap)
{
    unsigned long long k = (unsigned long long)key;
    return (unsigned int)((k * 2654435761ULL) % (unsigned long long)cap);
}

static long long Storage_ChunkKey(int cx, int cy, int mode)
{
    unsigned long long x = (unsigned int)(int)cx;
    unsigned long long y = (unsigned int)(int)cy;
    unsigned long long m = (unsigned int)(mode ? 1 : 0);
    return (long long)((x << 33) ^ (y << 1) ^ m);
}

static int Storage_InsertChunkWithMode(ForgeWorld* world, int mode, Chunk* chunk)
{
    if (!world || !chunk)
        return 0;
    if (world->chunkCount >= world->chunkCapacity)
        return 0;

    long long key = Storage_ChunkKey(chunk->cx, chunk->cy, mode);
    unsigned int idx = Storage_HashKey(key, (unsigned int)world->chunkCapacity);
    unsigned int tombstone = (unsigned int)-1;

    for (unsigned int n = 0; n < (unsigned int)world->chunkCapacity; ++n)
    {
        unsigned int i = (idx + n) % (unsigned int)world->chunkCapacity;
        if (world->chunkMap[i].state == 1 && world->chunkMap[i].key == key)
            return 0;
        if (world->chunkMap[i].state == 2 && tombstone == (unsigned int)-1)
            tombstone = i;
        if (world->chunkMap[i].state == 0)
        {
            unsigned int dst = (tombstone != (unsigned int)-1) ? tombstone : i;
            world->chunkMap[dst].key = key;
            world->chunkMap[dst].chunk = chunk;
            world->chunkMap[dst].state = 1;
            world->chunkCount++;
            return 1;
        }
    }
    return 0;
}

static void Storage_ClearWorldChunks(ForgeWorld* world)
{
    if (!world || !world->chunkMap)
        return;
    for (int i = 0; i < world->chunkCapacity; ++i)
    {
        if (world->chunkMap[i].state == 1 && world->chunkMap[i].chunk)
            free(world->chunkMap[i].chunk);
        world->chunkMap[i].state = 0;
        world->chunkMap[i].chunk = NULL;
        world->chunkMap[i].key = 0;
    }
    world->chunkCount = 0;
}

static int Storage_CollectChunks(const ForgeWorld* world, SaveChunkEntry** outEntries, int* outCount)
{
    if (!world || !outEntries || !outCount)
        return 0;

    int count = 0;
    for (int i = 0; i < world->chunkCapacity; ++i)
    {
        if (world->chunkMap[i].state == 1 && world->chunkMap[i].chunk)
            count++;
    }

    SaveChunkEntry* entries = NULL;
    if (count > 0)
    {
        entries = (SaveChunkEntry*)malloc(sizeof(SaveChunkEntry) * (size_t)count);
        if (!entries)
            return 0;
    }

    int at = 0;
    for (int i = 0; i < world->chunkCapacity; ++i)
    {
        if (world->chunkMap[i].state != 1 || !world->chunkMap[i].chunk)
            continue;

        int mode = (int)(world->chunkMap[i].key & 1LL);
        entries[at].mode = mode ? 1 : 0;
        entries[at].chunk = *world->chunkMap[i].chunk;
        at++;
    }

    *outEntries = entries;
    *outCount = count;
    return 1;
}

static char* Storage_GetFallbackPath(void)
{
    char* pref = SDL_GetPrefPath("Eternal Games Inc.", "EternalNight");
    if (!pref)
        return NULL;
    size_t n = strlen(pref) + strlen("saves/world.bin") + 4;
    char* out = (char*)malloc(n);
    if (!out)
    {
        SDL_free(pref);
        return NULL;
    }
#ifdef _WIN32
    snprintf(out, n, "%ssaves\\world.bin", pref);
#else
    snprintf(out, n, "%ssaves/world.bin", pref);
#endif
    SDL_free(pref);
    return out;
}

static bool Storage_WriteBytes(const void* bytes, size_t byteCount)
{
    char* path = Storage_GetFallbackPath();
    if (!path)
        return false;

    char* slash = strrchr(path, '/');
    char* bslash = strrchr(path, '\\');
    char* sep = slash ? slash : bslash;
    if (bslash && (!sep || bslash > sep))
        sep = bslash;
    if (sep)
    {
        char oldSep = *sep;
        *sep = '\0';
#ifdef _WIN32
        _mkdir(path);
#else
        mkdir(path, 0755);
#endif
        *sep = oldSep;
    }

    FILE* f = fopen(path, "wb");
    free(path);
    if (!f)
        return false;
    size_t wr = fwrite(bytes, 1, byteCount, f);
    fclose(f);
    return wr == byteCount;
}

static bool Storage_ReadBytes(void** outBytes, size_t* outByteCount)
{
    if (!outBytes || !outByteCount)
        return false;
    char* path = Storage_GetFallbackPath();
    if (!path)
        return false;
    FILE* f = fopen(path, "rb");
    free(path);
    if (!f)
        return false;

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0)
    {
        fclose(f);
        return false;
    }

    void* bytes = malloc((size_t)sz);
    if (!bytes)
    {
        fclose(f);
        return false;
    }
    size_t rd = fread(bytes, 1, (size_t)sz, f);
    fclose(f);
    if (rd != (size_t)sz)
    {
        free(bytes);
        return false;
    }

    *outBytes = bytes;
    *outByteCount = (size_t)sz;
    return true;
}

bool Storage_HasSave(void)
{
    char* path = Storage_GetFallbackPath();
    if (!path)
        return false;
    FILE* f = fopen(path, "rb");
    free(path);
    if (!f)
        return false;
    fclose(f);
    return true;
}

bool Storage_SaveGame(const ForgeWorld* world, const GameSaveState* state)
{
    if (!world || !state)
        return false;

    SaveChunkEntry* chunks = NULL;
    int chunkCount = 0;
    if (!Storage_CollectChunks(world, &chunks, &chunkCount))
        return false;

    SaveHeader h = {0};
    h.magic = SAVE_MAGIC;
    h.version = SAVE_VERSION;
    h.seed = world->seed;
    h.loadRadiusChunks = world->loadRadiusChunks;
    h.isCave = world->isCave;
    h.waterAmount = world->waterAmount;
    h.stoneAmount = world->stoneAmount;
    h.caveAmount = world->caveAmount;
    h.caveEntranceX = world->caveEntranceX;
    h.caveEntranceY = world->caveEntranceY;
    h.chunkCount = chunkCount;

    size_t totalSize = sizeof(SaveHeader) + sizeof(GameSaveState) + sizeof(SaveChunkEntry) * (size_t)chunkCount;
    unsigned char* bytes = (unsigned char*)malloc(totalSize);
    if (!bytes)
    {
        free(chunks);
        return false;
    }

    memcpy(bytes, &h, sizeof(SaveHeader));
    memcpy(bytes + sizeof(SaveHeader), state, sizeof(GameSaveState));
    if (chunkCount > 0)
        memcpy(bytes + sizeof(SaveHeader) + sizeof(GameSaveState), chunks, sizeof(SaveChunkEntry) * (size_t)chunkCount);

    bool ok = Storage_WriteBytes(bytes, totalSize);
    free(bytes);
    free(chunks);
    return ok;
}

bool Storage_LoadGame(ForgeWorld* world, GameSaveState* outState)
{
    if (!world || !outState)
        return false;

    void* bytes = NULL;
    size_t byteCount = 0;
    if (!Storage_ReadBytes(&bytes, &byteCount))
        return false;

    if (byteCount < sizeof(SaveHeader) + sizeof(GameSaveState))
    {
        free(bytes);
        return false;
    }

    SaveHeader h = {0};
    memcpy(&h, bytes, sizeof(SaveHeader));
    if (h.magic != SAVE_MAGIC || h.version != SAVE_VERSION || h.chunkCount < 0)
    {
        free(bytes);
        return false;
    }

    size_t expected = sizeof(SaveHeader) + sizeof(GameSaveState) + sizeof(SaveChunkEntry) * (size_t)h.chunkCount;
    if (byteCount < expected)
    {
        free(bytes);
        return false;
    }

    memcpy(outState, (unsigned char*)bytes + sizeof(SaveHeader), sizeof(GameSaveState));
    const SaveChunkEntry* chunks = (const SaveChunkEntry*)((const unsigned char*)bytes + sizeof(SaveHeader) + sizeof(GameSaveState));

    Storage_ClearWorldChunks(world);
    world->seed = h.seed;
    world->loadRadiusChunks = h.loadRadiusChunks;
    world->isCave = h.isCave;
    world->rngState = (unsigned int)(world->seed * 747796405u + 2891336453u);
    world->waterAmount = h.waterAmount;
    world->stoneAmount = h.stoneAmount;
    world->caveAmount = h.caveAmount;
    world->caveEntranceX = h.caveEntranceX;
    world->caveEntranceY = h.caveEntranceY;
    world->mobCount = 0;
    world->mobSpawnCooldown = 0.0f;

    for (int i = 0; i < h.chunkCount; ++i)
    {
        Chunk* c = (Chunk*)malloc(sizeof(Chunk));
        if (!c)
            continue;
        *c = chunks[i].chunk;
        if (!Storage_InsertChunkWithMode(world, chunks[i].mode, c))
            free(c);
    }

    free(bytes);
    return true;
}
