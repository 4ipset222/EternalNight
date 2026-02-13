#include "server.h"
#include <string.h>
#include <math.h>

static const float PLAYER_SPEED = 200.0f;
static const float ATTACK_DURATION = 0.30f;
static const float ATTACK_COOLDOWN = 0.40f;
static const float PLAYER_MAX_HP = 100.0f;
static const float PLAYER_RESPAWN_TIME = 5.0f;
static const float DAY_DURATION = 5.0f;
static const float NIGHT_DURATION = 180.0f;

static void ResetClient(ServerClient* c)
{
    memset(c, 0, sizeof(*c));
#ifdef _WIN32
    c->sock.handle = INVALID_SOCKET;
#else
    c->sock.handle = -1;
#endif
    NetRecvBuffer_Init(&c->recv);
    NetSendBuffer_Init(&c->send);
    c->hp = PLAYER_MAX_HP;
    c->isDead = false;
    c->respawnTimer = 0.0f;
    c->attackDirX = 1.0f;
    c->attackDirY = 0.0f;
    c->attackQueued = false;
    c->attackBuffered = false;
    c->lastInputSeq = 0;
}

static bool SendWelcome(ServerClient* c, uint8_t id, int seed)
{
    if (!c || !Net_IsValid(c->sock)) return false;
    uint8_t msg[32];
    uint16_t payloadLen = 1 + 1 + 4;
    uint16_t totalLen = (uint16_t)(1 + payloadLen);
    msg[0] = (uint8_t)(totalLen & 0xFF);
    msg[1] = (uint8_t)((totalLen >> 8) & 0xFF);
    msg[2] = (uint8_t)MSG_WELCOME;
    msg[3] = id;
    msg[4] = NET_MAX_PLAYERS;
    memcpy(&msg[5], &seed, sizeof(int));

    return NetSendBuffer_Append(&c->send, msg, (int)(2 + totalLen));
}

static void SendSnapshot(ServerState* s, ServerClient* c)
{
    if (!s || !c || !Net_IsValid(c->sock)) return;

    uint8_t buffer[NET_MAX_MESSAGE];
    int offset = 0;

    offset += 2; // length placeholder
    buffer[offset++] = (uint8_t)MSG_SNAPSHOT;
    buffer[offset++] = s->isNight ? 1 : 0;
    memcpy(buffer + offset, &s->cycleTimer, sizeof(float));
    offset += sizeof(float);

    uint8_t count = 0;
    int countPos = offset;
    buffer[offset++] = 0;

    for (int i = 0; i < NET_MAX_PLAYERS; ++i)
    {
        ServerClient* p = &s->clients[i];
        if (!p->connected) continue;
        buffer[offset++] = p->id;
        memcpy(buffer + offset, &p->x, sizeof(float)); offset += sizeof(float);
        memcpy(buffer + offset, &p->y, sizeof(float)); offset += sizeof(float);
        memcpy(buffer + offset, &p->hp, sizeof(float)); offset += sizeof(float);
        buffer[offset++] = p->isDead ? 1 : 0;
        memcpy(buffer + offset, &p->respawnTimer, sizeof(float)); offset += sizeof(float);
        buffer[offset++] = p->isAttacking ? 1 : 0;
        memcpy(buffer + offset, &p->attackProgress, sizeof(float)); offset += sizeof(float);
        memcpy(buffer + offset, &p->attackDirX, sizeof(float)); offset += sizeof(float);
        memcpy(buffer + offset, &p->attackDirY, sizeof(float)); offset += sizeof(float);
        memcpy(buffer + offset, &p->attackBaseAngle, sizeof(float)); offset += sizeof(float);
        memcpy(buffer + offset, &p->lastInputSeq, sizeof(uint16_t)); offset += sizeof(uint16_t);
        count++;

        if (offset + 32 >= (int)sizeof(buffer))
            break;
    }

    buffer[countPos] = count;

    uint16_t totalLen = (uint16_t)(offset - 2);
    buffer[0] = (uint8_t)(totalLen & 0xFF);
    buffer[1] = (uint8_t)((totalLen >> 8) & 0xFF);

    NetSendBuffer_Append(&c->send, buffer, offset);
}

bool Server_Init(ServerState* s, uint16_t port, int seed)
{
    if (!s) return false;
    memset(s, 0, sizeof(*s));

    if (!Net_Init())
        return false;

    s->listenSock = Net_CreateTCP();
    if (!Net_IsValid(s->listenSock))
        return false;

    if (!Net_Bind(&s->listenSock, port))
        return false;
    if (!Net_Listen(&s->listenSock))
        return false;
    Net_SetNonBlocking(&s->listenSock, true);

    for (int i = 0; i < NET_MAX_PLAYERS; ++i)
        ResetClient(&s->clients[i]);

    s->running = true;
    s->port = port;
    s->seed = seed;
    s->cycleTimer = 0.0f;
    s->isNight = false;
    s->snapshotTimer = 0.0f;
    s->world = NULL;
    s->tileSize = 16.0f;
    s->playerRadius = 8.0f;

    return true;
}

