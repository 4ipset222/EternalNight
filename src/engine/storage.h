#ifndef __FORGE_STORAGE_H__
#define __FORGE_STORAGE_H__

#include <stdbool.h>
#include "worldgen.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct GameSaveState
{
    float playerX;
    float playerY;
    float playerHP;
    int inCave;
    int isNight;
    float cycleTimer;
    float fogStrength;
    float caveEntranceX;
    float caveEntranceY;
} GameSaveState;

bool Storage_SaveGame(const ForgeWorld* world, const GameSaveState* state);
bool Storage_LoadGame(ForgeWorld* world, GameSaveState* outState);
bool Storage_HasSave(void);

#ifdef __cplusplus
}
#endif

#endif // __FORGE_STORAGE_H__
