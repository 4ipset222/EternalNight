#ifndef __GAME_LOGIC_H__
#define __GAME_LOGIC_H__

#include "world.h"
#include "player.h"
#include "inventory.h"
#include "net/protocol.h"
#include "engine/forge.h"
#include "game_input.h"

void UpdateSingleplayerGame(Player& player, World* world, const Camera2D& camera,
                           Inventory& inventory, Texture2D* weaponSprite, bool uiBlockInput, float dt);

void UpdateCaveState(Player& player, World* world, bool& inCave,
                    float& caveEntranceX, float& caveEntranceY, bool multiplayerActive);

void UpdateDayNightCycle(bool& isNight, float& cycleTimer, float DAY_DURATION,
                        float NIGHT_DURATION, float NIGHT_FADE, float& fogStrength, float dt);

#endif // __GAME_LOGIC_H__