void Server_Shutdown(ServerState* s)
{
    if (!s) return;
    for (int i = 0; i < NET_MAX_PLAYERS; ++i)
    {
        if (Net_IsValid(s->clients[i].sock))
            Net_Close(&s->clients[i].sock);
    }
    if (Net_IsValid(s->listenSock))
        Net_Close(&s->listenSock);

    s->running = false;
    Net_Shutdown();
}

void Server_SetWorld(ServerState* s, ForgeWorld* world, float tileSize, float playerRadius)
{
    if (!s) return;
    s->world = world;
    s->tileSize = tileSize;
    s->playerRadius = playerRadius;
}

static void HandleClientMessage(ServerState* s, ServerClient* c, uint8_t type, const uint8_t* payload, int payloadLen)
{
    (void)payloadLen;
    if (!s || !c) return;

    if (type == MSG_HELLO)
    {
        SendWelcome(c, c->id, s->seed);
    }
    else if (type == MSG_INPUT)
    {
        if (payload && payloadLen >= (int)(sizeof(float) * 4 + 1 + sizeof(uint16_t)))
        {
            int offset = 0;
            memcpy(&c->input.seq, payload + offset, sizeof(uint16_t)); offset += sizeof(uint16_t);
            memcpy(&c->input.moveX, payload + offset, sizeof(float)); offset += sizeof(float);
            memcpy(&c->input.moveY, payload + offset, sizeof(float)); offset += sizeof(float);
            c->input.attack = payload[offset++];
            memcpy(&c->input.attackDirX, payload + offset, sizeof(float)); offset += sizeof(float);
            memcpy(&c->input.attackDirY, payload + offset, sizeof(float)); offset += sizeof(float);
            c->lastInputSeq = c->input.seq;

            if (c->input.attack)
            {
                float dirLenSq = c->input.attackDirX * c->input.attackDirX + c->input.attackDirY * c->input.attackDirY;
                if (dirLenSq > 0.0001f)
                {
                    float inv = 1.0f / sqrtf(dirLenSq);
                    c->attackDirX = c->input.attackDirX * inv;
                    c->attackDirY = c->input.attackDirY * inv;
                    c->attackBaseAngle = atan2f(c->attackDirY, c->attackDirX);
                    c->attackBuffered = true;
                }
            }
        }
    }
}

static void UpdatePlayer(ServerState* s, ServerClient* p, float dt)
{
    if (!p) return;
    if (p->isDead)
    {
        p->isAttacking = false;
        p->attackBuffered = false;
        p->attackQueued = false;
        p->attackProgress = 0.0f;
        return;
    }

    float mx = p->input.moveX;
    float my = p->input.moveY;
    float lenSq = mx * mx + my * my;
    if (lenSq > 1.0f)
    {
        float inv = 1.0f / sqrtf(lenSq);
        mx *= inv;
        my *= inv;
    }

    float dx = mx * PLAYER_SPEED * dt;
    float dy = my * PLAYER_SPEED * dt;
    float nx = p->x;
    float ny = p->y;
    if (s && s->world)
    {
        World_MoveWithCollision(s->world, s->tileSize, s->playerRadius, &nx, &ny, dx, dy);
    }
    else
    {
        nx += dx;
        ny += dy;
    }
    p->x = nx;
    p->y = ny;

    if (p->attackCooldownTimer > 0.0f)
        p->attackCooldownTimer -= dt;

    if (p->attackBuffered && p->attackCooldownTimer <= 0.0f)
    {
        p->isAttacking = true;
        p->attackProgress = 0.0f;
        p->attackCooldownTimer = ATTACK_COOLDOWN;
        p->attackQueued = true;
        p->attackBuffered = false;
    }

    if (p->isAttacking)
    {
        p->attackProgress += dt / ATTACK_DURATION;
        if (p->attackProgress >= 1.0f)
        {
            p->attackProgress = 1.0f;
            p->isAttacking = false;
        }
    }
}

