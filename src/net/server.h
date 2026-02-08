#ifndef __NET_SERVER_H__
#define __NET_SERVER_H__

#include "net.h"
#include "protocol.h"
#include "engine/worldgen.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct ServerClient
{
    bool connected;
    uint8_t id;
    NetSocket sock;
    NetRecvBuffer recv;
    NetSendBuffer send;

    NetInputState input;
    float x;
    float y;
    float hp;

    bool isAttacking;
    float attackProgress;
    float attackCooldownTimer;
    float attackDirX;
    float attackDirY;
    float attackBaseAngle;
    bool attackQueued;
    bool attackBuffered;
    uint16_t lastInputSeq;
} ServerClient;

typedef struct ServerState
{
    bool running;
    uint16_t port;
    int seed;

    float cycleTimer;
    bool isNight;

    float snapshotTimer;

    NetSocket listenSock;
    ServerClient clients[NET_MAX_PLAYERS];

    ForgeWorld* world;
    float tileSize;
    float playerRadius;
} ServerState;

bool Server_Init(ServerState* s, uint16_t port, int seed);
void Server_Shutdown(ServerState* s);
void Server_SetWorld(ServerState* s, ForgeWorld* world, float tileSize, float playerRadius);
void Server_Update(ServerState* s, float dt);
int  Server_GetSnapshot(ServerState* s, NetPlayerState* outPlayers, int maxPlayers, bool* outIsNight, float* outCycleTimer);
void Server_Broadcast(ServerState* s, const uint8_t* data, int len);

#ifdef __cplusplus
}
#endif

#endif // __NET_SERVER_H__
