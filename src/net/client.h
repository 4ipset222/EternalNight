#ifndef __NET_CLIENT_H__
#define __NET_CLIENT_H__

#include "net.h"
#include "protocol.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct ClientState
{
    bool connected;
    uint8_t playerId;
    int seed;

    bool isNight;
    float cycleTimer;

    NetSocket sock;
    NetRecvBuffer recv;
    NetSendBuffer send;

    NetPlayerState players[NET_MAX_PLAYERS];
    int playerCount;

    NetMobState mobs[NET_MAX_MOBS];
    int mobCount;
} ClientState;

bool Client_Init(ClientState* c);
bool Client_Connect(ClientState* c, const char* host, uint16_t port);
void Client_Disconnect(ClientState* c);
void Client_Update(ClientState* c, float dt);
void Client_SendInput(ClientState* c, const NetInputState* in);
int  Client_GetSnapshot(ClientState* c, NetPlayerState* outPlayers, int maxPlayers, bool* outIsNight, float* outCycleTimer);
bool Client_IsConnected(ClientState* c);

#ifdef __cplusplus
}
#endif

#endif // __NET_CLIENT_H__
