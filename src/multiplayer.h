#ifndef __MULTIPLAYER_H__
#define __MULTIPLAYER_H__

#include <vector>
#include <cstdint>
#include "world.h"
#include "player.h"
#include "net/server.h"
#include "net/client.h"
#include "multiplayer_types.h"
#include "net/protocol.h"

struct RemotePlayer
{
    uint8_t id;
    Player player;
};

struct PendingInput
{
    uint16_t seq;
    float dt;
    float moveX;
    float moveY;
};

Color ColorForId(uint8_t id);

RemotePlayer* FindRemote(std::vector<RemotePlayer>& list, uint8_t id);

void UpdateMultiplayerInput(Player& player, World* world, NetInputState& netInput,
                           Texture2D* weaponSprite, bool uiBlockInput,
                           float& attackBufferTimer, float& lastAttackDirX, float& lastAttackDirY,
                           const Camera2D& camera, float dt);

void UpdateClientMovement(Player& player, World* world, NetInputState& netInput,
                         uint16_t& inputSeq, std::vector<PendingInput>& pendingInputs, float dt);

void ProcessNetworkSnapshot(const NetPlayerState* snap, int snapCount, uint8_t localId,
                           Player& player, std::vector<RemotePlayer>& remotePlayers,
                           std::vector<PendingInput>& pendingInputs, World* world,
                           Texture2D* swordSprite, MpMode mpMode, float dt);

void UpdateHostMobs(Player& player, World* world, ServerState& server, 
                   float& mobSyncTimer, bool isNight, float dt);

#endif // __MULTIPLAYER_H__
