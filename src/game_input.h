#ifndef __GAME_INPUT_H__
#define __GAME_INPUT_H__

#include "world.h"
#include "player.h"
#include "inventory.h"
#include "engine/forge.h"
#include "multiplayer_types.h"
#include "net/protocol.h"

void HandleBlockPlacement(const Player& player, const Camera2D& camera, 
                         World* world, Inventory& inventory, bool uiBlockInput);

void HandlePlayerAttack(Player& player, const Camera2D& camera, 
                       World* world, Texture2D* weaponSprite, bool uiBlockInput);

NetInputState GetNetInput();

void UpdateCameraZoom(Camera2D& camera, float dt);

#endif // __GAME_INPUT_H__