void Server_Update(ServerState* s, float dt)
{
    if (!s || !s->running) return;

    for (;;)
    {
        uint32_t addr = 0;
        uint16_t port = 0;
        NetSocket client = Net_Accept(&s->listenSock, &addr, &port);
        if (!Net_IsValid(client))
            break;

        int slot = -1;
        for (int i = 0; i < NET_MAX_PLAYERS; ++i)
        {
            if (!s->clients[i].connected)
            {
                slot = i;
                break;
            }
        }

        if (slot < 0)
        {
            Net_Close(&client);
            continue;
        }

        ServerClient* c = &s->clients[slot];
        ResetClient(c);
        c->connected = true;
        c->id = (uint8_t)slot;
        c->sock = client;
        Net_SetNonBlocking(&c->sock, true);
        SendWelcome(c, c->id, s->seed);
    }

    uint8_t temp[512];
    for (int i = 0; i < NET_MAX_PLAYERS; ++i)
    {
        ServerClient* c = &s->clients[i];
        if (!c->connected || !Net_IsValid(c->sock))
            continue;

        for (;;)
        {
            int r = Net_Recv(&c->sock, temp, (int)sizeof(temp));
            if (r > 0)
            {
                if (!NetRecvBuffer_Push(&c->recv, temp, r))
                {
                    c->connected = false;
                    Net_Close(&c->sock);
                    break;
                }

                uint8_t type;
                uint8_t payload[NET_MAX_MESSAGE];
                int payloadLen;
                while (NetRecvBuffer_PopMessage(&c->recv, &type, payload, &payloadLen))
                {
                    HandleClientMessage(s, c, type, payload, payloadLen);
                }
            }
            else if (r == 0)
            {
                c->connected = false;
                Net_Close(&c->sock);
                break;
            }
            else
            {
                if (!Net_WouldBlock())
                {
                    c->connected = false;
                    Net_Close(&c->sock);
                }
                break;
            }
        }
    }

    s->cycleTimer += dt;
    if (!s->isNight && s->cycleTimer >= DAY_DURATION)
    {
        s->isNight = true;
        s->cycleTimer = 0.0f;
    }
    else if (s->isNight && s->cycleTimer >= NIGHT_DURATION)
    {
        s->isNight = false;
        s->cycleTimer = 0.0f;
    }

    for (int i = 0; i < NET_MAX_PLAYERS; ++i)
    {
        ServerClient* c = &s->clients[i];
        if (!c->connected)
            continue;
        if (!c->isDead && c->hp <= 0.0f)
        {
            c->hp = 0.0f;
            c->isDead = true;
            c->respawnTimer = PLAYER_RESPAWN_TIME;
            c->isAttacking = false;
            c->attackBuffered = false;
            c->attackQueued = false;
            c->attackProgress = 0.0f;
            c->attackCooldownTimer = 0.0f;
        }

        if (c->isDead)
        {
            c->respawnTimer -= dt;
            if (c->respawnTimer <= 0.0f)
            {
                c->isDead = false;
                c->respawnTimer = 0.0f;
                c->hp = PLAYER_MAX_HP;
                c->x = 0.0f;
                c->y = 0.0f;
                c->isAttacking = false;
                c->attackBuffered = false;
                c->attackQueued = false;
                c->attackProgress = 0.0f;
                c->attackCooldownTimer = 0.0f;
            }
            continue;
        }

        UpdatePlayer(s, c, dt);
    }

    s->snapshotTimer += dt;
    if (s->snapshotTimer >= 0.033f)
    {
        s->snapshotTimer = 0.0f;
        for (int i = 0; i < NET_MAX_PLAYERS; ++i)
        {
            ServerClient* c = &s->clients[i];
            if (!c->connected || !Net_IsValid(c->sock))
                continue;
            SendSnapshot(s, c);
        }
    }

    for (int i = 0; i < NET_MAX_PLAYERS; ++i)
    {
        ServerClient* c = &s->clients[i];
        if (c->connected && Net_IsValid(c->sock))
            NetSendBuffer_Flush(&c->send, &c->sock);
    }
}

int Server_GetSnapshot(ServerState* s, NetPlayerState* outPlayers, int maxPlayers, bool* outIsNight, float* outCycleTimer)
{
    if (!s || !outPlayers || maxPlayers <= 0) return 0;

    int count = 0;
    for (int i = 0; i < NET_MAX_PLAYERS && count < maxPlayers; ++i)
    {
        ServerClient* p = &s->clients[i];
        if (!p->connected) continue;
        NetPlayerState* out = &outPlayers[count++];
        out->id = p->id;
        out->x = p->x;
        out->y = p->y;
        out->hp = p->hp;
        out->isDead = p->isDead ? 1 : 0;
        out->respawnTimer = p->respawnTimer;
        out->isAttacking = p->isAttacking ? 1 : 0;
        out->attackProgress = p->attackProgress;
        out->attackDirX = p->attackDirX;
        out->attackDirY = p->attackDirY;
        out->attackBaseAngle = p->attackBaseAngle;
        out->lastInputSeq = p->lastInputSeq;
    }

    if (outIsNight) *outIsNight = s->isNight;
    if (outCycleTimer) *outCycleTimer = s->cycleTimer;
    return count;
}

void Server_Broadcast(ServerState* s, const uint8_t* data, int len)
{
    if (!s || !data || len <= 0) return;
    for (int i = 1; i < NET_MAX_PLAYERS; ++i)
    {
        ServerClient* c = &s->clients[i];
        if (!c->connected || !Net_IsValid(c->sock))
            continue;
        NetSendBuffer_Append(&c->send, data, len);
    }
}
