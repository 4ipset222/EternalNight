#include "client.h"
#include <string.h>

static bool SendHello(ClientState* c)
{
    if (!c || !Net_IsValid(c->sock)) return false;
    uint8_t msg[16];
    uint16_t payloadLen = 4;
    uint16_t totalLen = (uint16_t)(1 + payloadLen);
    msg[0] = (uint8_t)(totalLen & 0xFF);
    msg[1] = (uint8_t)((totalLen >> 8) & 0xFF);
    msg[2] = (uint8_t)MSG_HELLO;
    int version = NET_PROTOCOL_VERSION;
    memcpy(&msg[3], &version, sizeof(int));

    return NetSendBuffer_Append(&c->send, msg, (int)(2 + totalLen));
}

static void HandleSnapshot(ClientState* c, const uint8_t* payload, int payloadLen)
{
    if (!c || !payload || payloadLen < 2) return;
    int offset = 0;

    c->isNight = payload[offset++] ? true : false;
    if (offset + (int)sizeof(float) > payloadLen) return;
    memcpy(&c->cycleTimer, payload + offset, sizeof(float));
    offset += sizeof(float);

    if (offset >= payloadLen) return;
    uint8_t count = payload[offset++];
    c->playerCount = 0;

    for (uint8_t i = 0; i < count; ++i)
    {
        if (offset + 2 + (int)(sizeof(float) * 7) + (int)sizeof(uint16_t) > payloadLen)
            break;
        NetPlayerState* p = &c->players[c->playerCount++];
        p->id = payload[offset++];
        memcpy(&p->x, payload + offset, sizeof(float)); offset += sizeof(float);
        memcpy(&p->y, payload + offset, sizeof(float)); offset += sizeof(float);
        memcpy(&p->hp, payload + offset, sizeof(float)); offset += sizeof(float);
        p->isAttacking = payload[offset++];
        memcpy(&p->attackProgress, payload + offset, sizeof(float)); offset += sizeof(float);
        memcpy(&p->attackDirX, payload + offset, sizeof(float)); offset += sizeof(float);
        memcpy(&p->attackDirY, payload + offset, sizeof(float)); offset += sizeof(float);
        memcpy(&p->attackBaseAngle, payload + offset, sizeof(float)); offset += sizeof(float);
        memcpy(&p->lastInputSeq, payload + offset, sizeof(uint16_t)); offset += sizeof(uint16_t);
    }
}

static void HandleMobs(ClientState* c, const uint8_t* payload, int payloadLen)
{
    if (!c || !payload || payloadLen < 1) return;
    int offset = 0;
    uint8_t count = payload[offset++];
    c->mobCount = 0;

    for (uint8_t i = 0; i < count && c->mobCount < NET_MAX_MOBS; ++i)
    {
        if (offset + 1 + (int)(sizeof(float) * 3) > payloadLen)
            break;
        NetMobState* m = &c->mobs[c->mobCount++];
        m->type = payload[offset++];
        memcpy(&m->x, payload + offset, sizeof(float)); offset += sizeof(float);
        memcpy(&m->y, payload + offset, sizeof(float)); offset += sizeof(float);
        memcpy(&m->hp, payload + offset, sizeof(float)); offset += sizeof(float);
    }
}

bool Client_Init(ClientState* c)
{
    if (!c) return false;
    memset(c, 0, sizeof(*c));
    return Net_Init();
}

bool Client_Connect(ClientState* c, const char* host, uint16_t port)
{
    if (!c) return false;

    c->sock = Net_CreateTCP();
    if (!Net_IsValid(c->sock))
        return false;

    if (!Net_Connect(&c->sock, host, port))
    {
        Net_Close(&c->sock);
        return false;
    }

    Net_SetNonBlocking(&c->sock, true);
    NetRecvBuffer_Init(&c->recv);
    NetSendBuffer_Init(&c->send);
    c->connected = true;

    SendHello(c);
    return true;
}

void Client_Disconnect(ClientState* c)
{
    if (!c) return;
    if (Net_IsValid(c->sock))
        Net_Close(&c->sock);
    c->connected = false;
}

void Client_Update(ClientState* c, float dt)
{
    (void)dt;
    if (!c || !c->connected) return;

    uint8_t temp[512];
    for (;;)
    {
        int r = Net_Recv(&c->sock, temp, (int)sizeof(temp));
        if (r > 0)
        {
            if (!NetRecvBuffer_Push(&c->recv, temp, r))
            {
                Client_Disconnect(c);
                return;
            }

            uint8_t type;
            uint8_t payload[NET_MAX_MESSAGE];
            int payloadLen;
            while (NetRecvBuffer_PopMessage(&c->recv, &type, payload, &payloadLen))
            {
                if (type == MSG_WELCOME)
                {
                    if (payloadLen >= 1 + 1 + (int)sizeof(int))
                    {
                        c->playerId = payload[0];
                        memcpy(&c->seed, payload + 2, sizeof(int));
                    }
                }
                else if (type == MSG_SNAPSHOT)
                {
                    HandleSnapshot(c, payload, payloadLen);
                }
                else if (type == MSG_MOBS)
                {
                    HandleMobs(c, payload, payloadLen);
                }
            }
        }
        else if (r == 0)
        {
            Client_Disconnect(c);
            return;
        }
        else
        {
            if (!Net_WouldBlock())
            {
                Client_Disconnect(c);
            }
            break;
        }
    }

    NetSendBuffer_Flush(&c->send, &c->sock);
}

void Client_SendInput(ClientState* c, const NetInputState* in)
{
    if (!c || !c->connected || !in) return;

    uint8_t msg[64];
    uint16_t payloadLen = (uint16_t)(sizeof(float) * 4 + 1 + sizeof(uint16_t));
    uint16_t totalLen = (uint16_t)(1 + payloadLen);
    msg[0] = (uint8_t)(totalLen & 0xFF);
    msg[1] = (uint8_t)((totalLen >> 8) & 0xFF);
    msg[2] = (uint8_t)MSG_INPUT;
    int offset = 3;
    memcpy(msg + offset, &in->seq, sizeof(uint16_t)); offset += sizeof(uint16_t);
    memcpy(msg + offset, &in->moveX, sizeof(float)); offset += sizeof(float);
    memcpy(msg + offset, &in->moveY, sizeof(float)); offset += sizeof(float);
    msg[offset++] = in->attack;
    memcpy(msg + offset, &in->attackDirX, sizeof(float)); offset += sizeof(float);
    memcpy(msg + offset, &in->attackDirY, sizeof(float)); offset += sizeof(float);

    NetSendBuffer_Append(&c->send, msg, (int)(2 + totalLen));
}

int Client_GetSnapshot(ClientState* c, NetPlayerState* outPlayers, int maxPlayers, bool* outIsNight, float* outCycleTimer)
{
    if (!c || !outPlayers || maxPlayers <= 0) return 0;

    int count = c->playerCount;
    if (count > maxPlayers) count = maxPlayers;
    for (int i = 0; i < count; ++i)
        outPlayers[i] = c->players[i];

    if (outIsNight) *outIsNight = c->isNight;
    if (outCycleTimer) *outCycleTimer = c->cycleTimer;
    return count;
}

bool Client_IsConnected(ClientState* c)
{
    return c && c->connected;
}
